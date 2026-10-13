#include "fsp_voices.h"

#include <math.h>
#include <stdio.h>

#include "../engine/flspro.h"

#include "fsp_debuglog.h"
#include "fsp_midi.h"
#include "fsp_plugclass.h"

const u32 Voice_MaxCount = 4096;
const i32 Voice_ErrorCodes = 256;
const u32 Voice_TraceSize = 200;
const f80 Velocity_CurveBase = 11.0L;
const f80 Velocity_CurveGain = 10.0L;
const f80 Velocity_Scale = 100.0L;

enum {
    Voice_Free,
    Voice_Playing,
    Voice_Ended
};

typedef struct {
    TSoundfontPlug *Plug;
    intptr_t HostTag;
    TVoiceParams_Old *Params;
    i32 Note;
    u32 Levels;
    i32 State;
} TVoice, *PVoice;

static TVoice Voice_Pool[Voice_MaxCount];
static CRITICAL_SECTION Voice_Lock;

static u32 Voices_TagOf(const TVoice *Voice)
{
    return (u32)(Voice - Voice_Pool) + 1;
}

static PVoice Voices_Acquire(TSoundfontPlug *Plug)
{
    PVoice Found = NULL;

    EnterCriticalSection(&Voice_Lock);
    for (u32 Index = 0; Index < Voice_MaxCount && !Found; ++Index) {
        if (Voice_Pool[Index].State == Voice_Free) {
            Found = &Voice_Pool[Index];
        }
    }
    if (Found) {
        Found->Plug = Plug;
        Found->State = Voice_Playing;
        ++Plug->VoiceCount;
    }
    LeaveCriticalSection(&Voice_Lock);
    return Found;
}

static void Voices_Return(PVoice Voice)
{
    if (!Voice) {
        return;
    }
    EnterCriticalSection(&Voice_Lock);
    --Voice->Plug->VoiceCount;
    Voice->Plug = NULL;
    Voice->State = Voice_Free;
    LeaveCriticalSection(&Voice_Lock);
}

static PVoice Voices_FromHandle(const TSoundfontPlug *Plug, TVoiceHandle Handle)
{
    PVoice Voice = (PVoice)Handle;

    if (Voice < Voice_Pool || Voice >= Voice_Pool + Voice_MaxCount) {
        return NULL;
    }
    if (Voice->Plug != Plug || Voice->State == Voice_Free) {
        return NULL;
    }
    return Voice;
}

static PVoice Voices_Next(const TSoundfontPlug *Plug, PVoice After)
{
    PVoice Voice = After ? After + 1 : Voice_Pool;

    for (; Voice < Voice_Pool + Voice_MaxCount; ++Voice) {
        if (Voice->Plug == Plug && Voice->State != Voice_Free) {
            return Voice;
        }
    }
    return NULL;
}

extern "C" {

static void Voices_OnNoteEnd(u32 Tag)
{
    PVoice Voice;

    if (Tag == 0 || Tag > Voice_MaxCount) {
        return;
    }
    Voice = &Voice_Pool[Tag - 1];
    if (Voice->State != Voice_Playing) {
        return;
    }
    Voice->State = Voice_Ended;
    Voice->Plug->PlugHost->Voice_Kill(Voice->HostTag, FALSE);
}

}

void Voices_Startup()
{
    InitializeCriticalSection(&Voice_Lock);
    fls_set_note_end_callback(Voices_OnNoteEnd);
}

static i32 Voices_Velocity(const TVoiceParams_Old *Params)
{
    volatile f32 Gain = (f32)(1.0L / (f80)(f32)logl(Velocity_CurveBase));
    volatile f32 Curve = (f32)(logl((f80)Params->InitLevels.Vol * Velocity_CurveGain + 1.0L) * (f80)Gain);
    i32 Velocity = (i32)lrintl((f80)Curve * Velocity_Scale);

    return Velocity > Midi_MaxVelocity ? Midi_MaxVelocity : Velocity;
}

static void Voices_Trace(const TVoiceParams_Old *Params, i32 Key, i32 Velocity)
{
    c8 Line[Voice_TraceSize];

    if (!DebugLog_TraceEnabled()) {
        return;
    }
    snprintf(Line, sizeof(Line), "FSP " Plug_Arch ": trigger init pan=%d vol=%f pitch=%d final pitch=%d key=%d vel=%d",
             Params->InitLevels.Pan, (f64)Params->InitLevels.Vol, Params->InitLevels.Pitch, Params->FinalLevels.Pitch,
             Key, Velocity);
    OutputDebugStringA(Line);
}

