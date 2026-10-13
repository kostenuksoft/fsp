#ifndef FSP_COMMANDS_H
#define FSP_COMMANDS_H

#include "fsp_def.h"

void Commands_SoundfontMenu(TSoundfontPlug *Plug);
void Commands_ParamMenu(TSoundfontPlug *Plug, i32 Param, POINT ScreenSpot);
void Commands_PatchMenu(TSoundfontPlug *Plug, POINT ScreenSpot);
void Commands_TypeValue(TSoundfontPlug *Plug, i32 Param);
void Commands_OpenDialog(TSoundfontPlug *Plug);
void Commands_EditSoundfont(TSoundfontPlug *Plug);

#endif
