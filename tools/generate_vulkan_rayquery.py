"""Compile the Linux Vulkan ray-query shaders to checked-in SPIR-V headers."""

from pathlib import Path
import argparse
import struct
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--glslc", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    for name in ("shadow", "reflection"):
        source = root / f"src/render/shaders/vulkan_{name}_rayquery.comp"
        output = root / f"src/render/shaders/generated/vulkan_{name}_rayquery.hpp"
        binary = output.with_suffix(".spv")
        subprocess.run([args.glslc, "-fshader-stage=compute", "--target-env=vulkan1.2",
                        "-o", str(binary), str(source)], check=True)
        data = binary.read_bytes()
        if len(data) % 4:
            raise ValueError("SPIR-V byte count is not word aligned")
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        lines = ["#pragma once", "#include <array>", "#include <cstdint>",
                 "namespace starfox::render::shadows {",
                 f"inline constexpr std::array<std::uint32_t,{len(words)}> vulkan_{name}_rayquery_spirv{{{{"]
        for pos in range(0, len(words), 8):
            lines.append("    " + ", ".join(f"0x{word:08x}U" for word in words[pos:pos + 8])
                         + ("," if pos + 8 < len(words) else ""))
        lines.extend(["}};", "}", ""])
        output.write_text("\n".join(lines), encoding="utf-8")
        binary.unlink()


if __name__ == "__main__":
    main()
