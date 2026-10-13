#include <math.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "../plugin/sdk/fp_plugclass.h"

#include "../plugin/fsp_def.h"
#include "../plugin/fsp_editor.h"
#include "../plugin/fsp_explorer.h"
#include "../plugin/fsp_layout.h"
#include "../plugin/fsp_params.h"

const c8 Test_ParentClass[] = "HostTestParent";
const c8 Test_MenuClass[] = "#32768";
const c8 Test_EntryPoint[] = "CreatePlugInstance";
const c8 Test_MenusMode[] = "menus";
const c8 Test_AudioFile[] = "host_test.f32";
const c8 Test_EditorShot[] = "editor.bmp";
const c8 Test_PatchMenuShot[] = "menu_patch.bmp";
const c8 Test_KnobMenuShot[] = "menu_knob.bmp";
const c8 Test_SoundfontMenuShot[] = "menu_sf.bmp";
const c8 *const Test_ExplorerShots[] = { "explorer1.bmp", "explorer2.bmp", "explorer3.bmp" };

const i32 Test_HostVersion = 11001001;
const i32 Test_SampleRate = 44100;
const i32 Test_BlockFrames = 512;
const i32 Test_Channels = 2;
const i32 Test_SendTracks = 4;
const i32 Test_SendFrames = 4096;
const i32 Test_FirstTag = 1234;
const i32 Test_SecondTag = 4321;
const i32 Test_FirstVoiceTag = 777;
const i32 Test_SecondVoiceTag = 778;
const f32 Test_NoteVolume = 0.78125f;
const i32 Test_NotePitch = 6000;
const i32 Test_HeldBlocks = 60;
const i32 Test_FilteredBlocks = 30;
const i32 Test_ReleaseBlocks = 400;
const i32 Test_SecondBlocks = 40;
const i32 Test_ReverbSendValue = 256;
const i32 Test_CutoffValue = 40;
const i32 Test_NamedParam = Param_Attack;

const u16 Bitmap_Signature = 0x4d42;
const u16 Bitmap_BitCount = 24;
const i32 Bitmap_BytesPerPixel = 3;
const i32 Bitmap_RowAlign = 4;

const DWORD Wait_ExplorerStepMs = 100;
const i32 Wait_ExplorerSteps = 50;
const DWORD Wait_ExplorerSettleMs = 400;
const DWORD Wait_QuerySettleMs = 500;
const DWORD Wait_MenuOpenMs = 700;
const DWORD Wait_MenuSettleMs = 400;
const DWORD Wait_KeyMs = 250;
const DWORD Wait_MenuDriverMs = 8000;
const i32 Menu_EscapeCount = 5;
constexpr TSpot Test_MenuParentSpot = { 300, 200 };
constexpr TSpot Test_ExplorerParentSpot = { 200, 200 };

enum {
    Arg_Plugin = 1,
    Arg_Soundfont = 2,
    Arg_Mode = 3,
    Arg_Required = 3
};

enum {
    Exit_Ok = 0,
    Exit_LoadFailed = 1,
    Exit_Usage = 2
};

static i32 Test_KilledVoices;
static c8 Test_LastPath[MAX_PATH];
static const c8 *Test_Query = "";
static const c8 *Test_MenuShot;
static i32 Test_MenuKeyPairs;

class THost : public TFruityPlugHost {
public:
    f32 SendBuffers[Test_SendTracks][Test_SendFrames * Test_Channels];

    virtual intptr_t _stdcall Dispatcher(TPluginTag Sender, intptr_t ID, intptr_t Index, intptr_t Value)
    {
        if (ID == FHD_LocateDataFile || ID == FHD_PackDataFile) {
            snprintf(Test_LastPath, sizeof(Test_LastPath), "%s", (const c8 *)Value);
            return (intptr_t)Test_LastPath;
        }
        if (ID == FHD_MsgBox) {
            printf("host msgbox\n");
        }
        return 0;
    }

