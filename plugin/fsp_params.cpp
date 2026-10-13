#include "fsp_params.h"

#include "fsp_midi.h"

extern const TParamDef Param_Defs[Param_Count] = {
    { 1, 2 * Midi_ProgramCount, 1, "Patch number", 0 },
    { 0, 127, 0, "Bank number", 0 },
    { 0, 256, 128, "Reverb send level", 0 },
    { 0, 256, 128, "Chorus send level", 0 },
    { 0, 127, 0, "Modulation", 0 },
    { -1, 5940, -1, "Env 2 attack time", 11 },
    { -1, 5940, -1, "Env 2 decay time", 13 },
    { -1, 127, -1, "Env 2 sustain level", 14 },
    { -1, 5940, -1, "Env 2 release time", 15 },
    { -1, 5900, -1, "LFO 2 predelay", 2 },
    { -128, 127, -128, "LFO 2 amount", 18 },
    { -1, 127, -1, "LFO 2 speed", 3 },
    { -1, 127, -1, "Initial filter cutoff", 21 },
};

bool Param_IsValid(i32 Index)
{
    return Index >= 0 && Index < Param_Count;
}

bool Param_CanBeOff(i32 Index)
{
    return Index >= Param_FirstNrpn;
}

i32 Param_Clamp(i32 Index, i32 Value)
{
    const TParamDef *Def = &Param_Defs[Index];

    if (Value < Def->Min) {
        return Def->Min;
    }
    if (Value > Def->Max) {
        return Def->Max;
    }
    return Value;
}

f64 Param_Range(i32 Index)
{
    return (f64)(Param_Defs[Index].Max - Param_Defs[Index].Min);
}
