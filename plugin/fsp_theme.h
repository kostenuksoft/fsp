#ifndef FSP_THEME_H
#define FSP_THEME_H

#include "fsp_def.h"

const c8 Theme_FontName[] = "Tahoma";
const i32 Theme_PanelFontHeight = -10;
const i32 Theme_MenuFontHeight = -11;

const COLORREF Theme_TextBright = RGB(0xbf, 0xd5, 0xd6);
const COLORREF Theme_TextHot = RGB(0xff, 0xff, 0xff);
const COLORREF Theme_TextDim = RGB(0x6f, 0x85, 0x86);
const COLORREF Theme_TextCredit = RGB(0x7f, 0xa3, 0xa6);
const COLORREF Theme_Accent = RGB(0xff, 0x98, 0x3d);
const COLORREF Theme_PanelFallback = RGB(0x6d, 0x74, 0x7b);

const u32 Theme_NeedlePixel = 0xb6c5c5;
const u32 Theme_NeedleDragPixel = 0xf04000;

const COLORREF Theme_ExplorerEdge = RGB(0x10, 0x16, 0x18);
const COLORREF Theme_ExplorerWell = RGB(0x06, 0x10, 0x12);
const COLORREF Theme_ExplorerSelected = RGB(0x2c, 0x44, 0x48);
const COLORREF Theme_ExplorerHover = RGB(0x16, 0x28, 0x2b);
const COLORREF Theme_ExplorerTrack = RGB(0x1c, 0x2e, 0x31);
const COLORREF Theme_ExplorerThumb = RGB(0xb6, 0xc5, 0xc5);
const COLORREF Theme_ExplorerFallback = RGB(0x59, 0x67, 0x6b);
const COLORREF Theme_ExplorerNumber = RGB(0x6f, 0x9a, 0x9c);

const COLORREF Theme_MenuBack = RGB(187, 196, 201);
const COLORREF Theme_MenuBorder = RGB(22, 31, 36);
const COLORREF Theme_MenuSeparator = RGB(147, 158, 164);
const COLORREF Theme_MenuHot = RGB(92, 108, 120);
const COLORREF Theme_MenuInk = RGB(30, 34, 46);
const COLORREF Theme_MenuInkHot = RGB(255, 255, 255);
const COLORREF Theme_MenuInkDisabled = RGB(132, 144, 151);
const COLORREF Theme_MenuHintInk = RGB(110, 122, 130);
const COLORREF Theme_MenuHintInkHot = RGB(220, 228, 232);
const COLORREF Theme_MenuArrow = RGB(104, 118, 128);

#endif
