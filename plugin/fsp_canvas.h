#ifndef FSP_CANVAS_H
#define FSP_CANVAS_H

#include "fsp_def.h"

const i32 Canvas_Opaque = 255;

typedef struct {
    HBITMAP Bitmap;
    u32 *Pixels;
    i32 W;
    i32 H;
} TCanvas, *PCanvas;

typedef struct {
    const u32 *Pixels;
    i32 StripW;
    i32 CellW;
    i32 CellH;
} TGlyph, *PGlyph;

bool Canvas_Create(PCanvas Canvas, HDC DC, i32 W, i32 H);
void Canvas_Destroy(PCanvas Canvas);
void Canvas_Blit(PCanvas Canvas, const TGlyph *Glyph, i32 Cell, i32 X, i32 Y, i32 Opacity);
void Canvas_BlitAtFraction(PCanvas Canvas, const TGlyph *Glyph, i32 Cell, i32 X, f64 Y);
void Canvas_Line(PCanvas Canvas, f64 X0, f64 Y0, f64 X1, f64 Y1, f64 HalfW, u32 Color);

#endif
