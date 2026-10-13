#include "fsp_plugclass.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../engine/flspro.h"

#include "fsp_midi.h"
#include "fsp_soundfont.h"
#include "fsp_voices.h"

const c8 Plug_LongName[] = "Fruity Soundfont Player";
const c8 Plug_ShortName[] = "SF Player";
const i32 Plug_DefaultPoly = 32;
const i32 Plug_HostColor = 0x6f6957;
const i32 Plug_DefaultSampleRate = 44100;
const i32 Plug_DefaultSendTracks = 4;
const i32 Plug_VoiceLevelCount = 2;

const i32 Quality_Realtime = 0;
const i32 Quality_High = 0x80;
const i32 Quality_Best = 0x100;
const i32 Quality_PerInterpolation = 2;
const intptr_t Quality_InterpolationMask = 0xffff00;
const i32 Quality_InterpolationShift = 8;
const i32 Quality_LowInterpolation = 4;
const i32 Quality_MidInterpolation = 0x41;
const i32 Quality_HighInterpolation = 0x81;

const i32 State_Version = 2;
const i32 State_NoVersion = 0x7fffffff;
const u8 State_LongText = 0xff;
const c8 State_FirstPrintable = 0x20;

const f80 Param_MidiScale = 65536.0L;
const f80 Param_Percent = 100.0L;
const f32 Send_LevelStep = 0.0078125f;
const f80 Send_CurveBase = 19.0L;
const f80 Send_CurveSpan = 18.0L;
const f32 Send_UnityLevel = 1.0f;

const c8 Value_PlainFormat[] = "%d";
const c8 Value_PercentFormat[] = "%d%%";
const c8 Value_Disabled[] = "Disabled";
const c8 Value_OffFormat[] = "%s: off";
const c8 Value_SendFormat[] = "%s: %d%%  (%d)";
const c8 Value_DrumKitFormat[] = "%s: %d  (drum kit %d)";
const c8 Value_NamedFormat[] = "%s: %d";
const u32 Value_HintSize = 64;

TFruityPlugInfo Plug_Info = {
    CurrentSDKVersion,
    (char *)Plug_LongName,
    (char *)Plug_ShortName,
    FPF_Generator | FPF_GetNoteInput | FPF_NoWindow | FPF_CanSend | FPF_DemoVersion,
    Param_Count,
    Plug_DefaultPoly,
    0,
    0
};

bool Plug_HasSynth(const TSoundfontPlug *Plug)
{
    return Plug->SynthInstance >= 0;
}

bool Plug_IsDrumChannel(const TSoundfontPlug *Plug)
{
    return Plug->Channel == Midi_DrumChannel;
}

i32 Plug_CurrentBank(const TSoundfontPlug *Plug)
{
    return Plug_IsDrumChannel(Plug) ? Midi_DrumBank : Plug->ParamValues[Param_Bank];
}

void Plug_Hint(TSoundfontPlug *Plug, const c8 *Text)
{
    Plug->PlugHost->OnHint(Plug->HostTag, (char *)Text);
}

void Plug_MarkDirty(TSoundfontPlug *Plug)
{
    Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_SetDirty, 0, 0);
}

static f32 Plug_SendLevel(i32 Value)
{
    volatile f32 Position = (f32)((f80)Value * Send_LevelStep);
    volatile f32 Curve = (f32)(expl(logl(Send_CurveBase) * (f80)Position) - 1.0L);

    return (f32)((1.0L / Send_CurveSpan) * (f80)Curve);
}

i32 Plug_SendPercent(const TSoundfontPlug *Plug, i32 Which)
{
    return (i32)lrintl((f80)Plug->Sends[Which].Level * Param_Percent);
}

static void Plug_ApplySends(TSoundfontPlug *Plug)
{
    if (!Plug_HasSynth(Plug)) {
        return;
    }
    Plug->PlugHost->LockMix_Old();
    fls_set_use_reverb(Plug->SynthInstance, Plug->Sends[Send_Reverb].Enabled);
    fls_set_use_chorus(Plug->SynthInstance, Plug->Sends[Send_Chorus].Enabled);
    Plug->PlugHost->UnlockMix_Old();
}

