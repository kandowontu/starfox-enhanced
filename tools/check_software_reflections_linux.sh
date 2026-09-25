#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root with an existing graphical display (WSLg works).
# Uses SDL's CPU renderer, not a software Vulkan adapter. No DXR required.
binary=${1:?Supply the Linux starfox_pc executable}
proof=${2:?Supply a new evidence directory}
effect=${3:-28}
case "$effect" in 27|28|29|30) ;; *) echo 'Expected Metallic, Mirror, Gold or Copper ID (27-30)' >&2; exit 2;; esac
for key in ${!STARFOX_@}; do unset "$key"; done
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
    for quality in 0 1 3; do
        prefix="$proof/$experience-$quality"
        env SDL_AUDIODRIVER=dummy STARFOX_TEST_HIDDEN=1 STARFOX_TEST_FRAMES=12 \
            STARFOX_TEST_EXPERIENCE="$experience" STARFOX_TEST_RENDERER=SOFTWARE \
            STARFOX_TEST_SKIP_PREROLL=1 STARFOX_TEST_PREROLL_TICKS=1000 \
            STARFOX_TEST_UNPACED=1 STARFOX_TEST_VSYNC=0 STARFOX_TEST_MSU1=0 \
            STARFOX_TEST_DISPLAY_MODE=16_9 STARFOX_TEST_RENDER_SCALE=2 \
            STARFOX_TEST_STEREO_OUTPUT=0 STARFOX_TEST_LANGUAGE=0 \
            STARFOX_TEST_DLSS_SELECTION=0 STARFOX_TEST_RAY_TRACING=0 \
            STARFOX_TEST_SOFTWARE_SHADOWS=0 STARFOX_TEST_REFLECTIVE_SURFACES="$quality" \
            STARFOX_TEST_RTX_LIGHTING=0 STARFOX_TEST_2D_FILTER=0 \
            STARFOX_TEST_BLOOM=0 STARFOX_TEST_BLOOM_2D=0 \
            STARFOX_TEST_EFFECT="$effect" STARFOX_TEST_WORLD_EFFECT=0 \
            STARFOX_TEST_HDR_EFFECT=0 STARFOX_TEST_CHROMATIC_ABERRATION=0 \
            STARFOX_TEST_MODEL_SMOOTHING=0 STARFOX_TEST_ANTI_ALIASING=0 \
            STARFOX_TEST_PRESENTATION_FPS=60 STARFOX_TEST_TIMING_MODE=ORIGINAL \
            STARFOX_TRACE_GPU=1 STARFOX_TRACE_PROFILE=1 \
            STARFOX_CAPTURE_PRESENTATION_PATH="$prefix.bmp" \
            "$binary" "$cartridge" "$symbols" LEVEL1_1 >"$prefix.out" 2>"$prefix.log"
        test -s "$prefix.bmp"
        if [[ $quality != 0 ]]; then
            grep -q "reflection-scene: CPU single bounce, quality=$quality" "$prefix.log"
            if cmp -s "$proof/$experience-0.bmp" "$prefix.bmp"; then
                echo "Reflection toggle did not change $experience quality $quality" >&2
                exit 1
            fi
        elif grep -q 'reflection-scene:' "$prefix.log"; then
            echo "Reflections ran while Off: $prefix.log" >&2
            exit 1
        fi
        if grep -q 'reflection-scene: GPU' "$prefix.log"; then
            echo "Software selection used GPU reflections: $prefix.log" >&2
            exit 1
        fi
    done
    echo "$experience: native Linux software effect=$effect OFF/LOW/HIGH captures passed"
done
