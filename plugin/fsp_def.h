#ifndef FSP_DEF_H
#define FSP_DEF_H

#include <windows.h>

#include "../common/fsp_types.h"

#ifdef _WIN64
#define Plug_Arch "x64"
#else
#define Plug_Arch "x32"
#endif

typedef struct {
    i32 X;
    i32 Y;
    i32 W;
    i32 H;
} TBox, *PBox;

typedef struct {
    i32 X;
    i32 Y;
} TSpot, *PSpot;

class TSoundfontPlug;

#endif
