#ifndef FSP_PATCHES_H
#define FSP_PATCHES_H

#include "fsp_def.h"
#include "fsp_midi.h"

const u32 Patch_NameSize = 64;
const u32 Patch_MaxCount = Midi_BankCount * Midi_ProgramCount;

typedef struct {
    i32 Bank;
    i32 Patch;
    c8 Name[Patch_NameSize];
} TPatchEntry, *PPatchEntry;

typedef struct {
    TPatchEntry Entries[Patch_MaxCount];
    u32 Count;
} TPatchList, *PPatchList;

PPatchList Patches_Scan(i32 SynthInstance);
i32 Patches_ParamForBank(i32 Bank, i32 Patch);

#endif