static void Plug_SendMod(TSoundfontPlug *Plug)
{
    u32 Message = Midi_ControlChange + (u32)Plug->Channel + (Midi_ModWheel << Midi_Data1Shift) +
                  ((u32)Plug->ParamValues[Param_Mod] << Midi_Data2Shift);

    if (Plug_HasSynth(Plug)) {
        fls_short_message(Plug->SynthInstance, Message);
    }
}

static void Plug_InitSynth(TSoundfontPlug *Plug)
{
    if (fls_init((u32)Plug->SampleRate, &Plug->SynthInstance) != FLS_OK) {
        Plug->SynthInstance = Synth_NoInstance;
        return;
    }
    fls_set_attenuation(Plug->SynthInstance, 0);
    Plug_ApplySends(Plug);
}

static void Plug_FreeSynth(TSoundfontPlug *Plug)
{
    if (!Plug_HasSynth(Plug)) {
        return;
    }
    fls_clear_soundfont(Plug->SynthInstance);
    fls_free(Plug->SynthInstance);
    Plug->SynthInstance = Synth_NoInstance;
}

static void Plug_ReapplyParams(TSoundfontPlug *Plug, i32 First, i32 Flags)
{
    for (i32 Index = First; Index < Param_Count; ++Index) {
        Plug->ProcessParam(Index, Plug->ParamValues[Index], Flags);
    }
}

TSoundfontPlug::TSoundfontPlug(TPluginTag SetHostTag, TFruityPlugHost *SetPlugHost)
{
    HostTag = SetHostTag;
    PlugHost = SetPlugHost;
    Info = &Plug_Info;
    SynthInstance = Synth_NoInstance;
    Program = 0;
    Channel = Midi_MelodicChannel;
    SampleRate = Plug_DefaultSampleRate;
    Quality = Quality_Realtime;
    SendTrackCount = Plug_DefaultSendTracks;
    HighQuality = false;
    VoiceCount = 0;
    FileName[0] = 0;
    for (i32 Which = 0; Which < Send_Count; ++Which) {
        Sends[Which].Track = Send_NoTrack;
        Sends[Which].Level = Send_UnityLevel;
        Sends[Which].Enabled = false;
    }
    for (i32 Index = 0; Index < Param_Count; ++Index) {
        ParamValues[Index] = Param_Defs[Index].Default;
    }
    Text_Copy(BankName, sizeof(BankName), Soundfont_NoName);
    Text_Copy(PatchName, sizeof(PatchName), Soundfont_NoName);
    Editor_Init(this);
    Plug_InitSynth(this);
    Plug_ReapplyParams(this, 0, REC_UpdateValue | REC_UpdateControl);
    PlugHost->Dispatcher(HostTag, FHD_SetNewColor, 0, Plug_HostColor);
    PlugHost->Dispatcher(HostTag, FHD_SetThreadSafe, 0, 0);
}

TSoundfontPlug::~TSoundfontPlug()
{
    Editor_Free(this);
    Voices_FreeAll(this);
    Plug_FreeSynth(this);
}

void _stdcall TSoundfontPlug::DestroyObject()
{
    delete this;
}

static void Plug_SetProcessMode(TSoundfontPlug *Plug, intptr_t Mode)
{
    i32 Interpolation = (i32)((Mode & Quality_InterpolationMask) >> Quality_InterpolationShift);

    if (!(Mode & PM_IsRendering)) {
        Plug->Quality = Quality_Realtime;
        return;
    }
    if (Interpolation < Quality_LowInterpolation) {
        Plug->Quality = Quality_Realtime;
    } else if (Interpolation < Quality_MidInterpolation) {
        Plug->Quality = Interpolation * Quality_PerInterpolation;
    } else if (Interpolation < Quality_HighInterpolation) {
        Plug->Quality = Quality_High;
    } else {
        Plug->Quality = Quality_Best;
    }
    if (Plug->HighQuality && Plug->Quality < Quality_High) {
        Plug->Quality = Quality_High;
    }
}

