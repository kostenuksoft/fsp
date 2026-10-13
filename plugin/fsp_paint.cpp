#include "fsp_paint.h"

#include <math.h>
#include <stdio.h>

#include "fsp_gdi.h"
#include "fsp_glyphs.h"
#include "fsp_plugclass.h"
#include "fsp_soundfont.h"
#include "fsp_theme.h"

const c8 Credit_Original[] = "Original plugin \251 Image-Line";
const c8 Credit_Rebuild[] = Plug_Arch " re-build by kostenuksoft";

const TGlyph Glyphs_Digits = { Glyph_Digits, Glyph_DigitsW, 7, Glyph_DigitsH };
const TGlyph Glyphs_Led = { Glyph_Led, Glyph_LedW, 5, Glyph_LedH };
const TGlyph Glyphs_Button = { Glyph_Btn, Glyph_BtnW, 31, Glyph_BtnH };
const TGlyph Glyphs_Slider = { Glyph_Slider, Glyph_SliderW, 18, Glyph_SliderH };
const TGlyph Glyphs_Logo = { Glyph_Logo, Glyph_LogoW, 112, Glyph_LogoH };
const TGlyph Glyphs_Info = { Glyph_Info, Glyph_InfoW, 17, Glyph_InfoH };
const TGlyph Glyphs_CreditMascot = { Glyph_ChanS, Glyph_ChanSW, Glyph_ChanSW, Glyph_ChanSH };
const i32 Mascot_CellW = 63;
const i32 Mascot_BodyH = 60;
const i32 Mascot_HandH = Glyph_ChanH - Mascot_BodyH;
const TGlyph Glyphs_MascotBody = { Glyph_Chan, Glyph_ChanW, Mascot_CellW, Mascot_BodyH };
const TGlyph Glyphs_MascotHand = { Glyph_Chan + Mascot_BodyH * Glyph_ChanW, Glyph_ChanW, Mascot_CellW, Mascot_HandH };

enum {
    Cell_Idle = 0,
    Cell_Active = 1,
    Cell_OpenIdle = 0,
    Cell_OpenPressed = 1,
    Cell_EditIdle = 2,
    Cell_EditPressed = 3,
    Cell_DigitBlank = 0,
    Cell_DigitZero = 1,
    Cell_DigitDash = 11
};

const c8 Digit_Format[] = "%*d";
const c8 Digit_First = '0';
const c8 Digit_Last = '9';
const c8 Digit_Minus = '-';

static HFONT Paint_PanelFontHandle;

HFONT Paint_PanelFont()
{
    if (!Paint_PanelFontHandle) {
        Paint_PanelFontHandle = CreateFontA(Theme_PanelFontHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                            NONANTIALIASED_QUALITY, 0, Theme_FontName);
    }
    return Paint_PanelFontHandle;
}

void Paint_Shutdown()
{
    if (Paint_PanelFontHandle) {
        DeleteObject(Paint_PanelFontHandle);
        Paint_PanelFontHandle = NULL;
    }
}

