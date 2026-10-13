#include "fsp_registration.h"

#include <stdio.h>
#include <string.h>

#include "fsp_gdi.h"
#include "fsp_main.h"
#include "fsp_regprobe.h"
#include "fsp_text.h"

const c8 Original_DllName[] = "Fruity Soundfont Player (original).dll";
const c8 Original_DllNameInPlace[] = "Fruity Soundfont Player.dll";
const i32 Original_ProductCode = 0x90a6;
const u32 Original_CheckOffset = 0x107b5c;
const DWORD Original_FileSize = 1775200;
const u8 Original_CheckPrologue[] = { 0x55, 0x8B, 0xEC, 0x51, 0xB9, 0x15, 0x00, 0x00, 0x00 };

static bool Registration_FileExists(const c8 *Path)
{
    return GetFileAttributesA(Path) != INVALID_FILE_ATTRIBUTES;
}

#ifdef _WIN64

const c8 Probe_ShellPath[] = "\\WindowsPowerShell\\v1.0\\powershell.exe";
const c8 Probe_CommandFormat[] = "\"%s%s\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ";
const c8 Probe_ScriptPrefixFormat[] = "$Dll = '%s'\n";
const c8 Probe_Quote = '\'';
const c8 Probe_RegisteredMark[] = "demo=NO";
const c8 Probe_DemoMark[] = "demo=YES";
const u32 Probe_ScriptSize = 8192;
const u32 Probe_CommandSize = 32767;
const u32 Probe_OutputSize = 2048;
const DWORD Probe_TimeoutMs = 30000;
const DWORD Probe_PollMs = 50;

const c8 Base64_Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const c8 Base64_Pad = '=';
const u32 Base64_GroupBytes = 3;
const u32 Base64_GroupSymbols = 4;
const u32 Base64_SymbolBits = 6;
const u32 Base64_SymbolMask = 63;
const u32 Base64_ByteBits = 8;

static void Registration_OriginalPath(c8 *Path, usize PathSize)
{
    snprintf(Path, PathSize, "%s%s", Main_Directory(), Original_DllName);
    if (!Registration_FileExists(Path)) {
        snprintf(Path, PathSize, "%s%s", Main_Directory(), Original_DllNameInPlace);
    }
}

static bool Base64_Append(c8 *Dest, usize DestSize, const u8 *Data, usize DataSize)
{
    usize Length = strlen(Dest);

    for (usize Index = 0; Index < DataSize; Index += Base64_GroupBytes) {
        u32 Group = 0;
        usize Present = DataSize - Index < Base64_GroupBytes ? DataSize - Index : Base64_GroupBytes;

        if (Length + Base64_GroupSymbols >= DestSize) {
            return false;
        }
        for (u32 Byte = 0; Byte < Base64_GroupBytes; ++Byte) {
            Group <<= Base64_ByteBits;
            if (Byte < Present) {
                Group |= Data[Index + Byte];
            }
        }
        for (u32 Symbol = 0; Symbol < Base64_GroupSymbols; ++Symbol) {
            u32 Shift = (Base64_GroupSymbols - 1 - Symbol) * Base64_SymbolBits;

            Dest[Length] = Symbol <= Present ? Base64_Alphabet[(Group >> Shift) & Base64_SymbolMask] : Base64_Pad;
            ++Length;
        }
    }
    Dest[Length] = 0;
    return true;
}

static bool Probe_BuildScript(const c8 *DllPath, c8 *Script, usize ScriptSize)
{
    c8 Quoted[2 * Text_PathSize];
    usize Length = 0;
    i32 Written;

    for (const c8 *Cursor = DllPath; *Cursor && Length + 2 < sizeof(Quoted); ++Cursor) {
        Quoted[Length] = *Cursor;
        ++Length;
        if (*Cursor == Probe_Quote) {
            Quoted[Length] = *Cursor;
            ++Length;
        }
    }
    Quoted[Length] = 0;
    Written = snprintf(Script, ScriptSize, Probe_ScriptPrefixFormat, Quoted);
    if (Written < 0 || (usize)Written + RegProbe_ScriptSize > ScriptSize) {
        return false;
    }
    Text_Append(Script, ScriptSize, RegProbe_Script);
    return true;
}

static bool Probe_BuildCommand(const c8 *Script, c8 *Command, usize CommandSize)
{
    static WCHAR Wide[Probe_ScriptSize];
    c8 SystemDir[MAX_PATH];
    i32 ScriptLength = (i32)strlen(Script);
    i32 WideLength;

    if (!GetSystemWow64DirectoryA(SystemDir, sizeof(SystemDir))) {
        return false;
    }
    WideLength = MultiByteToWideChar(CP_ACP, 0, Script, ScriptLength, Wide, Probe_ScriptSize);
    if (WideLength <= 0) {
        return false;
    }
    snprintf(Command, CommandSize, Probe_CommandFormat, SystemDir, Probe_ShellPath);
    return Base64_Append(Command, CommandSize, (const u8 *)Wide, (usize)WideLength * sizeof(WCHAR));
}

static void Probe_Drain(HANDLE Pipe, c8 *Output, usize OutputSize)
{
    usize Length = strlen(Output);
    DWORD Available = 0;
    DWORD Got = 0;

    while (PeekNamedPipe(Pipe, NULL, 0, NULL, &Available, NULL) && Available && Length + 1 < OutputSize) {
        if (!ReadFile(Pipe, Output + Length, (DWORD)(OutputSize - Length - 1), &Got, NULL) || !Got) {
            break;
        }
        Length += Got;
        Output[Length] = 0;
    }
}

