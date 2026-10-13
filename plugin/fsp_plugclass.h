#ifndef FSP_PLUGCLASS_H
#define FSP_PLUGCLASS_H

#include "fsp_def.h"

#include "sdk/fp_plugclass.h"

#include "fsp_editor.h"
#include "fsp_params.h"
#include "fsp_text.h"

const i32 Synth_NoInstance = -1;
const i32 Send_NoTrack = -1;

enum {
    Send_Reverb,
    Send_Chorus,
    Send_Count
};

typedef struct {
    i32 Track;
    f32 Level;
    bool Enabled;
} TSend, *PSend;

class TSoundfontPlug : public TFruityPlug {
public:
    TFruityPlugHost *PlugHost;
    i32 ParamValues[Param_Count];
    i32 SynthInstance;
    i32 Program;
    i32 Channel;
    i32 SampleRate;
    i32 Quality;
    i32 SendTrackCount;
    bool HighQuality;
    TSend Sends[Send_Count];
    c8 FileName[Text_PathSize];
    c8 BankName[Text_NameSize];
    c8 PatchName[Text_NameSize];
    u32 VoiceCount;
    TEditor Editor;

    TSoundfontPlug(TPluginTag SetHostTag, TFruityPlugHost *SetPlugHost);
    virtual ~TSoundfontPlug();

    virtual void _stdcall DestroyObject();
    virtual intptr_t _stdcall Dispatcher(intptr_t ID, intptr_t Index, intptr_t Value);
    virtual void _stdcall Idle_Public();
    virtual void _stdcall SaveRestoreState(IStream *Stream, BOOL Save);
    virtual void _stdcall GetName(int Section, int Index, int Value, char *Name);
    virtual int _stdcall ProcessEvent(int EventID, int EventValue, int Flags);
    virtual int _stdcall ProcessParam(int Index, int Value, int RECFlags);
    virtual void _stdcall Eff_Render(PWAV32FS SourceBuffer, PWAV32FS DestBuffer, int Length);
    virtual void _stdcall Gen_Render(PWAV32FS DestBuffer, int &Length);
    virtual TVoiceHandle _stdcall TriggerVoice(PVoiceParams VoiceParams, intptr_t SetTag);
    virtual void _stdcall Voice_Release(TVoiceHandle Handle);
    virtual void _stdcall Voice_Kill(TVoiceHandle Handle);
    virtual int _stdcall Voice_ProcessEvent(TVoiceHandle Handle, intptr_t EventID, intptr_t EventValue,
                                            intptr_t Flags);
    virtual int _stdcall Voice_Render(TVoiceHandle Handle, PWAV32FS DestBuffer, int &Length);
    virtual void _stdcall NewTick();
    virtual void _stdcall MIDITick();
    virtual void _stdcall MIDIIn(int &Msg);
    virtual void _stdcall MsgIn(intptr_t Msg);
    virtual int _stdcall OutputVoice_ProcessEvent(TOutVoiceHandle Handle, intptr_t EventID, intptr_t EventValue,
                                                 intptr_t Flags);
    virtual void _stdcall OutputVoice_Kill(TVoiceHandle Handle);
};

extern TFruityPlugInfo Plug_Info;

bool Plug_HasSynth(const TSoundfontPlug *Plug);
bool Plug_IsDrumChannel(const TSoundfontPlug *Plug);
i32 Plug_CurrentBank(const TSoundfontPlug *Plug);
void Plug_Hint(TSoundfontPlug *Plug, const c8 *Text);
void Plug_MarkDirty(TSoundfontPlug *Plug);
void Plug_SetParamFromEditor(TSoundfontPlug *Plug, i32 Index, i32 Value);
void Plug_SelectPatch(TSoundfontPlug *Plug, i32 Bank, i32 Patch);
void Plug_SetSendTrack(TSoundfontPlug *Plug, i32 Which, i32 TrackNumber);
void Plug_ToggleSend(TSoundfontPlug *Plug, i32 Which);
void Plug_ToggleQuality(TSoundfontPlug *Plug);
void Plug_FormatValue(const TSoundfontPlug *Plug, i32 Index, c8 *Dest, usize DestSize);
i32 Plug_SendPercent(const TSoundfontPlug *Plug, i32 Which);

#endif
