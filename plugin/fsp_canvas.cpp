#include "fsp_canvas.h"

#include <math.h>
#include <string.h>

const u32 Pixel_ChannelMax = 255;
const u32 Pixel_AlphaShift = 24;
const u32 Pixel_RedShift = 16;
const u32 Pixel_GreenShift = 8;
const u32 Pixel_OpaqueAlpha = Pixel_ChannelMax << Pixel_AlphaShift;
const u16 Pixel_BitCount = 32;
const f64 Pixel_Half = 0.5;
const f64 Pixel_FullCover = 1.0;

static u32 Pixel_Alpha(u32 Pixel)
{
    return Pixel >> Pixel_AlphaShift;
}

static u32 Pixel_Red(u32 Pixel)
{
    return (Pixel >> Pixel_RedShift) & Pixel_ChannelMax;
}

static u32 Pixel_Green(u32 Pixel)
{
    return (Pixel >> Pixel_GreenShift) & Pixel_ChannelMax;
}

static u32 Pixel_Blue(u32 Pixel)
{
    return Pixel & Pixel_ChannelMax;
}

static u32 Pixel_Pack(u32 Red, u32 Green, u32 Blue)
{
    return Pixel_OpaqueAlpha | (Red << Pixel_RedShift) | (Green << Pixel_GreenShift) | Blue;
}

static u32 Pixel_Mix(u32 Source, u32 Dest, u32 Alpha)
{
    u32 Rest = Pixel_ChannelMax - Alpha;
    u32 Red = (Pixel_Red(Source) * Alpha + Pixel_Red(Dest) * Rest) / Pixel_ChannelMax;
    u32 Green = (Pixel_Green(Source) * Alpha + Pixel_Green(Dest) * Rest) / Pixel_ChannelMax;
    u32 Blue = (Pixel_Blue(Source) * Alpha + Pixel_Blue(Dest) * Rest) / Pixel_ChannelMax;

    return Pixel_Pack(Red, Green, Blue);
}

static bool Canvas_Contains(PCanvas Canvas, i32 X, i32 Y)
{
    return X >= 0 && X < Canvas->W && Y >= 0 && Y < Canvas->H;
}

static u32 *Canvas_At(PCanvas Canvas, i32 X, i32 Y)
{
    return &Canvas->Pixels[Y * Canvas->W + X];
}

static u32 Glyph_At(const TGlyph *Glyph, i32 Cell, i32 X, i32 Y)
{
    return Glyph->Pixels[Y * Glyph->StripW + Cell * Glyph->CellW + X];
}

bool Canvas_Create(PCanvas Canvas, HDC DC, i32 W, i32 H)
{
    BITMAPINFO Info;
    void *Bits = NULL;

    memset(&Info, 0, sizeof(Info));
    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = W;
    Info.bmiHeader.biHeight = -H;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = Pixel_BitCount;
    Canvas->Bitmap = CreateDIBSection(DC, &Info, DIB_RGB_COLORS, &Bits, NULL, 0);
    Canvas->Pixels = (u32 *)Bits;
    Canvas->W = W;
    Canvas->H = H;
    return Canvas->Bitmap != NULL;
}

void Canvas_Destroy(PCanvas Canvas)
{
    if (Canvas->Bitmap) {
        DeleteObject(Canvas->Bitmap);
    }
    Canvas->Bitmap = NULL;
    Canvas->Pixels = NULL;
}

void Canvas_Blit(PCanvas Canvas, const TGlyph *Glyph, i32 Cell, i32 X, i32 Y, i32 Opacity)
{
    if (!Canvas->Pixels || Opacity <= 0) {
        return;
    }
    for (i32 Row = 0; Row < Glyph->CellH; ++Row) {
        for (i32 Column = 0; Column < Glyph->CellW; ++Column) {
            u32 Source = Glyph_At(Glyph, Cell, Column, Row);
            u32 Alpha = Pixel_Alpha(Source) * (u32)Opacity / Pixel_ChannelMax;
            u32 *Dest;

            if (!Canvas_Contains(Canvas, X + Column, Y + Row)) {
                continue;
            }
            Dest = Canvas_At(Canvas, X + Column, Y + Row);
            *Dest = Pixel_Mix(Source, *Dest, Alpha);
        }
    }
}