static bool Voices_NoteFailed(i32 Note)
{
    return Note == 0 || (Note < 0 && Note >= -Voice_ErrorCodes);
}

static i32 Voices_StartNote(TSoundfontPlug *Plug, PVoice Voice, i32 Key, i32 Velocity)
{
    if (!Plug_HasSynth(Plug) || !Voice || !Voice->Levels) {
        return FLS_ERROR;
    }
    return fls_note_on(Plug->SynthInstance, Plug->Channel, Plug->ParamValues[Param_Bank], Plug->Program, Key, Velocity,
                       Voice->Levels, Voices_TagOf(Voice), Plug->Quality);
}

TVoiceHandle Voices_Trigger(TSoundfontPlug *Plug, PVoiceParams VoiceParams, intptr_t HostTag)
{
    TVoiceParams_Old *Params = (TVoiceParams_Old *)VoiceParams;
    i32 Velocity = Voices_Velocity(Params);
    i32 Key = Params->InitLevels.Pitch / Midi_CentsPerKey;
    PVoice Voice;
    u32 Levels;
    i32 Note;

    Voices_Trace(Params, Key, Velocity);
    Voice = Voices_Acquire(Plug);
    Levels = fls_levels_new((const FlsLevels *)&Params->FinalLevels);
    if (Voice) {
        Voice->HostTag = HostTag;
        Voice->Params = Params;
        Voice->Levels = Levels;
    }
    Note = Voices_StartNote(Plug, Voice, Key, Velocity);
    if (Voices_NoteFailed(Note)) {
        fls_levels_delete(Levels);
        Voices_Return(Voice);
        Plug->PlugHost->Voice_Kill(HostTag, FALSE);
        return Voice_NoHandle;
    }
    Voice->Note = Note;
    return (TVoiceHandle)Voice;
}

void Voices_Purge(TSoundfontPlug *Plug)
{
    PVoice Voice = NULL;

    if (Plug->VoiceCount == 0) {
        return;
    }
    while ((Voice = Voices_Next(Plug, Voice)) != NULL) {
        if (Voice->State == Voice_Ended) {
            fls_levels_delete(Voice->Levels);
            Voices_Return(Voice);
        }
    }
}

void Voices_Release(TSoundfontPlug *Plug, TVoiceHandle Handle)
{
    PVoice Voice = Voices_FromHandle(Plug, Handle);

    if (!Voice || Voice->State != Voice_Playing || !Plug_HasSynth(Plug)) {
        return;
    }
    fls_note_off(Plug->SynthInstance, Voice->Note, false);
    Voices_Purge(Plug);
}

void Voices_Kill(TSoundfontPlug *Plug, TVoiceHandle Handle)
{
    PVoice Voice = Voices_FromHandle(Plug, Handle);

    if (!Voice) {
        return;
    }
    if (Voice->State == Voice_Playing && Plug_HasSynth(Plug)) {
        fls_note_off(Plug->SynthInstance, Voice->Note, true);
    }
    if (Voice->State == Voice_Playing) {
        Voice->State = Voice_Ended;
    }
    Voices_Purge(Plug);
}

void Voices_UpdateLevels(TSoundfontPlug *Plug)
{
    PVoice Voice = NULL;

    if (Plug->VoiceCount == 0) {
        return;
    }
    while ((Voice = Voices_Next(Plug, Voice)) != NULL) {
        if (Voice->State == Voice_Playing) {
            fls_levels_set(Voice->Levels, (const FlsLevels *)&Voice->Params->FinalLevels);
        }
    }
}

void Voices_EndAll(TSoundfontPlug *Plug)
{
    PVoice Voice = NULL;

    while ((Voice = Voices_Next(Plug, Voice)) != NULL) {
        if (Voice->State == Voice_Playing) {
            Voice->State = Voice_Ended;
        }
    }
}

void Voices_FreeAll(TSoundfontPlug *Plug)
{
    PVoice Voice = NULL;

    while ((Voice = Voices_Next(Plug, Voice)) != NULL) {
        if (Voice->State == Voice_Playing) {
            fls_note_off(Plug->SynthInstance, Voice->Note, true);
        }
        fls_levels_delete(Voice->Levels);
        Voices_Return(Voice);
    }
}
