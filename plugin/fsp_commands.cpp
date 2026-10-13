#include "fsp_commands.h"

#include <commdlg.h>
#include <math.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsp_menu.h"
#include "fsp_patches.h"
#include "fsp_plugclass.h"
#include "fsp_recent.h"
#include "fsp_soundfont.h"

const c8 OpenDialog_Filter[] = "SoundFont 2.0 Banks (*.sf2)\0*.sf2\0";
const c8 OpenDialog_ShellVerb[] = "open";

const c8 SoundfontMenu_BrowseLabel[] = "&Browse...";
const c8 SoundfontMenu_NoneLabel[] = "(&none)";

enum {
    SoundfontMenu_CmdBrowse = 1,
    SoundfontMenu_CmdNone,
    SoundfontMenu_CmdFirstRecent
};

const c8 ParamMenu_ResetLabel[] = "&Reset";
const c8 ParamMenu_TypeInLabel[] = "&Type in value...";
const c8 ParamMenu_HostTypeInMark[] = "type in";
const c8 ParamMenu_HostSeparator[] = "-";

enum {
    ParamMenu_CmdReset = 1,
    ParamMenu_CmdTypeIn,
    ParamMenu_CmdFirstHost
};

const c8 PatchMenu_ItemFormat[] = "%03d  %s";
const c8 PatchMenu_GroupFormat[] = "%03d - %03d";
const c8 PatchMenu_GroupHintFormat[] = "%s ...";
const c8 PatchMenu_BankFormat[] = "Bank %03d";
const c8 PatchMenu_DrumBankLabel[] = "Drum kits";
const c8 PatchMenu_CountFormat[] = "%d";
const u32 PatchMenu_FlatLimit = 20;
const u32 PatchMenu_GroupSize = 16;
const i32 PatchMenu_CmdFirst = 1;

const c8 TypeIn_CaptionFormat[] = "%s: %d to %d, or 0%% to 100%%%s";
const c8 TypeIn_OffSuffix[] = ", or off";
const c8 TypeIn_OffText[] = "off";
const c8 *const TypeIn_OffWords[] = { "off", "default", "reset", "disabled" };
const c8 TypeIn_ValueFormat[] = "%d";
const c8 TypeIn_PercentMark = '%';
const c8 TypeIn_DecimalComma = ',';
const c8 TypeIn_DecimalPoint = '.';
const f64 TypeIn_PercentMax = 100.0;
const f64 TypeIn_RoundHalf = 0.5;
const u32 TypeIn_CaptionSize = 200;
const i32 TypeIn_PromptAtCursor = -1;
const i32 TypeIn_NoColor = -1;

void Commands_OpenDialog(TSoundfontPlug *Plug)
{
    c8 FileName[MAX_PATH];
    OPENFILENAMEA Dialog;

    FileName[0] = 0;
    memset(&Dialog, 0, sizeof(Dialog));
    Dialog.lStructSize = sizeof(Dialog);
    Dialog.hwndOwner = Plug->EditorHandle;
    Dialog.lpstrFilter = OpenDialog_Filter;
    Dialog.lpstrFile = FileName;
    Dialog.nMaxFile = sizeof(FileName);
    Dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&Dialog)) {
        Soundfont_Load(Plug, FileName, true);
    }
}

void Commands_EditSoundfont(TSoundfontPlug *Plug)
{
    if (!Text_IsEmpty(Plug->FileName)) {
        ShellExecuteA(Plug->EditorHandle, OpenDialog_ShellVerb, Plug->FileName, NULL, NULL, SW_SHOWNORMAL);
    }
}

static void SoundfontMenu_AddRecentItems(HMENU Menu, const TSoundfontPlug *Plug, const TRecentList *Recent)
{
    c8 Name[Text_PathSize];
    c8 Folder[Text_PathSize];

    for (u32 Index = 0; Index < Recent->Count; ++Index) {
        const c8 *Path = Recent->Paths[Index];
        u32 Flags = Text_SameNoCase(Path, Plug->FileName) ? Menu_Checked : Menu_Plain;

        Text_BaseName(Path, Name, sizeof(Name));
        Text_DirName(Path, Folder, sizeof(Folder));
        Menu_AddItemWithHint(Menu, SoundfontMenu_CmdFirstRecent + (i32)Index, Name, Flags, Folder);
    }
    if (Recent->Count > 0) {
        Menu_AddSeparator(Menu);
    }
}

