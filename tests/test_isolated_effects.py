"""Exercise each layer-dependent presentation option without other filters."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    runtime, rom, symbols = map(lambda value: str(Path(value).resolve()), sys.argv[1:4])
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("STARFOX_TEST_", "STARFOX_CAPTURE_"))}
    env.update({
        "SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy",
        "STARFOX_TEST_FRAMES": "12", "STARFOX_TEST_PREROLL_TICKS": "1000",
        "STARFOX_TEST_PRESENTATION_FPS": "60", "STARFOX_TEST_UNPACED": "1",
        "STARFOX_TEST_RENDER_SCALE": "2", "STARFOX_TEST_BLOOM": "0",
        "STARFOX_TEST_EFFECT": "0", "STARFOX_TEST_WORLD_EFFECT": "0",
        "STARFOX_TEST_2D_FILTER": "0", "STARFOX_TEST_MODEL_SMOOTHING": "0",
        "STARFOX_TEST_RTX_LIGHTING": "0", "STARFOX_TEST_DISPLAY_MODE": "1",
        "STARFOX_TEST_EXPERIENCE": "ORIGINAL",
    })
    baseline = None
    with tempfile.TemporaryDirectory(prefix="sfe-isolated-effects-") as directory:
        for mode in ("off", "chromatic", "hdr", "shadows", "off-repeat"):
            env["STARFOX_TEST_CHROMATIC_ABERRATION"] = "3" if mode == "chromatic" else "0"
            env["STARFOX_TEST_HDR_EFFECT"] = "3" if mode == "hdr" else "0"
            env["STARFOX_TEST_ENHANCED_SHADOWS"] = "1" if mode == "shadows" else "0"
            capture = Path(directory) / (mode + ".bmp")
            env["STARFOX_CAPTURE_PATH"] = str(capture)
            subprocess.run([runtime, rom, symbols, "LEVEL1_1"], env=env,
                           check=True, timeout=60, capture_output=True)
            pixels = capture.read_bytes()
            if pixels[:2] != b"BM" or len(pixels) < 1000:
                raise AssertionError(f"Invalid {mode} capture")
            if mode == "off":
                baseline = pixels
            elif mode == "off-repeat":
                if pixels != baseline:
                    raise AssertionError("Baseline capture is nondeterministic")
            elif pixels == baseline:
                raise AssertionError(f"{mode} had no effect with other filters disabled")
            print(f"isolated {mode}: passed")

        env["STARFOX_TEST_MENU_PREVIEW"] = "1"
        env["STARFOX_TEST_PREROLL_TICKS"] = "0"
        menu_baseline = None
        for mode in ("visible", "peek", "restored"):
            if mode == "peek":
                env["STARFOX_TEST_MENU_PEEK"] = "1"
            else:
                env.pop("STARFOX_TEST_MENU_PEEK", None)
            capture = Path(directory) / ("menu-" + mode + ".bmp")
            env["STARFOX_CAPTURE_PATH"] = str(capture)
            subprocess.run([runtime, rom, symbols, "BOOT"], env=env,
                           check=True, timeout=60, capture_output=True)
            pixels = capture.read_bytes()
            if mode == "visible":
                menu_baseline = pixels
            elif mode == "peek" and pixels == menu_baseline:
                raise AssertionError("Menu peek did not remove the overlay")
            elif mode == "restored" and pixels != menu_baseline:
                raise AssertionError("Menu did not restore its original appearance")
            print(f"menu {mode}: passed")

        env.pop("STARFOX_TEST_MENU_PREVIEW", None)
        env["STARFOX_TEST_LANGUAGE"] = "4"
        clean_menu = None
        for mode in ("clean", "effects"):
            env["STARFOX_TEST_CHROMATIC_ABERRATION"] = "3" if mode == "effects" else "0"
            env["STARFOX_TEST_HDR_EFFECT"] = "3" if mode == "effects" else "0"
            capture = Path(directory) / ("localized-menu-" + mode + ".bmp")
            env["STARFOX_CAPTURE_PATH"] = str(capture)
            subprocess.run([runtime, rom, symbols, "BOOT"], env=env,
                           check=True, timeout=60, capture_output=True)
            pixels = capture.read_bytes()
            if mode == "clean":
                clean_menu = pixels
            elif pixels != clean_menu:
                raise AssertionError("Presentation effects changed the localized setup menu")
            print(f"localized menu {mode}: passed")


if __name__ == "__main__":
    main()
