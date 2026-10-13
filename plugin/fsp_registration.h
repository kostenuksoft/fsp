#ifndef FSP_REGISTRATION_H
#define FSP_REGISTRATION_H

#include "fsp_def.h"

#include "sdk/fp_plugclass.h"

enum {
    Registration_NotChecked = -2,
    Registration_Unknown = -1,
    Registration_Registered = 0,
    Registration_Demo = 1
};

i32 Registration_Check(TFruityPlugHost *PlugHost, TPluginTag HostTag, int *PlugFlags);

#endif