static void Plug_SetSampleRate(TSoundfontPlug *Plug, i32 Rate)
{
    if (Rate == Plug->SampleRate || Rate <= 0) {
        return;
    }
    Plug->SampleRate = Rate;
    Voices_EndAll(Plug);
    Plug_FreeSynth(Plug);
    Plug_InitSynth(Plug);
    Soundfont_Reload(Plug);
    Plug_ReapplyParams(Plug, Param_FirstEngine, REC_UpdateValue);
}

static void Plug_SetSendTrackCount(TSoundfontPlug *Plug, i32 Count)
{
    Plug->SendTrackCount = Count;
    for (i32 Which = 0; Which < Send_Count; ++Which) {
        if (Plug->Sends[Which].Track >= Count) {
            Plug->Sends[Which].Track = Count - 1;
        }
    }
    Editor_Redraw(Plug);
}

intptr_t _stdcall TSoundfontPlug::Dispatcher(intptr_t ID, intptr_t Index, intptr_t Value)
{
    switch (ID) {
    case FPD_ShowEditor:
        if (Value == 0) {
            Editor_Hide(this);
        } else {
            Editor_Show(this, (HWND)Value);
        }
        return 0;
    case FPD_ProcessMode:
        Plug_SetProcessMode(this, Value);
        return 0;
    case FPD_SetSampleRate:
        Plug_SetSampleRate(this, (i32)Value);
        return 0;
    case FPD_UseVoiceLevels:
        return Index < Plug_VoiceLevelCount ? 1 : 0;
    case FPD_CollectFile:
        if (Index == 0 && !Text_IsEmpty(FileName)) {
            return (intptr_t)FileName;
        }
        return 0;
    case FPD_SetNumSends:
        Plug_SetSendTrackCount(this, (i32)Value);
        return 0;
    case FPD_LoadFile:
        if (Value && Soundfont_IsLoadable((const c8 *)Value)) {
            Soundfont_Load(this, (const c8 *)Value, true);
            return 1;
        }
        return 0;
    }
    return 0;
}

void _stdcall TSoundfontPlug::Idle_Public()
{
}

static void State_WriteText(IStream *Stream, const c8 *Text)
{
    u32 Length = (u32)strlen(Text);
    u8 Short = (u8)Length;

    if (Length >= State_LongText) {
        Stream->Write(&State_LongText, sizeof(State_LongText), NULL);
        Stream->Write(&Length, sizeof(Length), NULL);
    } else {
        Stream->Write(&Short, sizeof(Short), NULL);
    }
    Stream->Write(Text, Length, NULL);
}

static void State_ReadText(IStream *Stream, c8 *Text, u32 TextSize)
{
    LARGE_INTEGER Skip;
    u32 Length = 0;
    u32 Kept;
    u8 Short = 0;
    ULONG Got = 0;

    Stream->Read(&Short, sizeof(Short), &Got);
    if (Short == State_LongText) {
        Stream->Read(&Length, sizeof(Length), &Got);
    } else {
        Length = Short;
    }
    Kept = Length < TextSize ? Length : 0;
    if (Kept) {
        Stream->Read(Text, Kept, &Got);
    }
    Skip.QuadPart = Length - Kept;
    if (Skip.QuadPart) {
        Stream->Seek(Skip, STREAM_SEEK_CUR, NULL);
    }
    while (Kept > 0 && (u8)Text[Kept - 1] < State_FirstPrintable) {
        --Kept;
    }
    Text[Kept] = 0;
}

static void State_WriteFlag(IStream *Stream, bool Flag)
{
    u8 Stored = Flag ? 1 : 0;

    Stream->Write(&Stored, sizeof(Stored), NULL);
}

static bool State_ReadFlag(IStream *Stream, bool Missing)
{
    u8 Stored = 0;
    ULONG Got = 0;

    Stream->Read(&Stored, sizeof(Stored), &Got);
    return Got ? Stored != 0 : Missing;
}