void Commands_SoundfontMenu(TSoundfontPlug *Plug)
{
    TRecentList Recent;
    HMENU Menu = Menu_Create();
    POINT Spot = { Layout_SoundfontMenu.X, Layout_SoundfontMenu.Y };
    i32 Command;
    i32 Picked;

    Recent_Load(&Recent);
    SoundfontMenu_AddRecentItems(Menu, Plug, &Recent);
    Menu_AddItem(Menu, SoundfontMenu_CmdBrowse, SoundfontMenu_BrowseLabel, Menu_Plain);
    Menu_AddItem(Menu, SoundfontMenu_CmdNone, SoundfontMenu_NoneLabel,
                 Text_IsEmpty(Plug->FileName) ? Menu_Disabled : Menu_Plain);
    ClientToScreen(Plug->EditorHandle, &Spot);
    Command = Menu_Track(Menu, Spot, Plug->EditorHandle);
    Picked = Command - SoundfontMenu_CmdFirstRecent;
    if (Command == SoundfontMenu_CmdBrowse) {
        Commands_OpenDialog(Plug);
    } else if (Command == SoundfontMenu_CmdNone) {
        Soundfont_Load(Plug, "", true);
    } else if (Picked >= 0 && (u32)Picked < Recent.Count) {
        Soundfont_Load(Plug, Recent.Paths[Picked], true);
    }
}

static bool ParamMenu_IsHostTypeIn(const c8 *Name)
{
    c8 Lowered[Text_NameSize];

    Text_Copy(Lowered, sizeof(Lowered), Name);
    Text_Lower(Lowered);
    return Text_Contains(Lowered, ParamMenu_HostTypeInMark);
}

static bool ParamMenu_AddHostItems(HMENU Menu, TSoundfontPlug *Plug, i32 Param)
{
    bool HostTypeIn = false;

    for (i32 Index = 0;; ++Index) {
        PParamMenuEntry Entry = (PParamMenuEntry)Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_GetParamMenuEntry, Param,
                                                                            Index);
        const c8 *Name;
        u32 Flags = Menu_Plain;

        if (!Entry) {
            break;
        }
        Name = Entry->Name ? Entry->Name : "";
        if (Text_Same(Name, ParamMenu_HostSeparator)) {
            Menu_AddSeparator(Menu);
            continue;
        }
        if (ParamMenu_IsHostTypeIn(Name)) {
            HostTypeIn = true;
            Menu_AddItem(Menu, ParamMenu_CmdTypeIn, Name, Menu_Plain);
            continue;
        }
        if (Entry->Flags & FHP_Checked) {
            Flags |= Menu_Checked;
        }
        if (Entry->Flags & FHP_Disabled) {
            Flags |= Menu_Disabled;
        }
        Menu_AddItem(Menu, ParamMenu_CmdFirstHost + Index, Name, Flags);
    }
    return HostTypeIn;
}

void Commands_ParamMenu(TSoundfontPlug *Plug, i32 Param, POINT ScreenSpot)
{
    HMENU Menu = Menu_Create();
    i32 Command;

    Menu_AddItem(Menu, ParamMenu_CmdReset, ParamMenu_ResetLabel, Menu_Plain);
    Menu_AddSeparator(Menu);
    if (!ParamMenu_AddHostItems(Menu, Plug, Param)) {
        Menu_AddSeparator(Menu);
        Menu_AddItem(Menu, ParamMenu_CmdTypeIn, ParamMenu_TypeInLabel, Menu_Plain);
    }
    Command = Menu_Track(Menu, ScreenSpot, Plug->EditorHandle);
    if (Command == ParamMenu_CmdReset) {
        Plug_SetParamFromEditor(Plug, Param, Param_Defs[Param].Default);
    } else if (Command == ParamMenu_CmdTypeIn) {
        Commands_TypeValue(Plug, Param);
    } else if (Command >= ParamMenu_CmdFirstHost) {
        Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_ParamMenu, Param, Command - ParamMenu_CmdFirstHost);
    }
}

