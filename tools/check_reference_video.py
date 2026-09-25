"""Bounded, muted libretro video smoke test for an independent reference core.

Does not patch RAM or load saves. Optional stage selection and the explicit
custom-rumble-wait bypass change only the in-memory ROM, never the input file.
Such runs are labelled diagnostics, not unmodified-ROM parity. Keeps every
ctypes callback alive until the core is unloaded and rejects blank captures.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
from PIL import Image


BUTTONS = {"b": 0, "select": 2, "start": 3, "up": 4, "down": 5,
           "left": 6, "right": 7, "a": 8, "x": 9, "l": 10, "r": 11, "y": 1}


def parse_press(value):
    """One-based inclusive frame range, e.g. start:600:602."""
    try:
        button, first, last = value.lower().split(":")
        first, last = int(first), int(last)
        if button not in BUTTONS or not 1 <= first <= last <= 10000:
            raise ValueError()
        return BUTTONS[button], first, last
    except ValueError as error:
        raise argparse.ArgumentTypeError("press must be BUTTON:FIRST:LAST (frames 1..10000)") from error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("core", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--dll-directory", type=Path, action="append", default=[])
    parser.add_argument("--press", type=parse_press, action="append", default=[],
                        help="Port 1 joypad button held over an inclusive frame range; repeatable")
    parser.add_argument("--symbols", type=Path)
    parser.add_argument("--first-stage", help="Replace Original route-1 first map in memory (requires symbols)")
    parser.add_argument("--skip-rumble-wait", action="store_true",
                        help="Diagnostic only: bypass verified custom ROM rumble wait in memory (requires symbols)")
    parser.add_argument("--watch", type=lambda value: int(value, 0), action="append", default=[],
                        help="Read a WRAM byte offset after execution (read-only diagnostic)")
    parser.add_argument("--observe-cpu", action="store_true",
                        help="Read CPU registers using the optional local observer bridge")
    parser.add_argument("--observe-bg2", action="store_true",
                        help="Read BG2 registers using the optional read-only local observer bridge")
    parser.add_argument("--background-only", action="store_true",
                        help="Diagnostic: use core layer visibility to show BG2 only; no ROM/RAM changes")
    parser.add_argument("--expect-blank", action="store_true",
                        help="Require an entirely black diagnostic frame (EX background 99)")
    args = parser.parse_args()
    if not 1 <= args.frames <= 10000:
        parser.error("frames must be in 1..10000")
    if args.dll_directory and os.name != "nt":
        parser.error("dll-directory is Windows-only")
    if (args.first_stage or args.skip_rumble_wait) and not args.symbols:
        parser.error("ROM diagnostics require symbols")
    if any(not 0 <= offset < 0x20000 for offset in args.watch):
        parser.error("watch offsets must lie within 128 KiB WRAM")
    # Local process search scope only; do not change the user's PATH.
    dll_scopes = [os.add_dll_directory(str(path.resolve())) for path in args.dll_directory]
    core = C.CDLL(str(args.core.resolve()))
    observer = None
    if args.observe_cpu:
        observer = getattr(core, "retro_debug_cpu_word", None)
        if observer is None:
            raise RuntimeError("Reference core does not include the read-only CPU observer")
        observer.argtypes = [C.c_uint]
        observer.restype = C.c_uint32
    cgram_observer = None
    if args.observe_bg2:
        cgram_observer = getattr(core, "retro_debug_cgram_word", None)
        if cgram_observer is not None:
            cgram_observer.argtypes = [C.c_uint]
            cgram_observer.restype = C.c_uint32
    pixel_format = 0
    bg2_observer = None
    if args.observe_bg2:
        bg2_observer = getattr(core, "retro_debug_bg2_word", None)
        if bg2_observer is None:
            raise RuntimeError("Reference core does not include the read-only BG2 observer")
        bg2_observer.argtypes = [C.c_uint]
        bg2_observer.restype = C.c_uint32
    captured = None
    video_frames = 0
    current_frame = 0
    environment_type = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
    video_type = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
    sample_type = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
    batch_type = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
    poll_type = C.CFUNCTYPE(None)
    input_type = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
    class Variable(C.Structure):
        _fields_ = [("key", C.c_char_p), ("value", C.c_char_p)]

    defaults = {}

    @environment_type
    def environment(command, data):
        nonlocal pixel_format
        if command == 10:  # RETRO_ENVIRONMENT_SET_PIXEL_FORMAT
            requested = C.cast(data, C.POINTER(C.c_uint))[0]
            if requested not in (0, 1, 2):
                return False
            pixel_format = requested
            return True
        if command == 3:  # RETRO_ENVIRONMENT_GET_CAN_DUPE
            C.cast(data, C.POINTER(C.c_bool))[0] = True
            return True
        if command == 52:  # Use legacy options, whose first value is the default.
            C.cast(data, C.POINTER(C.c_uint))[0] = 0
            return True
        if command == 16:  # RETRO_ENVIRONMENT_SET_VARIABLES
            variables = C.cast(data, C.POINTER(Variable))
            for index in range(4096):
                variable = variables[index]
                if not variable.key:
                    return True
                description = variable.value or b""
                _, separator, choices = description.partition(b"; ")
                if separator:
                    defaults[variable.key] = choices.split(b"|")[0]
            return False
        if command == 15:  # RETRO_ENVIRONMENT_GET_VARIABLE
            variable = C.cast(data, C.POINTER(Variable))
            if args.background_only and variable[0].key in (
                    b"snes9x_layer_1", b"snes9x_layer_3", b"snes9x_layer_4", b"snes9x_layer_5"):
                variable[0].value = b"disabled"
                return True
            if variable[0].key in defaults:
                variable[0].value = defaults[variable[0].key]
                return True
            return False
        if command == 17:  # No runtime option changes.
            C.cast(data, C.POINTER(C.c_bool))[0] = False
            return True
        return False

    @video_type
    def video(data, width, height, pitch):
        nonlocal captured, video_frames
        if not data or data == C.c_void_p(-1).value:
            return
        video_frames += 1
        if width > 4096 or height > 4096 or pitch > 65536:
            return
        captured = (width, height, pitch, pixel_format, C.string_at(data, pitch * height))

    def input_state(port, device, index, button):
        if port != 0 or device != 1 or index != 0:
            return 0
        return int(any(button == key and first <= current_frame <= last
                       for key, first, last in args.press))

    callbacks = [environment, video, sample_type(lambda left, right: None),
                 batch_type(lambda data, frames: frames), poll_type(lambda: None),
                 input_type(input_state)]
    names = ["environment", "video_refresh", "audio_sample", "audio_sample_batch",
             "input_poll", "input_state"]
    for name, callback in zip(names, callbacks):
        function = getattr(core, "retro_set_" + name)
        function.argtypes = [type(callback)]
        function.restype = None
        function(callback)

    class GameInfo(C.Structure):
        _fields_ = [("path", C.c_char_p), ("data", C.c_void_p),
                    ("size", C.c_size_t), ("meta", C.c_char_p)]

    rom = args.rom.read_bytes()
    if args.first_stage or args.skip_rumble_wait:
        symbols = {}
        for line in args.symbols.read_text().splitlines():
            fields = line.split()
            if len(fields) == 2 and fields[1].startswith("$"):
                symbols[fields[0]] = int(fields[1][1:], 16)
    if args.first_stage:
        def map_pointer(address):
            return (address & 0x7fff).to_bytes(2, "little") + bytes([address >> 16])
        path = symbols["STAGEPATHS.PATH6"]
        offset = (path >> 16) * 0x8000 + (path & 0x7fff)
        expected = bytes([3, 6, 21, 0]) + map_pointer(symbols["LEVEL1_1"])
        if rom[offset:offset + 7] != expected:
            raise RuntimeError("Original route-1 PATHSTART signature mismatch; refusing stage override")
        target = symbols[args.first_stage.upper()]
        rom = rom[:offset + 4] + map_pointer(target) + rom[offset + 7:]
        print(f"Diagnostic in-memory route-1 override: {args.first_stage}; input ROM unchanged")
    if args.skip_rumble_wait:
        address = symbols["PLANETSEQ_L"] + 8
        offset = (address >> 16) * 0x8000 + (address & 0x7fff)
        if symbols["RUMBLE_TIME"] != 0xee or rom[offset:offset + 4] != bytes.fromhex("a5 ee d0 fc"):
            raise RuntimeError("Custom rumble wait signature mismatch; refusing diagnostic substitution")
        # LDA #0 instead of LDA RUMBLE_TIME: the following BNE falls through.
        # Neither VRAM/PPU code nor RAM is patched. This is NOT an unmodified-ROM run.
        rom = rom[:offset] + bytes.fromhex("a9 00") + rom[offset + 2:]
        print("Diagnostic custom rumble wait bypass: in-memory ROM only; not unmodified-ROM parity")
    storage = C.create_string_buffer(rom)
    info = GameInfo(str(args.rom.resolve()).encode(), C.cast(storage, C.c_void_p), len(rom), None)
    core.retro_load_game.argtypes = [C.POINTER(GameInfo)]
    core.retro_load_game.restype = C.c_bool
    for name in ("retro_init", "retro_run", "retro_unload_game", "retro_deinit"):
        getattr(core, name).argtypes = []
        getattr(core, name).restype = None
    core.retro_init()
    loaded = False
    try:
        loaded = core.retro_load_game(C.byref(info))
        if not loaded:
            raise RuntimeError("Reference core rejected ROM")
        core.retro_set_controller_port_device.argtypes = [C.c_uint, C.c_uint]
        core.retro_set_controller_port_device.restype = None
        core.retro_set_controller_port_device(0, 1)  # RETRO_DEVICE_JOYPAD
        print(f"Reference defaults: {len(defaults)} options; Super FX clock "
              + defaults.get(b"snes9x_overclock_superfx", b"unavailable").decode())
        if args.background_only:
            print("Reference diagnostic: only BG2 visible (core layer settings)")
        for current_frame in range(1, args.frames + 1):
            core.retro_run()
        if observer is not None:
            names = ("PC", "A", "X", "Y", "P", "S", "NMI", "IRQ", "WAI", "V", "flags")
            print("Reference CPU: " + " ".join(f"{name}={observer(index):06x}"
                                               for index, name in enumerate(names)))
        if cgram_observer is not None:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.with_suffix(".cgram").write_bytes(b"".join(
                int(cgram_observer(index)).to_bytes(2, "little") for index in range(256)))
        if args.watch:
            core.retro_get_memory_size.argtypes = [C.c_uint]
            core.retro_get_memory_size.restype = C.c_size_t
            core.retro_get_memory_data.argtypes = [C.c_uint]
            core.retro_get_memory_data.restype = C.c_void_p
            size = core.retro_get_memory_size(2)  # RETRO_MEMORY_SYSTEM_RAM
            pointer = core.retro_get_memory_data(2)
            if not pointer or size != 0x20000:
                raise RuntimeError("Reference does not expose expected SNES WRAM")
            ram = C.string_at(pointer, size)
            print("WRAM: " + " ".join(f"{offset:05x}={ram[offset]:02x}" for offset in args.watch))
        if bg2_observer is not None:
            names = ("mode", "x", "y", "map", "size", "characters", "tile16",
                     "offset_x", "offset_y", "offset_map", "vertical_word", "horizontal_word")
            print("Reference BG2: " + " ".join(f"{name}={bg2_observer(index)}"
                                               for index, name in enumerate(names)))
        if captured is None:
            raise RuntimeError("Reference core emitted no software video")
        width, height, pitch, fmt, raw = captured
        rgb = bytearray()
        for y in range(height):
            for x in range(width):
                stride = 4 if fmt == 1 else 2
                at = y * pitch + x * stride
                value = int.from_bytes(raw[at:at + stride], "little")
                if fmt == 1:
                    rgb.extend(((value >> 16) & 255, (value >> 8) & 255, value & 255))
                else:
                    green_bits = 6 if fmt == 2 else 5
                    rgb.extend((((value >> (green_bits + 5)) & 31) * 255 // 31,
                                ((value >> 5) & ((1 << green_bits) - 1)) * 255 // ((1 << green_bits) - 1),
                                (value & 31) * 255 // 31))
        if args.expect_blank and any(rgb):
            raise RuntimeError("Expected an entirely black reference frame")
        if not any(rgb) and not args.expect_blank:
            raise RuntimeError("Reference emitted an entirely black final frame")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        Image.frombytes("RGB", (width, height), bytes(rgb)).save(args.output)
        print(f"Reference video: {video_frames} callbacks, {width}x{height}, format {fmt}; "
              f"{'expected blank' if args.expect_blank else 'nonblank'} capture {args.output}")
        if b"snes9x_overclock_superfx" in defaults:
            print("Reference Super FX clock: " + defaults[b"snes9x_overclock_superfx"].decode())
    finally:
        if loaded:
            core.retro_unload_game()
        core.retro_deinit()
        for scope in dll_scopes:
            scope.close()


if __name__ == "__main__":
    main()