static void Canvas_BlendWeighted(u32 *Dest, u32 Upper, u32 Lower, f64 Fraction)
{
    f64 UpperAlpha = (f64)Pixel_Alpha(Upper) * Fraction;
    f64 LowerAlpha = (f64)Pixel_Alpha(Lower) * (Pixel_FullCover - Fraction);
    f64 Alpha = UpperAlpha + LowerAlpha;
    f64 Red;
    f64 Green;
    f64 Blue;
    f64 Cover;
    u32 OutRed;
    u32 OutGreen;
    u32 OutBlue;

    if (Alpha <= 0.0) {
        return;
    }
    Red = (Pixel_Red(Upper) * UpperAlpha + Pixel_Red(Lower) * LowerAlpha) / Alpha;
    Green = (Pixel_Green(Upper) * UpperAlpha + Pixel_Green(Lower) * LowerAlpha) / Alpha;
    Blue = (Pixel_Blue(Upper) * UpperAlpha + Pixel_Blue(Lower) * LowerAlpha) / Alpha;
    Cover = Alpha / (f64)Pixel_ChannelMax;
    OutRed = (u32)(Red * Cover + Pixel_Red(*Dest) * (Pixel_FullCover - Cover) + Pixel_Half);
    OutGreen = (u32)(Green * Cover + Pixel_Green(*Dest) * (Pixel_FullCover - Cover) + Pixel_Half);
    OutBlue = (u32)(Blue * Cover + Pixel_Blue(*Dest) * (Pixel_FullCover - Cover) + Pixel_Half);
    *Dest = Pixel_Pack(OutRed, OutGreen, OutBlue);
}

void Canvas_BlitAtFraction(PCanvas Canvas, const TGlyph *Glyph, i32 Cell, i32 X, f64 Y)
{
    i32 Top = (i32)floor(Y);
    f64 Fraction = Y - Top;

    if (!Canvas->Pixels) {
        return;
    }
    for (i32 Row = 0; Row <= Glyph->CellH; ++Row) {
        for (i32 Column = 0; Column < Glyph->CellW; ++Column) {
            u32 Upper = Row > 0 ? Glyph_At(Glyph, Cell, Column, Row - 1) : 0;
            u32 Lower = Row < Glyph->CellH ? Glyph_At(Glyph, Cell, Column, Row) : 0;

            if (!Canvas_Contains(Canvas, X + Column, Top + Row)) {
                continue;
            }
            Canvas_BlendWeighted(Canvas_At(Canvas, X + Column, Top + Row), Upper, Lower, Fraction);
        }
    }
}

static f64 Line_Cover(f64 X, f64 Y, f64 X0, f64 Y0, f64 DeltaX, f64 DeltaY, f64 HalfW)
{
    f64 LengthSquared = DeltaX * DeltaX + DeltaY * DeltaY;
    f64 Along = LengthSquared > 0 ? ((X - X0) * DeltaX + (Y - Y0) * DeltaY) / LengthSquared : 0;
    f64 OffX;
    f64 OffY;

    Along = Along < 0 ? 0 : (Along > 1 ? 1 : Along);
    OffX = X - (X0 + Along * DeltaX);
    OffY = Y - (Y0 + Along * DeltaY);
    return HalfW + Pixel_Half - sqrt(OffX * OffX + OffY * OffY);
}

void Canvas_Line(PCanvas Canvas, f64 X0, f64 Y0, f64 X1, f64 Y1, f64 HalfW, u32 Color)
{
    i32 MinX = (i32)floor((X0 < X1 ? X0 : X1) - HalfW - 1);
    i32 MaxX = (i32)ceil((X0 > X1 ? X0 : X1) + HalfW + 1);
    i32 MinY = (i32)floor((Y0 < Y1 ? Y0 : Y1) - HalfW - 1);
    i32 MaxY = (i32)ceil((Y0 > Y1 ? Y0 : Y1) + HalfW + 1);

    if (!Canvas->Pixels) {
        return;
    }
    for (i32 Y = MinY; Y <= MaxY; ++Y) {
        for (i32 X = MinX; X <= MaxX; ++X) {
            f64 Cover;
            u32 Alpha;
            u32 *Dest;

            if (!Canvas_Contains(Canvas, X, Y)) {
                continue;
            }
            Cover = Line_Cover(X, Y, X0, Y0, X1 - X0, Y1 - Y0, HalfW);
            if (Cover <= 0) {
                continue;
            }
            Alpha = (u32)((Cover > Pixel_FullCover ? Pixel_FullCover : Cover) * (f64)Pixel_ChannelMax + Pixel_Half);
            Dest = Canvas_At(Canvas, X, Y);
            *Dest = Pixel_Mix(Color, *Dest, Alpha);
        }
    }
}