    virtual void _stdcall OnParamChanged(TPluginTag, int, int) {}
    virtual void _stdcall OnHint(TPluginTag, char *) {}
    virtual void _stdcall ComputeLRVol_Old(float &, float &, int, float) {}
    virtual void _stdcall Voice_Release(intptr_t) {}

    virtual void _stdcall Voice_Kill(intptr_t Sender, BOOL)
    {
        ++Test_KilledVoices;
        printf("host: voice kill tag=%lld\n", (i64)Sender);
    }

    virtual int _stdcall Voice_ProcessEvent(intptr_t, intptr_t, intptr_t, intptr_t)
    {
        return 0;
    }

    virtual void _stdcall LockMix_Old() {}
    virtual void _stdcall UnlockMix_Old() {}
    virtual void _stdcall MIDIOut_Delayed(TPluginTag, intptr_t) {}
    virtual void _stdcall MIDIOut(TPluginTag, intptr_t) {}
    virtual void _stdcall AddWave_32FM_32FS_Ramp(void *, void *, int, float, float, float &, float &) {}
    virtual void _stdcall AddWave_32FS_32FS_Ramp(void *, void *, int, float, float, float &, float &) {}

    virtual bool _stdcall LoadSample(TSampleHandle &, char *, PWaveFormatExtensible, int)
    {
        return false;
    }

    virtual void *_stdcall GetSampleData(TSampleHandle, int &)
    {
        return NULL;
    }

    virtual void _stdcall CloseSample(TSampleHandle) {}

    virtual int _stdcall GetSongMixingTime()
    {
        return 0;
    }

    virtual double _stdcall GetSongMixingTime_A()
    {
        return 0;
    }

    virtual double _stdcall GetSongPlayingTime()
    {
        return 0;
    }

    virtual void _stdcall OnControllerChanged(TPluginTag, intptr_t, intptr_t) {}

    virtual void *_stdcall GetSendBuffer(intptr_t Num)
    {
        if (Num < 0 || Num >= Test_SendTracks) {
            return NULL;
        }
        return SendBuffers[Num];
    }

    virtual void _stdcall PlugMsg_Delayed(TPluginTag, intptr_t) {}
    virtual void _stdcall PlugMsg_Kill(TPluginTag, intptr_t) {}
    virtual void _stdcall GetSampleInfo(TSampleHandle, PSampleInfo) {}
    virtual void _stdcall DistWave_32FM(int, int, void *, int, float, float, float) {}

    virtual void *_stdcall GetMixBuffer(int)
    {
        return NULL;
    }

    virtual void *_stdcall GetInsBuffer(TPluginTag, int)
    {
        return NULL;
    }

    virtual BOOL _stdcall PromptEdit(int, int, char *, char *, int &)
    {
        return FALSE;
    }

    virtual void _stdcall SuspendOutput_Old() {}
    virtual void _stdcall ResumeOutput_Old() {}
    virtual void _stdcall GetSampleRegion(TSampleHandle, int, PSampleRegion) {}
    virtual void _stdcall ComputeLRVol(float &, float &, float, float) {}
    virtual void _stdcall LockPlugin(TPluginTag) {}
    virtual void _stdcall UnlockPlugin(TPluginTag) {}
    virtual void _stdcall LockMix_Shared_Old() {}
    virtual void _stdcall UnlockMix_Shared_Old() {}
    virtual void _stdcall GetInBuffer(TPluginTag, intptr_t, PIOBuffer) {}
    virtual void _stdcall GetOutBuffer(TPluginTag, intptr_t, PIOBuffer) {}

    virtual TOutVoiceHandle _stdcall TriggerOutputVoice(TVoiceParams *, intptr_t, intptr_t)
    {
        return 0;
    }

    virtual void _stdcall OutputVoice_Release(TOutVoiceHandle) {}
    virtual void _stdcall OutputVoice_Kill(TOutVoiceHandle) {}

