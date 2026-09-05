// Audit-only entry point, appended to libretro.cpp by build-reference.ps1.
// Execute the cartridge's GSU code with a clean pipeline and bounded runtime.
static unsigned reference_run_gsu(unsigned address, unsigned stack, unsigned return_address)
{
    S9xResetSuperFX();
    SuperFX.speedPerLine = 1;
    S9xSetSuperFX(0x39, 0x303a); // both buses, 4bpp, 192 rows
    S9xSetSuperFX(0x10, 0x3038); // screen base $4000 (1 KiB units)
    S9xSetSuperFX(stack & 255, 0x3014);
    S9xSetSuperFX((stack >> 8) & 255, 0x3015);
    S9xSetSuperFX(return_address & 255, 0x3016);
    S9xSetSuperFX((return_address >> 8) & 255, 0x3017);
    S9xSetSuperFX(address >> 16, 0x3034);
    S9xSetSuperFX(address & 255, 0x301e);
    S9xSetSuperFX((address >> 8) & 255, 0x301f);
    unsigned steps = 0;
    while ((Memory.FillRAM[0x3030] & 0x20) && steps < 10000000) {
        S9xSuperFXExec();
        ++steps;
    }
    return steps;
}

extern "C" RETRO_API unsigned retro_reference_gsu(unsigned address, unsigned)
{
    return reference_run_gsu(address, 0, 0);
}

extern "C" RETRO_API unsigned retro_reference_gsu_call(
    unsigned address, unsigned stack, unsigned return_address)
{
    return reference_run_gsu(address, stack, return_address);
}