static void State_Save(TSoundfontPlug *Plug, IStream *Stream)
{
    const c8 *Packed = Plug->FileName;
    i32 Version = State_Version;

    Stream->Write(&Version, sizeof(Version), NULL);
    Stream->Write(Plug->ParamValues, sizeof(Plug->ParamValues), NULL);
    if (!Text_IsEmpty(Plug->FileName)) {
        const c8 *FromHost = (const c8 *)Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_PackDataFile, 0,
                                                                    (intptr_t)Plug->FileName);

        if (FromHost) {
            Packed = FromHost;
        }
    }
    State_WriteText(Stream, Packed);
    for (i32 Which = 0; Which < Send_Count; ++Which) {
        i32 Track = Plug->Sends[Which].Track;

        Stream->Write(&Track, sizeof(Track), NULL);
        State_WriteFlag(Stream, Plug->Sends[Which].Enabled);
    }
    State_WriteFlag(Stream, Plug->HighQuality);
    State_WriteFlag(Stream, Plug->Editor.InfoOpen);
}

static void State_Restore(TSoundfontPlug *Plug, IStream *Stream)
{
    c8 Name[Text_PathSize];
    i32 Version = State_NoVersion;
    ULONG Got = 0;

    Stream->Read(&Version, sizeof(Version), &Got);
    if (Version == State_NoVersion || Version < State_Version) {
        return;
    }
    Stream->Read(Plug->ParamValues, sizeof(Plug->ParamValues), &Got);
    State_ReadText(Stream, Name, sizeof(Name));
    for (i32 Which = 0; Which < Send_Count; ++Which) {
        i32 Track = Send_NoTrack;

        Stream->Read(&Track, sizeof(Track), &Got);
        Plug->Sends[Which].Track = Track;
        Plug->Sends[Which].Enabled = State_ReadFlag(Stream, false);
    }
    Plug->HighQuality = State_ReadFlag(Stream, false);
    if (State_ReadFlag(Stream, true) != Plug->Editor.InfoOpen) {
        Editor_ToggleInfo(Plug);
    }
    Plug_ApplySends(Plug);
    Soundfont_Load(Plug, Name, false);
    Plug_ReapplyParams(Plug, 0, REC_UpdateValue | REC_UpdateControl);
    Editor_Redraw(Plug);
}

void _stdcall TSoundfontPlug::SaveRestoreState(IStream *Stream, BOOL Save)
{
    if (Save != FALSE) {
        State_Save(this, Stream);
    } else {
        State_Restore(this, Stream);
    }
}

void _stdcall TSoundfontPlug::GetName(int Section, int Index, int Value, char *Name)
{
    (void)Value;
    if (Section == FPN_Param && Param_IsValid(Index)) {
        strcpy(Name, Param_Defs[Index].Name);
    }
}

int _stdcall TSoundfontPlug::ProcessEvent(int EventID, int EventValue, int Flags)
{
    (void)Flags;
    if (EventID == FPE_MaxPoly && Plug_HasSynth(this)) {
        bool Unlimited = EventValue <= 0 || EventValue > Midi_MaxPolyphony;

        fls_set_polyphony(SynthInstance, Unlimited ? Midi_MaxPolyphony : EventValue);
    }
    return 0;
}

static void Plug_PatchChanged(TSoundfontPlug *Plug, i32 Value)
{
    i32 Before = Plug->Channel;

    Plug->Program = ((Value - 1) % Midi_ProgramCount + Midi_ProgramCount) % Midi_ProgramCount;
    Plug->Channel = Value > Midi_ProgramCount ? Midi_DrumChannel : Midi_MelodicChannel;
    if (Before != Plug->Channel) {
        Plug_ReapplyParams(Plug, Param_FirstNrpn, REC_UpdateValue);
        Plug_SendMod(Plug);
    }
}

static void Plug_ParamChanged(TSoundfontPlug *Plug, i32 Index, i32 Value)
{
    const TParamDef *Def = &Param_Defs[Index];

    if (Index == Param_Patch) {
        Plug_PatchChanged(Plug, Value);
    } else if (Index == Param_ReverbSend || Index == Param_ChorusSend) {
        Plug->Sends[Index - Param_FirstSend].Level = Plug_SendLevel(Value);
    } else if (Index == Param_Mod) {
        Plug_SendMod(Plug);
    } else if (Index >= Param_FirstNrpn && Plug_HasSynth(Plug)) {
        fls_set_nrpn(Plug->SynthInstance, Plug->Channel, Def->Nrpn, Value == Def->Default ? FLS_NRPN_UNSET : Value);
    }
}

