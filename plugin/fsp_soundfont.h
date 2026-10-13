#ifndef FSP_SOUNDFONT_H
#define FSP_SOUNDFONT_H

#include "fsp_def.h"

const c8 Soundfont_Ext[] = ".sf2";
const c8 Soundbank_Ext[] = ".sbk";
const c8 Soundfont_NoName[] = "(none)";

bool Soundfont_IsLoadable(const c8 *FileName);
bool Soundfont_HasPatch(const TSoundfontPlug *Plug);
void Soundfont_Load(TSoundfontPlug *Plug, const c8 *FileName, bool FromUser);
void Soundfont_Reload(TSoundfontPlug *Plug);
void Soundfont_UpdatePatchName(TSoundfontPlug *Plug);

#endif
