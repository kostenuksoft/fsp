#ifndef FSP_ACCOUNT_H
#define FSP_ACCOUNT_H

#include "fsp_def.h"

const u32 Account_NameSize = 128;

void Account_GetName(c8 *Dest, usize DestSize);

#endif
