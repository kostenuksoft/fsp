#ifndef FSP_EXPLORER_H
#define FSP_EXPLORER_H

#include "fsp_def.h"

const c8 Explorer_ClassName[] = "FruitySoundfontPlayerX64Patches";

void Explorer_Startup(HINSTANCE Instance);
void Explorer_Shutdown(HINSTANCE Instance);
void Explorer_Open(TSoundfontPlug *Plug);
void Explorer_OwnerDestroyed(TSoundfontPlug *Plug);

#endif
