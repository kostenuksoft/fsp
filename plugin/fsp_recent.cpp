#include "fsp_recent.h"

#include <stdio.h>
#include <string.h>

#include "fsp_gdi.h"

const c8 Recent_SettingsKey[] = "Software\\FruitySoundfontPlayerX64";
const c8 Recent_ValueFormat[] = "Recent%u";
const u32 Recent_ValueNameSize = 16;

static void Recent_ValueName(u32 Index, c8 *Name, usize NameSize)
{
    snprintf(Name, NameSize, Recent_ValueFormat, Index);
}

static bool Recent_ReadPath(HKEY Key, u32 Index, c8 *Path, DWORD PathSize)
{
    c8 Name[Recent_ValueNameSize];
    DWORD Size = PathSize;
    DWORD Type = 0;

    Recent_ValueName(Index, Name, sizeof(Name));
    if (RegQueryValueExA(Key, Name, NULL, &Type, (BYTE *)Path, &Size) != ERROR_SUCCESS) {
        return false;
    }
    if (Type != REG_SZ) {
        return false;
    }
    Path[PathSize - 1] = 0;
    return true;
}

void Recent_Load(PRecentList List)
{
    TRegKey Key = { NULL };

    List->Count = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, Recent_SettingsKey, 0, KEY_READ, &Key.Handle) != ERROR_SUCCESS) {
        Key.Handle = NULL;
        return;
    }
    for (u32 Index = 0; Index < Recent_MaxCount; ++Index) {
        c8 *Path = List->Paths[List->Count];

        if (!Recent_ReadPath(Key.Handle, Index, Path, Text_PathSize)) {
            break;
        }
        if (!Text_IsEmpty(Path)) {
            ++List->Count;
        }
    }
}

static void Recent_Remove(PRecentList List, u32 Index)
{
    u32 Following = List->Count - Index - 1;

    memmove(List->Paths[Index], List->Paths[Index + 1], Following * sizeof(List->Paths[0]));
    --List->Count;
}

static void Recent_PutFirst(PRecentList List, const c8 *FileName)
{
    u32 Index = 0;

    while (Index < List->Count) {
        if (Text_SameNoCase(List->Paths[Index], FileName)) {
            Recent_Remove(List, Index);
        } else {
            ++Index;
        }
    }
    if (List->Count == Recent_MaxCount) {
        --List->Count;
    }
    memmove(List->Paths[1], List->Paths[0], List->Count * sizeof(List->Paths[0]));
    Text_Copy(List->Paths[0], sizeof(List->Paths[0]), FileName);
    ++List->Count;
}

static void Recent_Store(const TRecentList *List)
{
    TRegKey Key = { NULL };
    c8 Name[Recent_ValueNameSize];

    if (RegCreateKeyExA(HKEY_CURRENT_USER, Recent_SettingsKey, 0, NULL, 0, KEY_WRITE, NULL, &Key.Handle, NULL) !=
        ERROR_SUCCESS) {
        Key.Handle = NULL;
        return;
    }
    for (u32 Index = 0; Index < Recent_MaxCount; ++Index) {
        Recent_ValueName(Index, Name, sizeof(Name));
        if (Index < List->Count) {
            const c8 *Path = List->Paths[Index];

            RegSetValueExA(Key.Handle, Name, 0, REG_SZ, (const BYTE *)Path, (DWORD)strlen(Path) + 1);
        } else {
            RegDeleteValueA(Key.Handle, Name);
        }
    }
}

void Recent_Add(const c8 *FileName)
{
    TRecentList List;

    Recent_Load(&List);
    Recent_PutFirst(&List, FileName);
    Recent_Store(&List);
}