static i32 PatchMenu_Command(const TPatchEntry *Entry)
{
    return PatchMenu_CmdFirst + Entry->Bank * Midi_ProgramCount + Entry->Patch;
}

static bool PatchMenu_IsCurrent(const TSoundfontPlug *Plug, const TPatchEntry *Entry)
{
    return Entry->Bank == Plug_CurrentBank(Plug) && Entry->Patch == Plug->Program;
}

static bool PatchMenu_AddItems(HMENU Menu, const TSoundfontPlug *Plug, const TPatchEntry *Entries, u32 Count)
{
    c8 Label[Text_LabelSize];
    bool HasCurrent = false;

    for (u32 Index = 0; Index < Count; ++Index) {
        bool Current = PatchMenu_IsCurrent(Plug, &Entries[Index]);

        HasCurrent = HasCurrent || Current;
        snprintf(Label, sizeof(Label), PatchMenu_ItemFormat, Entries[Index].Patch + 1, Entries[Index].Name);
        Menu_AddItem(Menu, PatchMenu_Command(&Entries[Index]), Label, Current ? Menu_Checked : Menu_Plain);
    }
    return HasCurrent;
}

static void PatchMenu_AddGroups(HMENU Menu, const TSoundfontPlug *Plug, const TPatchEntry *Entries, u32 Count)
{
    c8 Label[Text_LabelSize];
    c8 Hint[Text_LabelSize];

    for (u32 First = 0; First < Count; First += PatchMenu_GroupSize) {
        u32 Size = Count - First < PatchMenu_GroupSize ? Count - First : PatchMenu_GroupSize;
        HMENU Group = Menu_Create();
        bool HasCurrent = PatchMenu_AddItems(Group, Plug, &Entries[First], Size);

        snprintf(Label, sizeof(Label), PatchMenu_GroupFormat, Entries[First].Patch + 1,
                 Entries[First + Size - 1].Patch + 1);
        snprintf(Hint, sizeof(Hint), PatchMenu_GroupHintFormat, Entries[First].Name);
        Menu_AddSubMenu(Menu, Group, Label, HasCurrent ? Menu_Checked : Menu_Plain, Hint);
    }
}

static void PatchMenu_AddBank(HMENU Menu, const TSoundfontPlug *Plug, const TPatchEntry *Entries, u32 Count,
                              bool OnlyBank)
{
    c8 Label[Text_LabelSize];
    c8 Hint[Text_NumberSize];
    HMENU BankMenu = OnlyBank ? Menu : Menu_Create();
    bool BankCurrent = Entries[0].Bank == Plug_CurrentBank(Plug);

    if (Count <= PatchMenu_FlatLimit) {
        PatchMenu_AddItems(BankMenu, Plug, Entries, Count);
    } else {
        PatchMenu_AddGroups(BankMenu, Plug, Entries, Count);
    }
    if (OnlyBank) {
        return;
    }
    if (Entries[0].Bank == Midi_DrumBank) {
        Text_Copy(Label, sizeof(Label), PatchMenu_DrumBankLabel);
    } else {
        snprintf(Label, sizeof(Label), PatchMenu_BankFormat, Entries[0].Bank);
    }
    snprintf(Hint, sizeof(Hint), PatchMenu_CountFormat, (i32)Count);
    Menu_AddSubMenu(Menu, BankMenu, Label, BankCurrent ? Menu_Checked : Menu_Plain, Hint);
}

static u32 PatchMenu_BankLength(const TPatchList *List, u32 First)
{
    u32 Length = 1;

    while (First + Length < List->Count && List->Entries[First + Length].Bank == List->Entries[First].Bank) {
        ++Length;
    }
    return Length;
}