void Paint_Text(HDC DC, const TBox *Box, const c8 *Text, COLORREF Color, UINT Format)
{
    RECT Area = { Box->X, Box->Y, Box->X + Box->W, Box->Y + Box->H };

    SetTextColor(DC, Color);
    DrawTextA(DC, Text, -1, &Area, Format | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void Paint_Fill(HDC DC, const TBox *Box, COLORREF Color)
{
    RECT Area = { Box->X, Box->Y, Box->X + Box->W, Box->Y + Box->H };
    TGdiObject Brush = { CreateSolidBrush(Color) };

    FillRect(DC, &Area, (HBRUSH)Brush.Handle);
}

void Paint_ArtworkCopy(HDC DC, HDC Artwork, i32 X, i32 Y, const TBox *Source)
{
    BitBlt(DC, X, Y, Source->W, Source->H, Artwork, Source->X, Source->Y, SRCCOPY);
}

void Paint_ArtworkStretch(HDC DC, HDC Artwork, const TBox *Dest, const TBox *Source)
{
    StretchBlt(DC, Dest->X, Dest->Y, Dest->W, Dest->H, Artwork, Source->X, Source->Y, Source->W, Source->H, SRCCOPY);
}

static void Paint_FrameHalf(HDC DC, HDC Artwork, i32 X, i32 Y, i32 W, i32 H, i32 SourceX)
{
    TBox Top = { SourceX, Artwork_FrameTopY, W, Artwork_FrameEdgeH };
    TBox Middle = { SourceX, Artwork_FrameMiddleY, W, Artwork_FrameMiddleH };
    TBox Bottom = { SourceX, Artwork_FrameBottomY, W, Artwork_FrameEdgeH };
    TBox MiddleDest = { X, Y + Artwork_FrameEdgeH, W, H - 2 * Artwork_FrameEdgeH };

    Paint_ArtworkCopy(DC, Artwork, X, Y, &Top);
    Paint_ArtworkStretch(DC, Artwork, &MiddleDest, &Middle);
    Paint_ArtworkCopy(DC, Artwork, X, Y + H - Artwork_FrameEdgeH, &Bottom);
}

void Paint_Frame(HDC DC, HDC Artwork, const TBox *Box, i32 LeftW)
{
    i32 RightW = Box->W - LeftW;

    Paint_FrameHalf(DC, Artwork, Box->X, Box->Y, LeftW, Box->H, Artwork_FrameLeft);
    Paint_FrameHalf(DC, Artwork, Box->X + LeftW, Box->Y, RightW, Box->H, Artwork_FrameRight - RightW + 1);
}

static void Paint_Well(HDC DC, HDC Artwork, const TBox *Well)
{
    TBox Body = { Artwork_Well.X, Artwork_Well.Y, Well->W - 1, Well->H };
    TBox Edge = { Artwork_WellRightEdge.X, Artwork_WellRightEdge.Y, 1, Well->H };

    Paint_ArtworkCopy(DC, Artwork, Well->X, Well->Y, &Body);
    Paint_ArtworkCopy(DC, Artwork, Well->X + Well->W - 1, Well->Y, &Edge);
}

static void Paint_InfoBackground(HDC DC, HDC Artwork)
{
    TBox Area = { 0, Layout_BaseH, Layout_PanelW, Layout_InfoH };

    Paint_ArtworkStretch(DC, Artwork, &Area, &Artwork_PanelFill);
    Paint_ArtworkCopy(DC, Artwork, 0, Layout_BaseH, &Artwork_InfoTopEdge);
    Paint_ArtworkCopy(DC, Artwork, 0, Layout_MaxH - Artwork_PanelBottomEdge.H, &Artwork_PanelBottomEdge);
    Paint_Frame(DC, Artwork, &Layout_InfoFrame, Layout_InfoFrameLeftW);
}

static void Paint_Background(TSoundfontPlug *Plug, HDC DC)
{
    TMemoryDC Artwork = { CreateCompatibleDC(DC) };
    TSelection Selected = { Artwork.Handle, SelectObject(Artwork.Handle, Plug->Editor.Artwork) };
    TBox Head = { 0, 0, Artwork_Width, Layout_SplitY };
    TBox SeamSource = { 0, Layout_SplitY - 1, Artwork_Width, 1 };
    TBox SeamDest = { 0, Layout_SplitY, Layout_PanelW, Layout_Gap };
    TBox Rest = { 0, Layout_SplitY, Artwork_Width, Artwork_Height - Layout_SplitY };

    Paint_ArtworkCopy(DC, Artwork.Handle, 0, 0, &Head);
    Paint_ArtworkStretch(DC, Artwork.Handle, &SeamDest, &SeamSource);
    Paint_ArtworkCopy(DC, Artwork.Handle, 0, Layout_SplitY + Layout_Gap, &Rest);
    for (usize Index = 0; Index < ARRAY_COUNT(Layout_Wells); ++Index) {
        Paint_Well(DC, Artwork.Handle, &Layout_Wells[Index]);
    }
    if (Plug->Editor.InfoOpen) {
        Paint_InfoBackground(DC, Artwork.Handle);
    }
}

static void Paint_Digits(PCanvas Canvas, TSpot Spot, i32 Count, i32 Value, bool Blank)
{
    c8 Text[Text_NumberSize];

    snprintf(Text, sizeof(Text), Digit_Format, Count, Value);
    for (i32 Index = 0; Index < Count; ++Index) {
        c8 Symbol = Text[Index];
        i32 Cell = Cell_DigitBlank;

        if (Blank || Symbol == Digit_Minus) {
            Cell = Cell_DigitDash;
        } else if (Symbol >= Digit_First && Symbol <= Digit_Last) {
            Cell = Cell_DigitZero + (Symbol - Digit_First);
        }
        Canvas_Blit(Canvas, &Glyphs_Digits, Cell, Spot.X + Index * Glyphs_Digits.CellW, Spot.Y, Canvas_Opaque);
    }
}

static f64 Paint_ControlPosition(const TSoundfontPlug *Plug, i32 Control)
{
    const TControl *Def = &Layout_Controls[Control];
    f64 Value = Plug->ParamValues[Def->Param];
    f64 Position;

    if (Plug->Editor.Drag == Control) {
        Value = Plug->Editor.DragValue;
    }
    Position = (Value - Param_Defs[Def->Param].Min) / Param_Range(Def->Param);
    return Position < 0 ? 0 : (Position > 1 ? 1 : Position);
}

static void Paint_Slider(TSoundfontPlug *Plug, i32 Control)
{
    const TBox *Box = &Layout_Controls[Control].Box;
    bool Lit = Plug->Editor.Drag == Control || Plug->Editor.Hover == Control;
    f64 Travel = Box->H - Glyphs_Slider.CellH;
    f64 Y = Box->Y + (1.0 - Paint_ControlPosition(Plug, Control)) * Travel;

    Canvas_BlitAtFraction(&Plug->Editor.Canvas, &Glyphs_Slider, Lit ? Cell_Active : Cell_Idle, Box->X, Y);
}

static void Paint_Knob(TSoundfontPlug *Plug, i32 Control)
{
    const TBox *Box = &Layout_Controls[Control].Box;
    f64 Angle = Layout_KnobStartAngle - Layout_KnobSweepAngle * Paint_ControlPosition(Plug, Control);
    f64 CenterX = (Box->W - 1) * 0.5;
    f64 CenterY = (Box->H - 1) * 0.5;
    f64 ReachX = (CenterX + Layout_KnobRadiusPad) * cos(Angle);
    f64 ReachY = (CenterY + Layout_KnobRadiusPad) * sin(Angle);
    u32 Color = Plug->Editor.Drag == Control ? Theme_NeedleDragPixel : Theme_NeedlePixel;

    Canvas_Line(&Plug->Editor.Canvas, Box->X + CenterX + ReachX, Box->Y + CenterY - ReachY,
                Box->X + CenterX + ReachX * Layout_KnobNeedleInner, Box->Y + CenterY - ReachY * Layout_KnobNeedleInner,
                Layout_KnobNeedleHalfW, Color);
}

static void Paint_Controls(TSoundfontPlug *Plug)
{
    for (i32 Control = 0; Control < Layout_ControlCount; ++Control) {
        const TControl *Def = &Layout_Controls[Control];
        TSpot Digits = { Def->Box.X + Layout_ControlDigitsInset.X, Def->Box.Y + Layout_ControlDigitsInset.Y };

        if (Def->Kind == Control_Digits) {
            Paint_Digits(&Plug->Editor.Canvas, Digits, Layout_ControlDigitCount, Plug->ParamValues[Def->Param], false);
        } else if (Def->Kind == Control_Slider) {
            Paint_Slider(Plug, Control);
        } else {
            Paint_Knob(Plug, Control);
        }
    }
}

static void Paint_Led(TSoundfontPlug *Plug, TSpot Spot, i32 Led)
{
    PCanvas Canvas = &Plug->Editor.Canvas;

    Canvas_Blit(Canvas, &Glyphs_Led, Cell_Idle, Spot.X, Spot.Y, Canvas_Opaque);
    Canvas_Blit(Canvas, &Glyphs_Led, Cell_Active, Spot.X, Spot.Y, Plug->Editor.LedFade[Led]);
}

static void Paint_Glyphs(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;
    PCanvas Canvas = &Editor->Canvas;
    i32 OpenCell = Editor->Pressed == Hit_Open ? Cell_OpenPressed : Cell_OpenIdle;
    i32 EditCell = Editor->Pressed == Hit_Edit ? Cell_EditPressed : Cell_EditIdle;
    i32 ReverbTrack = Plug->Sends[Send_Reverb].Track;
    i32 ChorusTrack = Plug->Sends[Send_Chorus].Track;

    Canvas_Blit(Canvas, &Glyphs_Button, OpenCell, Layout_OpenButton.X, Layout_OpenButton.Y, Canvas_Opaque);
    Canvas_Blit(Canvas, &Glyphs_Button, EditCell, Layout_EditButton.X, Layout_EditButton.Y, Canvas_Opaque);
    Canvas_Blit(Canvas, &Glyphs_Logo, Cell_Idle, Layout_Logo.X, Layout_Logo.Y, Canvas_Opaque);
    Canvas_Blit(Canvas, &Glyphs_Logo, Cell_Active, Layout_Logo.X, Layout_Logo.Y, Editor->LogoFade);
    Paint_Led(Plug, Layout_ReverbLed, Led_Reverb);
    Paint_Led(Plug, Layout_ChorusLed, Led_Chorus);
    Paint_Led(Plug, Layout_QualityLed, Led_Quality);
    Paint_Digits(Canvas, Layout_ReverbSendDigits, Layout_SendDigitCount, ReverbTrack + 1, ReverbTrack < 0);
    Paint_Digits(Canvas, Layout_ChorusSendDigits, Layout_SendDigitCount, ChorusTrack + 1, ChorusTrack < 0);
    Paint_Controls(Plug);
}

static void Paint_Names(TSoundfontPlug *Plug, HDC DC)
{
    bool Hot = Plug->Editor.Hover == Hit_SoundfontName;

    Paint_Text(DC, &Layout_SoundfontText, Plug->BankName, Hot ? Theme_TextHot : Theme_TextBright, DT_LEFT);
    Paint_Text(DC, &Layout_PatchText, Plug->PatchName, Soundfont_HasPatch(Plug) ? Theme_Accent : Theme_TextDim,
               DT_LEFT);
}

static void Paint_Credits(TSoundfontPlug *Plug, HDC DC)
{
    TBox Line = { Layout_CreditX, Layout_CreditFirstY, Layout_CreditW, Layout_TextH };

    Paint_Text(DC, &Line, Credit_Original, Theme_Accent, DT_LEFT);
    Line.Y += Layout_CreditStepY;
    Paint_Text(DC, &Line, Credit_Rebuild, Theme_TextBright, DT_LEFT);
    Line.Y += Layout_CreditStepY;
    Paint_Text(DC, &Line, Plug->Editor.Credit, Theme_TextCredit, DT_LEFT);
    GdiFlush();
    Canvas_Blit(&Plug->Editor.Canvas, &Glyphs_CreditMascot, Cell_Idle, Layout_CreditMascot.X, Layout_CreditMascot.Y,
                Canvas_Opaque);
}

static void Paint_Overlays(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;
    PCanvas Canvas = &Editor->Canvas;
    bool InfoLit = Editor->InfoOpen || Editor->Pressed == Hit_Info;

    Canvas_Blit(Canvas, &Glyphs_MascotBody, Cell_Idle, Layout_Mascot.X, Layout_Mascot.Y, Canvas_Opaque);
    Canvas_Blit(Canvas, &Glyphs_MascotHand, Cell_Idle, Layout_Mascot.X, Layout_MascotHandY, Canvas_Opaque);
    Canvas_Blit(Canvas, &Glyphs_Info, InfoLit ? Cell_Active : Cell_Idle, Layout_InfoButton.X, Layout_InfoButton.Y,
                Canvas_Opaque);
}

void Paint_Editor(TSoundfontPlug *Plug, HDC DC)
{
    TSelection Font = { DC, NULL };
    TBox Panel = { 0, 0, Layout_PanelW, Layout_MaxH };

    if (Editor_Artwork(Plug)) {
        Paint_Background(Plug, DC);
    } else {
        Paint_Fill(DC, &Panel, Theme_PanelFallback);
    }
    GdiFlush();
    Paint_Glyphs(Plug);
    SetBkMode(DC, TRANSPARENT);
    Font.Previous = SelectObject(DC, Paint_PanelFont());
    Paint_Names(Plug, DC);
    GdiFlush();
    Paint_Overlays(Plug);
    if (Plug->Editor.InfoOpen) {
        Paint_Credits(Plug, DC);
    }
}
