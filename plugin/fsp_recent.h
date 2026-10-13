#ifndef FSP_RECENT_H
#define FSP_RECENT_H

#include "fsp_def.h"
#include "fsp_text.h"

const u32 Recent_MaxCount = 16;

typedef struct {
    c8 Paths[Recent_MaxCount][Text_PathSize];
    u32 Count;
} TRecentList, *PRecentList;

void Recent_Load(PRecentList List);
void Recent_Add(const c8 *FileName);

#endif