void Commands_PatchMenu(TSoundfontPlug *Plug, POINT ScreenSpot)
{
    PPatchList List;
    HMENU Menu;
    bool OnlyBank;
    i32 Command;

    if (!Plug_HasSynth(Plug) || Text_IsEmpty(Plug->FileName)) {
        return;
    }
    List = Patches_Scan(Plug->SynthInstance);
    if (List->Count == 0) {
        return;
    }
    OnlyBank = PatchMenu_BankLength(List, 0) == List->Count;
    Menu = Menu_Create();
    for (u32 First = 0; First < List->Count; First += PatchMenu_BankLength(List, First)) {
        PatchMenu_AddBank(Menu, Plug, &List->Entries[First], PatchMenu_BankLength(List, First), OnlyBank);
    }
    Command = Menu_Track(Menu, ScreenSpot, Plug->EditorHandle);
    if (Command >= PatchMenu_CmdFirst) {
        i32 Bank = (Command - PatchMenu_CmdFirst) / Midi_ProgramCount;
        i32 Patch = (Command - PatchMenu_CmdFirst) % Midi_ProgramCount;

        Plug_SelectPatch(Plug, Bank, Patch);
    }
}

static bool TypeIn_MeansOff(const c8 *Text)
{
    for (usize Index = 0; Index < ARRAY_COUNT(TypeIn_OffWords); ++Index) {
        if (Text_Same(Text, TypeIn_OffWords[Index])) {
            return true;
        }
    }
    return false;
}

static bool TypeIn_Parse(c8 *Text, i32 Min, i32 Max, i32 *Value)
{
    usize Length = strlen(Text);
    bool Percent = Text[Length - 1] == TypeIn_PercentMark;
    c8 *End = NULL;
    f64 Number;

    if (Percent) {
        Text[Length - 1] = 0;
    }
    for (c8 *Cursor = Text; *Cursor; ++Cursor) {
        if (*Cursor == TypeIn_DecimalComma) {
            *Cursor = TypeIn_DecimalPoint;
        }
    }
    Number = strtod(Text, &End);
    if (End == Text) {
        return false;
    }
    if (Percent) {
        if (Number < 0) {
            Number = 0;
        }
        if (Number > TypeIn_PercentMax) {
            Number = TypeIn_PercentMax;
        }
        Number = Min + Number / TypeIn_PercentMax * (f64)(Max - Min);
    }
    *Value = (i32)floor(Number + TypeIn_RoundHalf);
    return true;
}

void Commands_TypeValue(TSoundfontPlug *Plug, i32 Param)
{
    const TParamDef *Def = &Param_Defs[Param];
    bool HasOff = Param_CanBeOff(Param) && Def->Default == Def->Min;
    i32 Min = HasOff ? Def->Min + 1 : Def->Min;
    c8 Caption[TypeIn_CaptionSize];
    c8 Text[Text_NameSize];
    int Color = TypeIn_NoColor;
    i32 Value;

    snprintf(Caption, sizeof(Caption), TypeIn_CaptionFormat, Def->Name, Min, Def->Max, HasOff ? TypeIn_OffSuffix : "");
    if (HasOff && Plug->ParamValues[Param] == Def->Default) {
        Text_Copy(Text, sizeof(Text), TypeIn_OffText);
    } else {
        snprintf(Text, sizeof(Text), TypeIn_ValueFormat, Plug->ParamValues[Param]);
    }
    if (!Plug->PlugHost->PromptEdit(TypeIn_PromptAtCursor, TypeIn_PromptAtCursor, Caption, Text, Color)) {
        return;
    }
    Text_Lower(Text);
    Text_Trim(Text);
    if (Text_IsEmpty(Text)) {
        return;
    }
    if (TypeIn_MeansOff(Text)) {
        Plug_SetParamFromEditor(Plug, Param, Def->Default);
        return;
    }
    if (TypeIn_Parse(Text, Min, Def->Max, &Value)) {
        Plug_SetParamFromEditor(Plug, Param, Value);
    }
}
