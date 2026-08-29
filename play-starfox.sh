#!/usr/bin/env bash
set -euo pipefail

map="${1:-BOOT}"
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
rom_path="$project_root/upstream-ultrastarfox/SF.SFC"
symbols_path="$project_root/upstream-ultrastarfox/SYMBOLS.TXT"
ex_rom_path="$project_root/upstream-star-fox-ex/SFES/SFES.SFC"
ex_symbols_path="$project_root/upstream-star-fox-ex/SYMBOLS.TXT"
build_path="$project_root/build/release"
executable_path="$build_path/starfox_pc"

for required in "$rom_path" "$symbols_path" "$ex_rom_path" "$ex_symbols_path"; do
    if [[ ! -f $required ]]; then
        echo "Missing Original or EX build output. Run tools/build_upstream.sh and tools/build_starfox_ex.sh first." >&2
        exit 1
    fi
done

if [[ ! -x $executable_path ]]; then
    cmake -S "$project_root" -B "$build_path" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build_path" --target starfox_pc -j 8
fi

exec "$executable_path" "$rom_path" "$symbols_path" "$map"
