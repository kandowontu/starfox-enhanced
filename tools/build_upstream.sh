#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_root="$(cd -- "${1:-$script_dir/../upstream-ultrastarfox}" && pwd)"
expected_commit=270e959a47d82240d9290a6c6630032c9ec53ff5

dosbox="$source_root/dosbox-x"
if [[ ! -x $dosbox ]]; then
    dosbox="$(command -v dosbox-x || true)"
fi
if [[ -z $dosbox ]]; then
    echo "UltraStarFox DOSBox toolchain not found in $source_root or on PATH" >&2
    exit 1
fi

actual_commit="$(git -C "$source_root" rev-parse HEAD)"
if [[ $actual_commit != "$expected_commit" ]]; then
    echo "UltraStarFox must be checked out at $expected_commit (found $actual_commit)" >&2
    exit 1
fi

native_patch="$script_dir/../config/ultrastarfox-native-runtime.patch"
if [[ ! -f $native_patch ]]; then
    echo "UltraStarFox native-runtime feature patch not found at $native_patch" >&2
    exit 1
fi

# The patch enables the MSU-1 driver the native runtime drives directly. It is
# applied to the pinned checkout rather than committed, so tolerate a source
# tree that is already patched from an earlier build.
check_native_patch() {
    git -C "$source_root" apply "$@" --check --ignore-space-change \
        --ignore-whitespace "$native_patch" >/dev/null 2>&1
}
if check_native_patch; then
    if ! git -C "$source_root" apply --ignore-space-change \
        --ignore-whitespace "$native_patch"; then
        echo "Could not apply the UltraStarFox native-runtime feature patch" >&2
        exit 1
    fi
    # The patch is stored with LF endings but the DOS sources are CRLF, and
    # the assembler rejects the lone-LF lines git apply leaves behind. Git for
    # Windows hides this via core.autocrlf; restore the CRLF endings here so
    # the patched checkout assembles the same way on Linux.
    while IFS= read -r patched_file; do
        [[ -n $patched_file ]] || continue
        target="$source_root/$patched_file"
        [[ -f $target ]] || continue
        if grep -qU $'\r' "$target"; then
            sed -i 's/\r*$/\r/' "$target"
        fi
    done < <(git -C "$source_root" diff --name-only)
elif ! check_native_patch --reverse; then
    echo "UltraStarFox source does not match the native-runtime feature patch" >&2
    exit 1
fi

batch_path="$source_root/.starfox-port-build.bat"
success_path="$source_root/.starfox-port-build.ok"
if [[ -e $batch_path ]]; then
    echo "Refusing to overwrite existing temporary build file: $batch_path" >&2
    exit 1
fi
trap 'rm -f "$batch_path" "$success_path"' EXIT

rm -f "$success_path"
printf '%s\r\n' \
    '@echo off' \
    'set path=%path%;c:\bin' \
    'cd sf' \
    'make' \
    'if errorlevel 1 goto failed' \
    'cd ..' \
    'echo ok>.starfox-port-build.ok' \
    'exit' \
    ':failed' \
    'cd ..' \
    'exit' > "$batch_path"

if ! (cd "$source_root" && "$dosbox" -fastlaunch "$(basename "$batch_path")") \
    || [[ ! -f $success_path ]]; then
    echo "UltraStarFox assembler build failed" >&2
    exit 1
fi

for name in SF.SFC SYMBOLS.TXT BANKS.CSV; do
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
        echo "UltraStarFox build did not produce $name" >&2
        exit 1
    fi
    ls -l -- "$target"
done
