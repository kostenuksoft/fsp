#include "fsp_text.h"

#include <stdio.h>
#include <string.h>

const c8 Text_PathSeparators[] = "\\/";
const c8 Text_ExtSeparator = '.';
const c8 Text_Blanks[] = " \t";

void Text_Copy(c8 *Dest, usize DestSize, const c8 *Source)
{
    snprintf(Dest, DestSize, "%s", Source ? Source : "");
}

void Text_Append(c8 *Dest, usize DestSize, const c8 *Source)
{
    usize Length = strlen(Dest);

    if (Length < DestSize) {
        Text_Copy(Dest + Length, DestSize - Length, Source);
    }
}

void Text_Lower(c8 *Text)
{
    CharLowerBuffA(Text, (DWORD)strlen(Text));
}

void Text_Trim(c8 *Text)
{
    usize Skip = strspn(Text, Text_Blanks);
    usize Length = strlen(Text + Skip);

    memmove(Text, Text + Skip, Length + 1);
    while (Length > 0 && strchr(Text_Blanks, Text[Length - 1])) {
        --Length;
        Text[Length] = 0;
    }
}

bool Text_IsEmpty(const c8 *Text)
{
    return !Text || Text[0] == 0;
}

bool Text_Same(const c8 *Left, const c8 *Right)
{
    return strcmp(Left, Right) == 0;
}

bool Text_SameNoCase(const c8 *Left, const c8 *Right)
{
    return _stricmp(Left, Right) == 0;
}

bool Text_Contains(const c8 *Text, const c8 *Part)
{
    return strstr(Text, Part) != NULL;
}

bool Text_HasExt(const c8 *FileName, const c8 *Ext)
{
    const c8 *Dot = strrchr(FileName, Text_ExtSeparator);

    return Dot && Text_SameNoCase(Dot, Ext);
}

static const c8 *Text_AfterLastSeparator(const c8 *Path)
{
    const c8 *Name = Path;
    const c8 *Cursor;

    for (Cursor = Path; *Cursor; ++Cursor) {
        if (strchr(Text_PathSeparators, *Cursor)) {
            Name = Cursor + 1;
        }
    }
    return Name;
}

void Text_BaseName(const c8 *Path, c8 *Dest, usize DestSize)
{
    c8 *Dot;

    Text_Copy(Dest, DestSize, Text_AfterLastSeparator(Path));
    Dot = strrchr(Dest, Text_ExtSeparator);
    if (Dot) {
        *Dot = 0;
    }
}

void Text_DirName(const c8 *Path, c8 *Dest, usize DestSize)
{
    const c8 *Name = Text_AfterLastSeparator(Path);
    usize Length = Name > Path ? (usize)(Name - Path) - 1 : 0;

    if (Length >= DestSize) {
        Length = DestSize - 1;
    }
    memcpy(Dest, Path, Length);
    Dest[Length] = 0;
}
