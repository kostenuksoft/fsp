#ifndef FSP_DEBUGLOG_H
#define FSP_DEBUGLOG_H

#include "fsp_def.h"

void DebugLog_Write(const c8 *Format, ...);
bool DebugLog_TraceEnabled();

#endif
