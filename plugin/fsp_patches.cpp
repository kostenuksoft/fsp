#include "fsp_patches.h"

#include "../engine/flspro.h"

static TPatchList Patch_List;

PPatchList Patches_Scan(i32 SynthInstance)
{
    Patch_List.Count = 0;
    for (i32 Bank = 0; Bank < Midi_BankCount; ++Bank) {
        for (i32 Patch = 0; Patch < Midi_ProgramCount; ++Patch) {
            PPatchEntry Entry = &Patch_List.Entries[Patch_List.Count];

            fls_patch_name(SynthInstance, Patch, Bank, Entry->Name, sizeof(Entry->Name));
            if (Entry->Name[0] == 0) {
                continue;
            }
            Entry->Bank = Bank;
            Entry->Patch = Patch;
            ++Patch_List.Count;
        }
    }
    return &Patch_List;
}

i32 Patches_ParamForBank(i32 Bank, i32 Patch)
{
    if (Bank == Midi_DrumBank) {
        return Midi_ProgramCount + Patch + 1;
    }
    return Patch + 1;
}
