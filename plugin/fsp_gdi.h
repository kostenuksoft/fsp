#ifndef FSP_GDI_H
#define FSP_GDI_H

#include "fsp_def.h"

struct TGdiObject {
    HGDIOBJ Handle;

    ~TGdiObject()
    {
        if (Handle) {
            DeleteObject(Handle);
        }
    }
};

struct TMemoryDC {
    HDC Handle;

    ~TMemoryDC()
    {
        if (Handle) {
            DeleteDC(Handle);
        }
    }
};

struct TScreenDC {
    HDC Handle;

    ~TScreenDC()
    {
        if (Handle) {
            ReleaseDC(NULL, Handle);
        }
    }
};

struct TSelection {
    HDC DC;
    HGDIOBJ Previous;

    ~TSelection()
    {
        if (Previous) {
            SelectObject(DC, Previous);
        }
    }
};

struct TRegKey {
    HKEY Handle;

    ~TRegKey()
    {
        if (Handle) {
            RegCloseKey(Handle);
        }
    }
};

struct TFileHandle {
    HANDLE Handle;

    ~TFileHandle()
    {
        if (Handle) {
            CloseHandle(Handle);
        }
    }
};

#endif
