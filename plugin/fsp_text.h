#ifndef FSP_TEXT_H
#define FSP_TEXT_H

#include "fsp_def.h"

const u32 Text_PathSize = 2 * MAX_PATH;
const u32 Text_NameSize = 256;
const u32 Text_LabelSize = 320;
const u32 Text_NumberSize = 16;

void Text_Copy(c8 *Dest, usize DestSize, const c8 *Source);
void Text_Append(c8 *Dest, usize DestSize, const c8 *Source);
void Text_Lower(c8 *Text);
void Text_Trim(c8 *Text);
bool Text_IsEmpty(const c8 *Text);
bool Text_Same(const c8 *Left, const c8 *Right);
bool Text_SameNoCase(const c8 *Left, const c8 *Right);
bool Text_Contains(const c8 *Text, const c8 *Part);
bool Text_HasExt(const c8 *FileName, const c8 *Ext);
void Text_BaseName(const c8 *Path, c8 *Dest, usize DestSize);
void Text_DirName(const c8 *Path, c8 *Dest, usize DestSize);

#endif