static void Plug_ShowValueHint(TSoundfontPlug *Plug, i32 Index, i32 Value)
{
    c8 Text[Value_HintSize];

    if (Index >= Param_FirstNrpn && Value == Param_Defs[Index].Default) {
        Text_Copy(Text, sizeof(Text), Value_Disabled);
    } else if (Index == Param_ReverbSend || Index == Param_ChorusSend) {
        snprintf(Text, sizeof(Text), Value_PercentFormat, Plug_SendPercent(Plug, Index - Param_FirstSend));
    } else {
        snprintf(Text, sizeof(Text), Value_PlainFormat, Value);
    }
    Plug_Hint(Plug, Text);
}

int _stdcall TSoundfontPlug::ProcessParam(int Index, int Value, int RECFlags)
{
    if (!Param_IsValid(Index)) {
        return Value;
    }
    if (RECFlags & REC_FromMIDI) {
        f80 Scaled = (f80)Value / Param_MidiScale * (f80)(Param_Defs[Index].Max - Param_Defs[Index].Min);

        Value = (i32)lrintl(Scaled) + Param_Defs[Index].Min;
    }
    if (RECFlags & REC_UpdateValue) {
        Value = Param_Clamp(Index, Value);
        ParamValues[Index] = Value;
        Plug_ParamChanged(this, Index, Value);
    } else if (RECFlags & REC_GetValue) {
        Value = ParamValues[Index];
    }
    if (RECFlags & REC_UpdateControl) {
        Editor_Redraw(this);
    }
    if (Index <= Param_Bank && (RECFlags & (REC_UpdateControl | REC_NoLink))) {
        Soundfont_UpdatePatchName(this);
    }
    if (RECFlags & REC_ShowHint) {
        Plug_ShowValueHint(this, Index, Value);
    }
    return Value;
}

void _stdcall TSoundfontPlug::Eff_Render(PWAV32FS SourceBuffer, PWAV32FS DestBuffer, int Length)
{
    (void)SourceBuffer;
    (void)DestBuffer;
    (void)Length;
}

void _stdcall TSoundfontPlug::Gen_Render(PWAV32FS DestBuffer, int &Length)
{
    f32 *SendBuffers[Send_Count];

    if (!Plug_HasSynth(this)) {
        return;
    }
    Voices_UpdateLevels(this);
    for (i32 Which = 0; Which < Send_Count; ++Which) {
        SendBuffers[Which] = NULL;
        if (Sends[Which].Track >= 0) {
            SendBuffers[Which] = (f32 *)PlugHost->GetSendBuffer(Sends[Which].Track);
        }
    }
    fls_render(SynthInstance, (f32 *)DestBuffer, Length, SendBuffers[Send_Reverb], SendBuffers[Send_Chorus],
               Sends[Send_Reverb].Level, Sends[Send_Chorus].Level);
    Voices_Purge(this);
}

TVoiceHandle _stdcall TSoundfontPlug::TriggerVoice(PVoiceParams VoiceParams, intptr_t SetTag)
{
    return Voices_Trigger(this, VoiceParams, SetTag);
}

void _stdcall TSoundfontPlug::Voice_Release(TVoiceHandle Handle)
{
    Voices_Release(this, Handle);
}

void _stdcall TSoundfontPlug::Voice_Kill(TVoiceHandle Handle)
{
    Voices_Kill(this, Handle);
}

int _stdcall TSoundfontPlug::Voice_ProcessEvent(TVoiceHandle Handle, intptr_t EventID, intptr_t EventValue,
                                                intptr_t Flags)
{
    (void)Handle;
    (void)EventID;
    (void)EventValue;
    (void)Flags;
    return 0;
}

int _stdcall TSoundfontPlug::Voice_Render(TVoiceHandle Handle, PWAV32FS DestBuffer, int &Length)
{
    (void)Handle;
    (void)DestBuffer;
    (void)Length;
    return 0;
}

