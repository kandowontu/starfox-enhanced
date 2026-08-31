#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"
executable="${1:-$project_root/build/release/starfox_pc}"
sample_seconds="${2:-0.6}"
minimum_ratio="${3:-0.70}"

if [[ ! -x $executable ]]; then
    echo "Runtime executable not found at $executable" >&2
    exit 1
fi

rates=(20 30 60 90 120 240 360 480)
displays=(0 1 2 3 4)
paces=(UNLOCKED ORIGINAL)

printf '%-8s %-6s %-9s %-9s %-7s %s\n' \
    DISPLAY FPS PACE MEASURED RATIO RESULT
failures=0
combinations=0

for display in "${displays[@]}"; do
    for rate in "${rates[@]}"; do
        for pace in "${paces[@]}"; do
            frames="$(awk -v rate="$rate" -v sample="$sample_seconds" \
                'BEGIN { frames = int(rate * sample); \
                    if (frames < rate * sample) frames += 1; \
                    if (frames < 12) frames = 12; print frames }')"
            set +e
            error_text="$(
                STARFOX_TEST_FRAMES="$frames" \
                STARFOX_TEST_SKIP_PREROLL=1 \
                STARFOX_TEST_PRESENTATION_FPS="$rate" \
                STARFOX_TEST_DISPLAY_MODE="$display" \
                STARFOX_TEST_TIMING_MODE="$pace" \
                STARFOX_TRACE_FPS=1 \
                "$executable" 2>&1 >/dev/null
            )"
            status=$?
            set -e
            if [[ $status -ne 0 ]]; then
                echo "Matrix run failed for display=$display fps=$rate pace=$pace" >&2
                echo "$error_text" >&2
                exit 1
            fi
            measured="$(sed -n 's/.*measured=\([0-9][0-9]*\).*/\1/p' \
                <<<"$error_text" | tail -n 1)"
            if [[ -z $measured ]]; then
                echo "Matrix run did not report FPS for display=$display fps=$rate pace=$pace" >&2
                echo "$error_text" >&2
                exit 1
            fi
            ratio="$(awk -v measured="$measured" -v rate="$rate" \
                'BEGIN { printf "%.3f", measured / rate }')"
            if awk -v ratio="$ratio" -v measured="$measured" \
                -v minimum="$minimum_ratio" \
                'BEGIN { exit !(measured > 0 && ratio >= minimum) }'; then
                result=PASS
            else
                result=FAIL
                failures=$((failures + 1))
            fi
            combinations=$((combinations + 1))
            printf '%-8s %-6s %-9s %-9s %-7s %s\n' \
                "$display" "$rate" "$pace" "$measured" "$ratio" "$result"
        done
    done
done

if [[ $failures -ne 0 ]]; then
    echo "$failures runtime timing combinations were stuck or unexpectedly low" >&2
    exit 1
fi
echo "Validated $combinations resolution/FPS/pace combinations."
