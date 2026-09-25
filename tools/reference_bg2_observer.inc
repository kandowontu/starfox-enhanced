// Include in a local Snes9x libretro.cpp for read-only diagnostic captures.
// Never linked into or shipped with Star Fox Enhanced. No emulation changes.
#include "ppu.h"
#include "memmap.h"
#if defined(_WIN32)
#define STARFOX_REFERENCE_EXPORT __declspec(dllexport)
#else
#define STARFOX_REFERENCE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" STARFOX_REFERENCE_EXPORT unsigned retro_debug_bg2_word(unsigned field) {
    switch(field) {
    case 0:return PPU.BGMode;
    case 1:return PPU.BG[1].HOffset;
    case 2:return PPU.BG[1].VOffset;
    case 3:return PPU.BG[1].SCBase;
    case 4:return PPU.BG[1].SCSize;
    case 5:return PPU.BG[1].NameBase;
    case 6:return PPU.BG[1].BGSize;
    case 7:return PPU.BG[2].HOffset;
    case 8:return PPU.BG[2].VOffset;
    case 9:return PPU.BG[2].SCBase;
    case 10:return Memory.VRAM[0x5f40]|(Memory.VRAM[0x5f41]<<8);
    case 11:return Memory.VRAM[0x5f00]|(Memory.VRAM[0x5f01]<<8);
    default:return 0;
    }
}
extern "C" STARFOX_REFERENCE_EXPORT unsigned retro_debug_cgram_word(unsigned index) {
    return index<256 ? PPU.CGDATA[index] : 0;
}
#undef STARFOX_REFERENCE_EXPORT
