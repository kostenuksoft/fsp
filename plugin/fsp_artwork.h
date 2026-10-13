#ifndef FSP_ARTWORK_H
#define FSP_ARTWORK_H

#include "fsp_def.h"

const c8 Artwork_FileName[] = "Artwork\\Back.bmp";

const i32 Artwork_Width = 241;
const i32 Artwork_Height = 240;

constexpr TBox Artwork_Well = { 66, 42, 0, 18 };
constexpr TSpot Artwork_WellRightEdge = { 234, 42 };

constexpr TBox Artwork_PanelFill = { 0, 28, Artwork_Width, 2 };
constexpr TBox Artwork_PanelTopEdge = { 0, 0, Artwork_Width, 3 };
constexpr TBox Artwork_InfoTopEdge = { 0, 68, Artwork_Width, 3 };
constexpr TBox Artwork_PanelBottomEdge = { 0, 63, Artwork_Width, 6 };
constexpr TBox Artwork_ColumnTitles = { 4, 32, 93, 9 };

const i32 Artwork_FrameLeft = 65;
const i32 Artwork_FrameRight = 235;
const i32 Artwork_FrameTopY = 41;
const i32 Artwork_FrameMiddleY = 50;
const i32 Artwork_FrameBottomY = 52;
const i32 Artwork_FrameEdgeH = 9;
const i32 Artwork_FrameMiddleH = 2;

#endif
