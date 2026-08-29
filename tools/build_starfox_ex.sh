#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_root="$(cd -- "${1:-$script_dir/../upstream-star-fox-ex}" && pwd)"
expected_commit=b5e2d837a15a72a532cd019bfe332b7a4b660924

dosbox="$source_root/dosbox-x"
if [[ ! -x $dosbox ]]; then
    dosbox="$(command -v dosbox-x || true)"
fi
if [[ -z $dosbox ]]; then
    echo "Star Fox EX DOSBox toolchain not found in $source_root or on PATH" >&2
    exit 1
fi

actual_commit="$(git -C "$source_root" rev-parse HEAD)"
if [[ $actual_commit != "$expected_commit" ]]; then
    echo "Star Fox EX must be checked out at $expected_commit (found $actual_commit)" >&2
    exit 1
fi

batch_path="$source_root/.starfox-port-ex-build.bat"
success_path="$source_root/.starfox-port-ex-build.ok"
if [[ -e $batch_path ]]; then
    echo "Refusing to overwrite existing temporary build file: $batch_path" >&2
    exit 1
fi
trap 'rm -f "$batch_path" "$success_path"' EXIT

rm -f "$success_path"
printf '%s\r\n' \
    '@echo off' \
    'set path=%path%;c:\bin' \
    'set sasmheap=14400' \
    'cd sfes' \
    'make clean' \
    'if errorlevel 1 goto failed' \
    'make hardware=0 newface=1' \
    'if errorlevel 1 goto failed' \
    'copy sfes.sfc ..\sfes.sfc' \
    'if errorlevel 1 goto failed' \
    'cd ..' \
    'echo ok>.starfox-port-ex-build.ok' \
    'exit' \
    ':failed' \
    'cd ..' \
    'exit' > "$batch_path"

if ! (cd "$source_root" && "$dosbox" -fastlaunch "$(basename "$batch_path")") \
    || [[ ! -f $success_path ]]; then
    echo "Star Fox EX assembler build failed" >&2
    exit 1
fi

for name in SFES/SFES.SFC SYMBOLS.TXT BANKS.CSV; do
    target="$source_root/$name"
    if [[ ! -f $target ]]; then
        # DOSBox emits 8.3 names in its own case; Linux is case-sensitive.
        produced="$(find "$(dirname "$target")" -maxdepth 1 \
            -iname "$(basename "$name")" -print -quit 2>/dev/null)"
        if [[ -n $produced ]]; then
            mv -- "$produced" "$target"
        fi
    fi
    if [[ ! -f $target ]]; then
        echo "Star Fox EX build did not produce $name" >&2
        exit 1
    fi
    ls -l -- "$target"
done