void _stdcall TSoundfontPlug::NewTick()
{
}

void _stdcall TSoundfontPlug::MIDITick()
{
}

void _stdcall TSoundfontPlug::MIDIIn(int &Msg)
{
    (void)Msg;
}

void _stdcall TSoundfontPlug::MsgIn(intptr_t Msg)
{
    (void)Msg;
}

int _stdcall TSoundfontPlug::OutputVoice_ProcessEvent(TOutVoiceHandle Handle, intptr_t EventID, intptr_t EventValue,
                                                      intptr_t Flags)
{
    (void)Handle;
    (void)EventID;
    (void)EventValue;
    (void)Flags;
    return 0;
}

void _stdcall TSoundfontPlug::OutputVoice_Kill(TVoiceHandle Handle)
{
    (void)Handle;
}

void Plug_SetParamFromEditor(TSoundfontPlug *Plug, i32 Index, i32 Value)
{
    Value = Param_Clamp(Index, Value);
    if (Value == Plug->ParamValues[Index]) {
        return;
    }
    Plug->ProcessParam(Index, Value, REC_UpdateValue | REC_ShowHint | REC_NoLink);
    Plug->PlugHost->OnParamChanged(Plug->HostTag, Index, Value);
    Editor_Redraw(Plug);
    if (Editor_IsDragging(Plug)) {
        Editor_RedrawNow(Plug);
    }
}

void Plug_SelectPatch(TSoundfontPlug *Plug, i32 Bank, i32 Patch)
{
    if (Bank == Midi_DrumBank) {
        Plug_SetParamFromEditor(Plug, Param_Patch, Midi_ProgramCount + Patch + 1);
    } else {
        Plug_SetParamFromEditor(Plug, Param_Bank, Bank);
        Plug_SetParamFromEditor(Plug, Param_Patch, Patch + 1);
    }
    Soundfont_UpdatePatchName(Plug);
}

void Plug_SetSendTrack(TSoundfontPlug *Plug, i32 Which, i32 TrackNumber)
{
    if (TrackNumber < 0) {
        TrackNumber = 0;
    }
    if (TrackNumber > Plug->SendTrackCount) {
        TrackNumber = Plug->SendTrackCount;
    }
    if (Plug->Sends[Which].Track == TrackNumber - 1) {
        return;
    }
    Plug->PlugHost->LockMix_Old();
    Plug->Sends[Which].Track = TrackNumber - 1;
    Plug->PlugHost->UnlockMix_Old();
    Plug_MarkDirty(Plug);
    Editor_Redraw(Plug);
}

void Plug_ToggleSend(TSoundfontPlug *Plug, i32 Which)
{
    Plug->Sends[Which].Enabled = !Plug->Sends[Which].Enabled;
    Plug_ApplySends(Plug);
    Plug_MarkDirty(Plug);
    Editor_Redraw(Plug);
}

void Plug_ToggleQuality(TSoundfontPlug *Plug)
{
    Plug->HighQuality = !Plug->HighQuality;
    Plug_MarkDirty(Plug);
    Editor_Redraw(Plug);
}

void Plug_FormatValue(const TSoundfontPlug *Plug, i32 Index, c8 *Dest, usize DestSize)
{
    const TParamDef *Def = &Param_Defs[Index];
    i32 Value = Plug->ParamValues[Index];

    if (Param_CanBeOff(Index) && Value == Def->Default) {
        snprintf(Dest, DestSize, Value_OffFormat, Def->Name);
    } else if (Index == Param_ReverbSend || Index == Param_ChorusSend) {
        snprintf(Dest, DestSize, Value_SendFormat, Def->Name, Plug_SendPercent(Plug, Index - Param_FirstSend), Value);
    } else if (Index == Param_Patch && Value > Midi_ProgramCount) {
        snprintf(Dest, DestSize, Value_DrumKitFormat, Def->Name, Value, Value - Midi_ProgramCount);
    } else {
        snprintf(Dest, DestSize, Value_NamedFormat, Def->Name, Value);
    }
}
