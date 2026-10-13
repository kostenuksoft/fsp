#ifndef FSP_TOOLTIP_H
#define FSP_TOOLTIP_H

#include "fsp_def.h"

void Tooltip_Startup(HINSTANCE Instance);
void Tooltip_Shutdown(HINSTANCE Instance);
void Tooltip_Show(TSoundfontPlug *Plug, i32 Control, bool AtControl);
void Tooltip_Hide(TSoundfontPlug *Plug);
void Tooltip_Free(TSoundfontPlug *Plug);

#endif
