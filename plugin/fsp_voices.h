#ifndef FSP_VOICES_H
#define FSP_VOICES_H

#include "fsp_def.h"

#include "sdk/fp_plugclass.h"

const TVoiceHandle Voice_NoHandle = -1;

void Voices_Startup();
TVoiceHandle Voices_Trigger(TSoundfontPlug *Plug, PVoiceParams VoiceParams, intptr_t HostTag);
void Voices_Release(TSoundfontPlug *Plug, TVoiceHandle Handle);
void Voices_Kill(TSoundfontPlug *Plug, TVoiceHandle Handle);
void Voices_UpdateLevels(TSoundfontPlug *Plug);
void Voices_Purge(TSoundfontPlug *Plug);
void Voices_EndAll(TSoundfontPlug *Plug);
void Voices_FreeAll(TSoundfontPlug *Plug);

#endif
