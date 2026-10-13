#include "fsp_debuglog.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "fsp_text.h"

const c8 DebugLog_MarkerName[] = "fsp_x64_debug";
const c8 DebugLog_FileName[] = "fsp_x64.log";
const c8 DebugLog_TraceEnv[] = "FSP_LOG";
const c8 DebugLog_LineEnd = '\n';

enum {
    DebugLog_Unknown,
    DebugLog_Off,
    DebugLog_On
};

static i32 DebugLog_State = DebugLog_Unknown;
static c8 DebugLog_Path[Text_PathSize];

static void DebugLog_Detect()
{
    c8 Marker[Text_PathSize];
    DWORD Length = GetTempPathA(MAX_PATH, DebugLog_Path);

    DebugLog_State = DebugLog_Off;
    if (!Length || Length >= MAX_PATH) {
        return;
    }
    Text_Copy(Marker, sizeof(Marker), DebugLog_Path);
    Text_Append(Marker, sizeof(Marker), DebugLog_MarkerName);
    Text_Append(DebugLog_Path, sizeof(DebugLog_Path), DebugLog_FileName);
    if (GetFileAttributesA(Marker) != INVALID_FILE_ATTRIBUTES) {
        DebugLog_State = DebugLog_On;
    }
}

void DebugLog_Write(const c8 *Format, ...)
{
    FILE *File;
    va_list List;

    if (DebugLog_State == DebugLog_Unknown) {
        DebugLog_Detect();
    }
    if (DebugLog_State != DebugLog_On) {
        return;
    }
    File = fopen(DebugLog_Path, "a");
    if (!File) {
        return;
    }
    va_start(List, Format);
    vfprintf(File, Format, List);
    va_end(List);
    fputc(DebugLog_LineEnd, File);
    fclose(File);
}

bool DebugLog_TraceEnabled()
{
    return getenv(DebugLog_TraceEnv) != NULL;
}
