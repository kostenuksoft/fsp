#include "fsp_soundfont.h"

#include <stdio.h>
#include <string.h>

#include "../engine/flspro.h"

#include "fsp_midi.h"
#include "fsp_plugclass.h"
#include "fsp_recent.h"

const c8 Soundfont_LoadingHintFormat[] = "^iLoading %s soundfont...";
const c8 Soundfont_ErrorFormat[] = "%s|%s";
const c8 Soundfont_DefaultError[] = "Could not load the soundfont.";
const c8 Soundfont_DrumKitSuffix[] = " (DrumKit)";
const usize Soundfont_LongPath = 250;
const c8 Soundfont_ForeignSeparator = '/';
const c8 Soundfont_NativeSeparator = '\\';

bool Soundfont_IsLoadable(const c8 *FileName)
{
    return Text_HasExt(FileName, Soundfont_Ext) || Text_HasExt(FileName, Soundbank_Ext);
}

bool Soundfont_HasPatch(const TSoundfontPlug *Plug)
{
    return !Text_Same(Plug->PatchName, Soundfont_NoName);
}

void Soundfont_UpdatePatchName(TSoundfontPlug *Plug)
{
    c8 Name[Text_NameSize];

    Name[0] = 0;
    if (Plug_HasSynth(Plug)) {
        fls_patch_name(Plug->SynthInstance, Plug->Program, Plug_CurrentBank(Plug), Name, sizeof(Name));
    }
    if (Text_IsEmpty(Name)) {
        Text_Copy(Plug->PatchName, sizeof(Plug->PatchName), Soundfont_NoName);
    } else {
        Text_Copy(Plug->PatchName, sizeof(Plug->PatchName), Name);
        if (Plug_IsDrumChannel(Plug)) {
            Text_Append(Plug->PatchName, sizeof(Plug->PatchName), Soundfont_DrumKitSuffix);
        }
    }
    Editor_Redraw(Plug);
}

static void Soundfont_NativePath(const c8 *FileName, c8 *Dest, usize DestSize)
{
    c8 ShortPath[MAX_PATH];
    const c8 *Chosen = FileName;

    if (GetShortPathNameA(FileName, ShortPath, sizeof(ShortPath)) > 0 && strlen(FileName) >= Soundfont_LongPath) {
        Chosen = ShortPath;
    }
    Text_Copy(Dest, DestSize, Chosen);
    for (c8 *Cursor = Dest; *Cursor; ++Cursor) {
        if (*Cursor == Soundfont_ForeignSeparator) {
            *Cursor = Soundfont_NativeSeparator;
        }
    }
}

static void Soundfont_ReportFailure(TSoundfontPlug *Plug)
{
    c8 Message[Text_PathSize];
    const c8 *Reason = fls_last_message();

    if (Text_IsEmpty(Reason)) {
        Reason = Soundfont_DefaultError;
    }
    snprintf(Message, sizeof(Message), Soundfont_ErrorFormat, Plug_Info.LongName, Reason);
    Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_MsgBox, (intptr_t)Message, MB_OK | MB_ICONWARNING);
}

static void Soundfont_LoadIntoSynth(TSoundfontPlug *Plug)
{
    c8 Native[Text_PathSize];

    Plug->PlugHost->SuspendOutput_Old();
    fls_clear_soundfont(Plug->SynthInstance);
    if (!Text_IsEmpty(Plug->FileName)) {
        Soundfont_NativePath(Plug->FileName, Native, sizeof(Native));
        if (fls_load_soundfont(Plug->SynthInstance, Native) != FLS_OK) {
            Soundfont_ReportFailure(Plug);
        } else {
            Recent_Add(Plug->FileName);
        }
    }
    Plug->PlugHost->ResumeOutput_Old();
}

static void Soundfont_PickFirstPatch(TSoundfontPlug *Plug)
{
    c8 Name[Text_NameSize];
    i32 Bank = Plug->ParamValues[Param_Bank];

    fls_patch_name(Plug->SynthInstance, Plug->Program, Bank, Name, sizeof(Name));
    if (!Text_IsEmpty(Name)) {
        return;
    }
    for (i32 Patch = 0; Patch < Midi_ProgramCount; ++Patch) {
        fls_patch_name(Plug->SynthInstance, Patch, Bank, Name, sizeof(Name));
        if (!Text_IsEmpty(Name)) {
            Plug_SetParamFromEditor(Plug, Param_Patch, Patch + 1);
            return;
        }
    }
}

static void Soundfont_Announce(TSoundfontPlug *Plug, bool FromUser)
{
    c8 Shown[Text_PathSize];
    c8 Hint[Text_PathSize];

    Text_BaseName(Plug->FileName, Shown, sizeof(Shown));
    if (FromUser && !Text_IsEmpty(Plug->FileName)) {
        Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_SetNewName, 0, (intptr_t)Shown);
    }
    snprintf(Hint, sizeof(Hint), Soundfont_LoadingHintFormat, Shown);
    Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_OnHint_Direct, 0, (intptr_t)Hint);
}

static void Soundfont_Locate(TSoundfontPlug *Plug, const c8 *FileName)
{
    const c8 *Located = NULL;

    if (!Text_IsEmpty(FileName)) {
        Located = (const c8 *)Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_LocateDataFile, 0, (intptr_t)FileName);
    }
    Text_Copy(Plug->FileName, sizeof(Plug->FileName), Text_IsEmpty(Located) ? FileName : Located);
}

void Soundfont_Load(TSoundfontPlug *Plug, const c8 *FileName, bool FromUser)
{
    c8 Requested[Text_PathSize];
    c8 Name[Text_NameSize];

    Text_Copy(Requested, sizeof(Requested), FileName);
    if (!Text_IsEmpty(Requested) && !Text_HasExt(Requested, Soundfont_Ext)) {
        return;
    }
    if (Text_Same(Requested, Plug->FileName)) {
        return;
    }
    Soundfont_Locate(Plug, Requested);
    Soundfont_Announce(Plug, FromUser);
    if (Plug_HasSynth(Plug)) {
        Soundfont_LoadIntoSynth(Plug);
    }
    Plug_Hint(Plug, "");

    Name[0] = 0;
    if (Plug_HasSynth(Plug)) {
        fls_bank_name(Plug->SynthInstance, Name, sizeof(Name));
    }
    Text_Copy(Plug->BankName, sizeof(Plug->BankName), Text_IsEmpty(Name) ? Soundfont_NoName : Name);
    if (FromUser && !Plug_IsDrumChannel(Plug) && Plug_HasSynth(Plug)) {
        Soundfont_PickFirstPatch(Plug);
    }
    Soundfont_UpdatePatchName(Plug);
}

void Soundfont_Reload(TSoundfontPlug *Plug)
{
    c8 Kept[Text_PathSize];

    Text_Copy(Kept, sizeof(Kept), Plug->FileName);
    Plug->FileName[0] = 0;
    Soundfont_Load(Plug, Kept, false);
}
