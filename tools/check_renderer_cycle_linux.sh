#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root. Keep captures separate from player settings.
# For Lavapipe-only CI, explicitly set STARFOX_TEST_SOFTWARE_GPU=1 and the
# desired VK_ICD_FILENAMES. This still exercises Vulkan, not SDL software.
binary=${1:?Supply the Linux starfox_pc executable}
proof=${2:?Supply a new evidence directory}
# Ignore unrelated diagnostics inherited from a previous capture, but preserve
# the caller's explicit permission to use a software Vulkan implementation.
for key in ${!STARFOX_@}; do
    if [[ $key != STARFOX_TEST_SOFTWARE_GPU ]]; then unset "$key"; fi
done
mkdir "$proof"
proof=$(realpath "$proof")
for experience in ORIGINAL EX; do
    if [[ $experience == ORIGINAL ]]; then
        cartridge=upstream-ultrastarfox/SF.SFC
        symbols=upstream-ultrastarfox/SYMBOLS.TXT
    else
        cartridge=tmp/runtime-inputs/starfox-ex/SFES.SFC
        symbols=assets/symbols/starfox-ex.txt
    fi
    env SDL_GPU_DRIVER=vulkan SDL_AUDIODRIVER=dummy \
        STARFOX_TEST_HIDDEN=1 STARFOX_TEST_FRAMES=40 STARFOX_TEST_RENDERER_CYCLE=1 \
        STARFOX_TEST_RENDERER=GPU STARFOX_TEST_EXPERIENCE="$experience" \
        STARFOX_TEST_SKIP_PREROLL=1 STARFOX_TEST_PREROLL_TICKS=1000 \
        STARFOX_TEST_UNPACED=1 STARFOX_TEST_VSYNC=0 STARFOX_TEST_MSU1=0 \
        STARFOX_TEST_DISPLAY_MODE=16_9 STARFOX_TEST_RENDER_SCALE=2 \
        STARFOX_TEST_STEREO_OUTPUT=0 STARFOX_TEST_LANGUAGE=0 \
        STARFOX_TEST_DLSS_SELECTION=0 STARFOX_TEST_FSR1_SELECTION=0 \
        STARFOX_TEST_NEURAL_SELECTION=0 STARFOX_TEST_ANTI_ALIASING=0 \
        STARFOX_TEST_RAY_TRACING=0 \
        STARFOX_TEST_SOFTWARE_SHADOWS=0 STARFOX_TEST_REFLECTIVE_SURFACES=0 \
        STARFOX_TEST_RTX_LIGHTING=0 STARFOX_TEST_2D_FILTER=0 \
        STARFOX_TEST_BLOOM=0 STARFOX_TEST_BLOOM_2D=0 \
        STARFOX_TEST_EFFECT=0 STARFOX_TEST_WORLD_EFFECT=0 \
        STARFOX_TEST_HDR_EFFECT=0 STARFOX_TEST_CHROMATIC_ABERRATION=0 \
        STARFOX_TEST_MODEL_SMOOTHING=0 \
        STARFOX_TEST_PRESENTATION_FPS=60 STARFOX_TEST_TIMING_MODE=ORIGINAL \
        STARFOX_TRACE_GPU=1 STARFOX_CAPTURE_PRESENTATION_PATH="$proof/$experience.bmp" \
        "$binary" "$cartridge" "$symbols" LEVEL1_1 >"$proof/$experience.out" 2>"$proof/$experience.log"
    test "$(grep -c '^renderer-cycle frame=' "$proof/$experience.log")" = 4
    grep -q '^renderer-cycle frame=32 mode=GPU$' "$proof/$experience.log"
    grep -q 'native-background: GPU resident ordered layers' "$proof/$experience.log"
    if grep -q 'SDL GPU unavailable:' "$proof/$experience.log"; then
        echo "GPU fallback occurred: $proof/$experience.log" >&2
        exit 1
    fi
    test -s "$proof/$experience.bmp"
    echo "$experience: four live renderer switches and final capture passed"
done