    virtual int _stdcall OutputVoice_ProcessEvent(TOutVoiceHandle, intptr_t, intptr_t, intptr_t)
    {
        return 0;
    }

    virtual BOOL _stdcall PromptEdit_Ex(int, int, const char *, char *, int &, int &, int &, int, int)
    {
        return FALSE;
    }

    virtual void _stdcall SuspendOutput(TPluginTag) {}
    virtual void _stdcall ResumeOutput(TPluginTag) {}
};

typedef TFruityPlug *(_stdcall *TCreatePlug)(TFruityPlugHost *, TPluginTag);

static THost Test_Host;

static TSpot Test_Center(const TBox *Box)
{
    TSpot Spot = { Box->X + Box->W / 2, Box->Y + Box->H / 2 };

    return Spot;
}

static LPARAM Test_PointParam(TSpot Spot)
{
    return MAKELPARAM(Spot.X, Spot.Y);
}

static void Bitmap_Save(HDC Source, i32 X, i32 Y, i32 W, i32 H, const c8 *FileName)
{
    BITMAPFILEHEADER FileHeader;
    BITMAPINFO Info;
    void *Bits = NULL;
    i32 Stride = (W * Bitmap_BytesPerPixel + Bitmap_RowAlign - 1) / Bitmap_RowAlign * Bitmap_RowAlign;
    HDC Memory = CreateCompatibleDC(Source);
    HBITMAP Bitmap;
    HGDIOBJ Previous;
    FILE *File;

    memset(&Info, 0, sizeof(Info));
    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = W;
    Info.bmiHeader.biHeight = H;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = Bitmap_BitCount;
    Bitmap = CreateDIBSection(Source, &Info, DIB_RGB_COLORS, &Bits, NULL, 0);
    Previous = SelectObject(Memory, Bitmap);
    BitBlt(Memory, 0, 0, W, H, Source, X, Y, SRCCOPY);
    GdiFlush();

    memset(&FileHeader, 0, sizeof(FileHeader));
    FileHeader.bfType = Bitmap_Signature;
    FileHeader.bfOffBits = sizeof(FileHeader) + sizeof(Info.bmiHeader);
    FileHeader.bfSize = FileHeader.bfOffBits + (DWORD)(Stride * H);
    File = fopen(FileName, "wb");
    if (File) {
        fwrite(&FileHeader, sizeof(FileHeader), 1, File);
        fwrite(&Info.bmiHeader, sizeof(Info.bmiHeader), 1, File);
        fwrite(Bits, 1, (usize)(Stride * H), File);
        fclose(File);
    }
    SelectObject(Memory, Previous);
    DeleteObject(Bitmap);
    DeleteDC(Memory);
}

static void Bitmap_SaveScreenArea(const RECT *Area, const c8 *FileName)
{
    HDC Screen = GetDC(NULL);

    Bitmap_Save(Screen, Area->left, Area->top, Area->right - Area->left, Area->bottom - Area->top, FileName);
    ReleaseDC(NULL, Screen);
}

static void Bitmap_SaveWindow(HWND Window, const c8 *FileName)
{
    RECT Area;

    GetWindowRect(Window, &Area);
    Bitmap_SaveScreenArea(&Area, FileName);
}