static void Probe_Collect(HANDLE Process, HANDLE Pipe, c8 *Output, usize OutputSize)
{
    DWORD Started = GetTickCount();

    Output[0] = 0;
    for (;;) {
        Probe_Drain(Pipe, Output, OutputSize);
        if (WaitForSingleObject(Process, Probe_PollMs) == WAIT_OBJECT_0) {
            Probe_Drain(Pipe, Output, OutputSize);
            return;
        }
        if (GetTickCount() - Started > Probe_TimeoutMs) {
            TerminateProcess(Process, 1);
            return;
        }
    }
}

static bool Probe_Run(c8 *Command, c8 *Output, usize OutputSize)
{
    SECURITY_ATTRIBUTES Security;
    STARTUPINFOA Startup;
    PROCESS_INFORMATION Started;
    TFileHandle ReadEnd = { NULL };
    TFileHandle WriteEnd = { NULL };

    memset(&Security, 0, sizeof(Security));
    Security.nLength = sizeof(Security);
    Security.bInheritHandle = TRUE;
    if (!CreatePipe(&ReadEnd.Handle, &WriteEnd.Handle, &Security, 0)) {
        return false;
    }
    SetHandleInformation(ReadEnd.Handle, HANDLE_FLAG_INHERIT, 0);
    memset(&Startup, 0, sizeof(Startup));
    Startup.cb = sizeof(Startup);
    Startup.dwFlags = STARTF_USESTDHANDLES;
    Startup.hStdOutput = WriteEnd.Handle;
    Startup.hStdError = WriteEnd.Handle;
    if (!CreateProcessA(NULL, Command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, Main_Directory(), &Startup,
                        &Started)) {
        return false;
    }
    CloseHandle(WriteEnd.Handle);
    WriteEnd.Handle = NULL;
    Probe_Collect(Started.hProcess, ReadEnd.Handle, Output, OutputSize);
    CloseHandle(Started.hThread);
    CloseHandle(Started.hProcess);
    return true;
}

static i32 Registration_Probe()
{
    static c8 Script[Probe_ScriptSize];
    static c8 Command[Probe_CommandSize];
    c8 DllPath[Text_PathSize];
    c8 Output[Probe_OutputSize];

    Registration_OriginalPath(DllPath, sizeof(DllPath));
    if (!Registration_FileExists(DllPath)) {
        return Registration_Unknown;
    }
    if (!Probe_BuildScript(DllPath, Script, sizeof(Script)) || !Probe_BuildCommand(Script, Command, sizeof(Command))) {
        return Registration_Unknown;
    }
    if (!Probe_Run(Command, Output, sizeof(Output))) {
        return Registration_Unknown;
    }
    if (Text_Contains(Output, Probe_RegisteredMark)) {
        return Registration_Registered;
    }
    if (Text_Contains(Output, Probe_DemoMark)) {
        return Registration_Demo;
    }
    return Registration_Unknown;
}

i32 Registration_Check(TFruityPlugHost *PlugHost, TPluginTag HostTag, int *PlugFlags)
{
    static i32 Verdict = Registration_NotChecked;

    PlugHost->Dispatcher(HostTag, FHD_CheckProdCode, Original_ProductCode, (intptr_t)PlugFlags);
    if (Verdict == Registration_NotChecked) {
        Verdict = Registration_Probe();
    }
    return Verdict;
}

#else

static void Registration_CallOriginal(void *Check, TFruityPlugHost *PlugHost, TPluginTag HostTag, int *Flags)
{
    __asm {
        mov eax, PlugHost
        mov edx, HostTag
        mov ecx, Original_ProductCode
        push Flags
        call Check
    }
}

static bool Registration_IsOriginal(const c8 *Path)
{
    WIN32_FILE_ATTRIBUTE_DATA Data;

    if (!GetFileAttributesExA(Path, GetFileExInfoStandard, &Data)) {
        return false;
    }
    return Data.nFileSizeHigh == 0 && Data.nFileSizeLow == Original_FileSize;
}

static i32 Registration_AskOriginal(TFruityPlugHost *PlugHost, TPluginTag HostTag)
{
    static HMODULE Module;
    c8 Path[Text_PathSize];
    u8 *Check;
    int Flags = FPF_DemoVersion;

    snprintf(Path, sizeof(Path), "%s%s", Main_Directory(), Original_DllName);
    if (!Registration_FileExists(Path) || !Registration_IsOriginal(Path)) {
        return Registration_Unknown;
    }
    if (!Module) {
        Module = LoadLibraryA(Path);
    }
    if (!Module) {
        return Registration_Unknown;
    }
    Check = (u8 *)Module + Original_CheckOffset;
    if (IsBadReadPtr(Check, sizeof(Original_CheckPrologue))) {
        return Registration_Unknown;
    }
    if (memcmp(Check, Original_CheckPrologue, sizeof(Original_CheckPrologue)) != 0) {
        return Registration_Unknown;
    }
    Registration_CallOriginal(Check, PlugHost, HostTag, &Flags);
    return (Flags & FPF_DemoVersion) ? Registration_Demo : Registration_Registered;
}

i32 Registration_Check(TFruityPlugHost *PlugHost, TPluginTag HostTag, int *PlugFlags)
{
    static i32 Verdict = Registration_NotChecked;

    (void)PlugFlags;
    if (Verdict == Registration_NotChecked) {
        Verdict = Registration_AskOriginal(PlugHost, HostTag);
    }
    return Verdict;
}

#endif
