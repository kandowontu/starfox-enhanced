"""Exercise the actual pinned/patched Switch callbacks with a mock AUDOUT device.

Usage: python tests/test_switch_audio_backend.py downloaded-sdl3-switch.patch c++
No Switch SDK or audio hardware is needed. The input patch is already hash-pinned
by CMake; this test also verifies that our follow-up patch applies to it.
"""
import hashlib
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def extract_files(patch, target):
    path = None
    lines = []
    for line in patch.splitlines() + ["diff --git end"]:
        if line.startswith("diff --git "):
            if path is not None:
                destination = target / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text("\n".join(lines) + "\n")
            path, lines = None, []
        elif line.startswith("+++ b/external/SDL3/src/audio/switch/"):
            path = line[len("+++ b/external/SDL3/"):]
        elif path is not None and line.startswith("+"):
            lines.append(line[1:])


def callback(source, name):
    start = source.index("static bool " + name)
    end = source.index("\n}\n", start) + 3
    return source[start:end]


with tempfile.TemporaryDirectory(prefix="starfox-switch-audio-") as directory:
    work = pathlib.Path(directory)
    patch = pathlib.Path(sys.argv[1]).read_bytes()
    if hashlib.sha256(patch).hexdigest() != "9f694599723d78b08c09d99f8203ddea52ee5178575a807263fe79f8ab1f28dc":
        raise RuntimeError("Unexpected Switch backend revision")
    extract_files(patch.decode(), work)
    subprocess.run(["git", "apply", "--unsafe-paths",
                    str(ROOT / "cmake/sdl3-switch-audio-pipeline.patch")],
                   cwd=work, check=True)
    source = (work / "src/audio/switch/SDL_switchaudio.c").read_text()
    harness = r'''
#include <cstdint>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <iostream>
using Uint8 = unsigned char;
using u32 = uint32_t;
using u64 = uint64_t;
#define NUM_AUDIO_BUFFERS 2
#define R_FAILED(x) ((x) != 0)
#define SDL_memcpy std::memcpy
struct AudioOutBuffer { void *buffer; u64 data_size; };
struct Private { AudioOutBuffer buffers[2]; int next = 0; bool pipeline_started = false; };
struct SDL_AudioDevice { Private *hidden; };
std::deque<AudioOutBuffer*> queued;
bool spurious = true;
bool fail_wait = false;
bool unknown_release = false;
bool SDL_SetError(const char*) { return false; }
int audoutAppendAudioOutBuffer(AudioOutBuffer *buffer) {
    for (auto *entry : queued) if (entry == buffer)
        throw std::runtime_error("reused hardware-owned buffer");
    queued.push_back(buffer);
    return 0;
}
int audoutWaitPlayFinish(AudioOutBuffer **released, u32 *count, u64) {
    if (fail_wait) return 1;
    if (queued.size() != 2)
        throw std::runtime_error("hardware queue drains between every chunk");
    if (spurious) { spurious = false; *count = 0; return 0; }
    *released = unknown_release ? nullptr : queued.front();
    queued.pop_front();
    *count = 1;
    return 0;
}
'''
    harness += callback(source, "SWITCHAUDIO_PlayDevice")
    harness += callback(source, "SWITCHAUDIO_WaitDevice")
    harness += r'''
int main() {
    Uint8 storage[2][32]{};
    Uint8 packet[32]{};
    Private state{};
    SDL_AudioDevice device{&state};
    for (int i = 0; i < 2; ++i) state.buffers[i].buffer = storage[i];
    for (int tick = 0; tick < 1000; ++tick) {
        for (auto &sample : packet) sample = Uint8(tick);
        if (!SWITCHAUDIO_PlayDevice(&device, packet, sizeof(packet))) return 1;
        if (!SWITCHAUDIO_WaitDevice(&device)) return 2;
        if (queued.size() != 1) return 3;
        if (std::memcmp(queued.front()->buffer, packet, sizeof(packet))) return 4;
    }
    fail_wait = true;
    if (SWITCHAUDIO_WaitDevice(&device)) return 5;
    fail_wait = false;
    if (!SWITCHAUDIO_PlayDevice(&device, packet, sizeof(packet))) return 6;
    unknown_release = true;
    if (SWITCHAUDIO_WaitDevice(&device)) return 7;
    std::cout << "Switch audio: 1000 continuous handoffs, ownership, spurious wake and errors passed\n";
}
'''
    (work / "test.cpp").write_text(harness)
    executable = work / "test.exe"
    subprocess.run([sys.argv[2], "-std=c++17", str(work / "test.cpp"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