static void Bitmap_SaveEditor(HWND Editor, const c8 *FileName)
{
    HDC DC = GetDC(Editor);

    RedrawWindow(Editor, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    Bitmap_Save(DC, 0, 0, Layout_PanelW, Layout_MaxH, FileName);
    ReleaseDC(Editor, DC);
}

static void Keys_Press(WORD Key)
{
    keybd_event((BYTE)Key, 0, 0, 0);
    keybd_event((BYTE)Key, 0, KEYEVENTF_KEYUP, 0);
    Sleep(Wait_KeyMs);
}

static bool Menus_VisibleArea(RECT *Area)
{
    HWND Menu = NULL;
    bool Found = false;

    while ((Menu = FindWindowExA(NULL, Menu, Test_MenuClass, NULL)) != NULL) {
        RECT MenuArea;

        if (!IsWindowVisible(Menu)) {
            continue;
        }
        GetWindowRect(Menu, &MenuArea);
        if (Found) {
            UnionRect(Area, Area, &MenuArea);
        } else {
            *Area = MenuArea;
        }
        Found = true;
    }
    return Found;
}

static DWORD WINAPI Menus_Driver(LPVOID Unused)
{
    RECT Area;

    (void)Unused;
    Sleep(Wait_MenuOpenMs);
    for (i32 Pair = 0; Pair < Test_MenuKeyPairs; ++Pair) {
        Keys_Press(VK_DOWN);
        Keys_Press(VK_RIGHT);
    }
    Sleep(Wait_MenuSettleMs);
    if (Menus_VisibleArea(&Area)) {
        Bitmap_SaveScreenArea(&Area, Test_MenuShot);
    } else {
        printf("no menu window found\n");
    }
    for (i32 Press = 0; Press < Menu_EscapeCount; ++Press) {
        Keys_Press(VK_ESCAPE);
    }
    return 0;
}

static void Menus_Capture(HWND Editor, TSpot Spot, const c8 *FileName, i32 KeyPairs)
{
    HANDLE Driver;

    Test_MenuShot = FileName;
    Test_MenuKeyPairs = KeyPairs;
    Driver = CreateThread(NULL, 0, Menus_Driver, NULL, 0, NULL);
    SendMessageA(Editor, WM_RBUTTONUP, 0, Test_PointParam(Spot));
    WaitForSingleObject(Driver, Wait_MenuDriverMs);
    CloseHandle(Driver);
}

static HWND Explorer_WaitForWindow()
{
    HWND Window = NULL;

    for (i32 Step = 0; Step < Wait_ExplorerSteps && !Window; ++Step) {
        Sleep(Wait_ExplorerStepMs);
        Window = FindWindowA(Explorer_ClassName, NULL);
    }
    return Window;
}

static DWORD WINAPI Explorer_Driver(LPVOID Unused)
{
    HWND Window = Explorer_WaitForWindow();
    TSpot NameTitle = { (Layout_ExplorerTitleRight[1] + Layout_ExplorerTitleRight[2]) / 2,
                        (Layout_ExplorerTitlesTop + Layout_ExplorerList.Y) / 2 };

    (void)Unused;
    if (!Window) {
        printf("explorer did not open\n");
        return 0;
    }
    Sleep(Wait_ExplorerSettleMs);
    Bitmap_SaveWindow(Window, Test_ExplorerShots[0]);
    for (const c8 *Symbol = Test_Query; *Symbol; ++Symbol) {
        PostMessageA(Window, WM_CHAR, (WPARAM)(u8)*Symbol, 0);
    }
    Sleep(Wait_QuerySettleMs);
    Bitmap_SaveWindow(Window, Test_ExplorerShots[1]);
    PostMessageA(Window, WM_LBUTTONDOWN, 0, Test_PointParam(NameTitle));
    Sleep(Wait_ExplorerSettleMs);
    Bitmap_SaveWindow(Window, Test_ExplorerShots[2]);
    PostMessageA(Window, WM_KEYDOWN, VK_RETURN, 0);
    return 0;
}

static f64 Audio_Render(TFruityPlug *Plug, i32 Blocks, FILE *Output)
{
    static f32 Buffer[Test_BlockFrames * Test_Channels];
    f64 Peak = 0;

    for (i32 Block = 0; Block < Blocks; ++Block) {
        int Length = Test_BlockFrames;

        memset(Buffer, 0, sizeof(Buffer));
        Plug->Gen_Render((PWAV32FS)Buffer, Length);
        for (usize Index = 0; Index < ARRAY_COUNT(Buffer); ++Index) {
            if (fabs(Buffer[Index]) > Peak) {
                Peak = fabs(Buffer[Index]);
            }
        }
        if (Output) {
            fwrite(Buffer, sizeof(Buffer), 1, Output);
        }
    }
    return Peak;
}

static void Test_FillVoice(TVoiceParams_Old *Voice)
{
    memset(Voice, 0, sizeof(*Voice));
    Voice->InitLevels.Vol = Test_NoteVolume;
    Voice->InitLevels.Pitch = Test_NotePitch;
    Voice->FinalLevels = Voice->InitLevels;
    Voice->FinalLevels.Pitch = 0;
}

static void Test_PlayNote(TFruityPlug *Plug, TVoiceParams_Old *Voice)
{
    FILE *Output = fopen(Test_AudioFile, "wb");
    TVoiceHandle Handle = Plug->TriggerVoice((PVoiceParams)Voice, Test_FirstVoiceTag);

    printf("voice=%p\n", (void *)Handle);
    printf("peak while held: %.4f\n", Audio_Render(Plug, Test_HeldBlocks, Output));
    Plug->ProcessParam(Param_ReverbSend, Test_ReverbSendValue, REC_UpdateValue);
    Plug->ProcessParam(Param_Cutoff, Test_CutoffValue, REC_UpdateValue);
    printf("peak with cutoff %d: %.4f\n", Test_CutoffValue, Audio_Render(Plug, Test_FilteredBlocks, Output));
    if (Handle != (TVoiceHandle)-1) {
        Plug->Voice_Release(Handle);
    }
    printf("peak after release: %.4f, killed=%d\n", Audio_Render(Plug, Test_ReleaseBlocks, Output), Test_KilledVoices);
    if (Output) {
        fclose(Output);
    }
}

static IStream *Test_SaveState(TFruityPlug *Plug)
{
    IStream *Stream = NULL;
    LARGE_INTEGER Start;
    STATSTG Stat;

    CreateStreamOnHGlobal(NULL, TRUE, &Stream);
    Plug->SaveRestoreState(Stream, TRUE);
    Stream->Stat(&Stat, STATFLAG_NONAME);
    printf("state size=%u\n", (u32)Stat.cbSize.QuadPart);
    Start.QuadPart = 0;
    Stream->Seek(Start, STREAM_SEEK_SET, NULL);
    return Stream;
}

static void Test_RestoreAndPlay(TFruityPlug *Plug, IStream *State, TVoiceParams_Old *Voice)
{
    TVoiceHandle Handle;

    Plug->SaveRestoreState(State, FALSE);
    printf("restored param12=%d param2=%d\n", Plug->ProcessParam(Param_Cutoff, 0, REC_GetValue),
           Plug->ProcessParam(Param_ReverbSend, 0, REC_GetValue));
    Handle = Plug->TriggerVoice((PVoiceParams)Voice, Test_SecondVoiceTag);
    printf("second instance peak: %.4f\n", Audio_Render(Plug, Test_SecondBlocks, NULL));
    Plug->Voice_Kill(Handle);
}

static HWND Test_CreateParent()
{
    WNDCLASSA Class;

    memset(&Class, 0, sizeof(Class));
    Class.lpfnWndProc = DefWindowProcA;
    Class.hInstance = GetModuleHandleA(NULL);
    Class.lpszClassName = Test_ParentClass;
    RegisterClassA(&Class);
    return CreateWindowExA(WS_EX_TOOLWINDOW, Test_ParentClass, "", WS_POPUP | WS_VISIBLE, 0, 0, Layout_PanelW, Layout_MaxH,
                           NULL, NULL, Class.hInstance, NULL);
}

static void Test_Menus(HWND Parent, HWND Editor)
{
    SetWindowPos(Parent, HWND_TOPMOST, Test_MenuParentSpot.X, Test_MenuParentSpot.Y, 0, 0, SWP_NOSIZE);
    SetForegroundWindow(Parent);
    Menus_Capture(Editor, Test_Center(&Layout_PatchName), Test_PatchMenuShot, 2);
    Menus_Capture(Editor, Test_Center(&Layout_Controls[Param_LfoDelay].Box), Test_KnobMenuShot, 0);
    Menus_Capture(Editor, Test_Center(&Layout_SoundfontName), Test_SoundfontMenuShot, 0);
}

static void Test_Explorer(TFruityPlug *Plug, HWND Parent, const c8 *Query)
{
    HANDLE Driver;

    Test_Query = Query;
    SetWindowPos(Parent, HWND_TOPMOST, Test_ExplorerParentSpot.X, Test_ExplorerParentSpot.Y, 0, 0, SWP_NOSIZE);
    Driver = CreateThread(NULL, 0, Explorer_Driver, NULL, 0, NULL);
    SendMessageA(Plug->EditorHandle, WM_LBUTTONDOWN, 0, Test_PointParam(Test_Center(&Layout_PatchName)));
    CloseHandle(Driver);
    printf("after explorer: patch=%d bank=%d\n", Plug->ProcessParam(Param_Patch, 0, REC_GetValue),
           Plug->ProcessParam(Param_Bank, 0, REC_GetValue));
}

static void Test_Editor(TFruityPlug *Plug, const c8 *Mode)
{
    HWND Parent = Test_CreateParent();

    Plug->Dispatcher(FPD_ShowEditor, 0, (intptr_t)Parent);
    printf("editor=%p\n", (void *)Plug->EditorHandle);
    UpdateWindow(Parent);
    if (Plug->EditorHandle) {
        Bitmap_SaveEditor(Plug->EditorHandle, Test_EditorShot);
    }
    if (Plug->EditorHandle && Mode && !strcmp(Mode, Test_MenusMode)) {
        Test_Menus(Parent, Plug->EditorHandle);
    } else if (Plug->EditorHandle && Mode) {
        Test_Explorer(Plug, Parent, Mode);
    }
    Plug->Dispatcher(FPD_ShowEditor, 0, 0);
    DestroyWindow(Parent);
}

static void Test_Run(TCreatePlug Create, const c8 *Soundfont, const c8 *Mode)
{
    TVoiceParams_Old Voice;
    c8 Name[MAX_PATH];
    TFruityPlug *First = Create(&Test_Host, Test_FirstTag);
    TFruityPlug *Second;
    IStream *State;

    printf("created %p name=%s flags=%x params=%d\n", (void *)First, First->Info->LongName, First->Info->Flags,
           First->Info->NumParams);
    First->Dispatcher(FPD_SetSampleRate, 0, Test_SampleRate);
    printf("loadfile=%lld\n", (i64)First->Dispatcher(FPD_LoadFile, 0, (intptr_t)Soundfont));
    First->GetName(FPN_Param, Test_NamedParam, 0, Name);
    printf("param%d=%s value=%d\n", Test_NamedParam, Name, First->ProcessParam(Test_NamedParam, 0, REC_GetValue));

    Test_FillVoice(&Voice);
    Test_PlayNote(First, &Voice);
    State = Test_SaveState(First);
    Second = Create(&Test_Host, Test_SecondTag);
    Test_RestoreAndPlay(Second, State, &Voice);
    Test_Editor(Second, Mode);
    State->Release();
    Second->DestroyObject();
    First->DestroyObject();
}

int main(int ArgCount, char **Args)
{
    HMODULE Module;
    TCreatePlug Create;

    if (ArgCount < Arg_Required) {
        printf("usage: host_test <plugin dll> <soundfont> [search text | menus]\n");
        return Exit_Usage;
    }
    Module = LoadLibraryA(Args[Arg_Plugin]);
    if (!Module) {
        printf("LoadLibrary failed %lu\n", GetLastError());
        return Exit_LoadFailed;
    }
    Create = (TCreatePlug)GetProcAddress(Module, Test_EntryPoint);
    Test_Host.HostVersion = Test_HostVersion;
    Test_Run(Create, Args[Arg_Soundfont], ArgCount > Arg_Mode ? Args[Arg_Mode] : NULL);
    printf("done\n");
    return Exit_Ok;
}
