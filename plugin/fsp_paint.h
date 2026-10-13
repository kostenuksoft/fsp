#ifndef FSP_PAINT_H
#define FSP_PAINT_H

#include "fsp_def.h"

HFONT Paint_PanelFont();
void Paint_Shutdown();
void Paint_Text(HDC DC, const TBox *Box, const c8 *Text, COLORREF Color, UINT Format);
void Paint_Fill(HDC DC, const TBox *Box, COLORREF Color);
void Paint_Frame(HDC DC, HDC Artwork, const TBox *Box, i32 LeftW);
void Paint_ArtworkCopy(HDC DC, HDC Artwork, i32 X, i32 Y, const TBox *Source);
void Paint_ArtworkStretch(HDC DC, HDC Artwork, const TBox *Dest, const TBox *Source);
void Paint_Editor(TSoundfontPlug *Plug, HDC DC);

#endif
