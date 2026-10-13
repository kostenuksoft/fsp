#ifndef FSP_PARAMS_H
#define FSP_PARAMS_H

#include "fsp_def.h"

enum {
    Param_Patch,
    Param_Bank,
    Param_ReverbSend,
    Param_ChorusSend,
    Param_Mod,
    Param_Attack,
    Param_Decay,
    Param_Sustain,
    Param_Release,
    Param_LfoDelay,
    Param_LfoDepth,
    Param_LfoSpeed,
    Param_Cutoff,
    Param_Count
};

const i32 Param_FirstSend = Param_ReverbSend;
const i32 Param_FirstEngine = Param_Mod;
const i32 Param_FirstNrpn = Param_Attack;

typedef struct {
    i32 Min;
    i32 Max;
    i32 Default;
    const c8 *Name;
    i32 Nrpn;
} TParamDef, *PParamDef;

extern const TParamDef Param_Defs[Param_Count];

bool Param_IsValid(i32 Index);
bool Param_CanBeOff(i32 Index);
i32 Param_Clamp(i32 Index, i32 Value);
f64 Param_Range(i32 Index);

#endif
