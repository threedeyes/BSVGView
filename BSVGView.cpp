/*
 * Copyright 2020-2025, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#define NANOSVG_IMPLEMENTATION

#include "BSVGView.h"

#include <stdio.h>
#include <math.h>
#include <GradientLinear.h>
#include <GradientRadial.h>

BSVGView::BSVGView(BRect frame, const char* name, uint32 resizeMask, uint32 flags)
    : BView(frame, name, resizeMask, flags),
      fSVGImage(NULL),
      fScale(1.0f),
      fOffsetX(0.0f),
      fOffsetY(0.0f),
      fAutoScale(true)
{
//    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

BSVGView::~BSVGView()
{
    Unload();
}

status_t
BSVGView::LoadFromFile(const char* filename, const char* units, float dpi)
{
    if (!filename)
        return B_BAD_VALUE;

    Unload();

    fSVGImage = nsvgParseFromFile(filename, units, dpi);
    if (!fSVGImage)
        return B_ERROR;

    fLoadedFile = filename;

    if (fAutoScale)
        _CalculateAutoScale();

    Invalidate();
    return B_OK;
}

status_t
BSVGView::LoadFromMemory(const char* data, const char* units, float dpi)
{
    if (!data)
        return B_BAD_VALUE;

    Unload();

    char* dataCopy = strdup(data);
    fSVGImage = nsvgParse(dataCopy, units, dpi);
    free(dataCopy);

    if (!fSVGImage)
        return B_ERROR;

    fLoadedFile.SetTo("");

    if (fAutoScale)
        _CalculateAutoScale();
        
    Invalidate();
    return B_OK;
}

void
BSVGView::Unload()
{
    if (fSVGImage) {
        nsvgDelete(fSVGImage);
        fSVGImage = NULL;
    }
    fLoadedFile.SetTo("");
}

void
BSVGView::Draw(BRect updateRect)
{
    if (!fSVGImage)
        return;

    PushState();

    for (NSVGshape* shape = fSVGImage->shapes; shape != NULL; shape = shape->next) {
        if (shape->flags & NSVG_FLAGS_VISIBLE) {
            _DrawShape(shape);
        }
    }

    PopState();
}

void
BSVGView::AttachedToWindow()
{
    BView::AttachedToWindow();

    if (fAutoScale && fSVGImage)
        _CalculateAutoScale();
}

void
BSVGView::FrameResized(float newWidth, float newHeight)
{
    BView::FrameResized(newWidth, newHeight);

    if (fAutoScale && fSVGImage) {
        _CalculateAutoScale();
        Invalidate();
    }
}

void
BSVGView::SetScale(float scale)
{
    if (scale > 0.0f) {
        fScale = scale;
        Invalidate();
    }
}

void
BSVGView::SetOffset(float x, float y)
{
    fOffsetX = x;
    fOffsetY = y;
    Invalidate();
}

void
BSVGView::SetAutoScale(bool enable)
{
    fAutoScale = enable;
    if (enable && fSVGImage) {
        _CalculateAutoScale();
        Invalidate();
    }
}

void
BSVGView::FitToWindow()
{
    if (fSVGImage) {
        fAutoScale = true;
        _CalculateAutoScale();
        Invalidate();
    }
}

void
BSVGView::CenterImage()
{
    if (!fSVGImage)
        return;
        
    BRect bounds = Bounds();
    float scaledWidth = fSVGImage->width * fScale;
    float scaledHeight = fSVGImage->height * fScale;
    
    fOffsetX = (bounds.Width() - scaledWidth) / 2.0f;
    fOffsetY = (bounds.Height() - scaledHeight) / 2.0f;
    
    Invalidate();
}

void
BSVGView::ActualSize()
{
    if (fSVGImage) {
        fAutoScale = false;
        fScale = 1.0f;
        CenterImage();
    }
}

BRect
BSVGView::GetSVGBounds() const
{
    if (!fSVGImage)
        return BRect();
        
    return BRect(0, 0, fSVGImage->width - 1, fSVGImage->height - 1);
}

float
BSVGView::GetSVGWidth() const
{
    return fSVGImage ? fSVGImage->width : 0.0f;
}

float
BSVGView::GetSVGHeight() const
{
    return fSVGImage ? fSVGImage->height : 0.0f;
}

void
BSVGView::_DrawShape(NSVGshape* shape)
{
    if (!shape)
        return;

    BShape bShape;

    SetDrawingMode(B_OP_ALPHA);

    for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
        _ConvertPath(path, bShape);
    }

    if (shape->fill.type != NSVG_PAINT_NONE) {
        _ApplyFillPaint(&shape->fill, shape->opacity);
        
        if (shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT || 
            shape->fill.type == NSVG_PAINT_RADIAL_GRADIENT) {
            BGradient* gradient = NULL;
            _SetupGradient(shape->fill.gradient, bShape.Bounds(), shape->fill.type, &gradient);
            if (gradient) {
                FillShape(&bShape, *gradient);
                delete gradient;
            } else {
                FillShape(&bShape);
            }
        } else {
            FillShape(&bShape);
        }
    }

    if (shape->stroke.type != NSVG_PAINT_NONE && shape->strokeWidth > 0.0f) {
        _ApplyStrokePaint(&shape->stroke, shape->opacity);
        _SetupStrokeStyle(shape);

        if (shape->stroke.type == NSVG_PAINT_LINEAR_GRADIENT || 
            shape->stroke.type == NSVG_PAINT_RADIAL_GRADIENT) {
            if (shape->stroke.gradient && shape->stroke.gradient->nstops > 0) {
                rgb_color color = _ConvertColor(shape->stroke.gradient->stops[0].color, shape->opacity);
                SetHighColor(color);
            }
        }       
        StrokeShape(&bShape);
    }
}

void
BSVGView::_ConvertPath(NSVGpath* path, BShape& shape)
{
    if (!path || path->npts < 2)
        return;

    BPoint startPoint(path->pts[0] * fScale + fOffsetX,
                     path->pts[1] * fScale + fOffsetY);
    shape.MoveTo(startPoint);

    for (int i = 1; i < path->npts; i += 3) {
        if (i + 2 < path->npts) {
            BPoint control1(path->pts[i*2] * fScale + fOffsetX,
                           path->pts[i*2 + 1] * fScale + fOffsetY);
            BPoint control2(path->pts[(i+1)*2] * fScale + fOffsetX,
                           path->pts[(i+1)*2 + 1] * fScale + fOffsetY);
            BPoint endPoint(path->pts[(i+2)*2] * fScale + fOffsetX,
                           path->pts[(i+2)*2 + 1] * fScale + fOffsetY);

            shape.BezierTo(control1, control2, endPoint);
        }
    }

    if (path->closed)
        shape.Close();
}

void
BSVGView::_ApplyFillPaint(NSVGpaint* paint, float opacity)
{
    if (!paint)
        return;

    switch (paint->type) {
        case NSVG_PAINT_COLOR: {
            rgb_color color = _ConvertColor(paint->color, opacity);
            SetHighColor(color);
            break;
        }

        default:
            break;
    }
}

void
BSVGView::_ApplyStrokePaint(NSVGpaint* paint, float opacity)
{
    if (!paint)
        return;

    switch (paint->type) {
        case NSVG_PAINT_COLOR: {
            rgb_color color = _ConvertColor(paint->color, opacity);
            SetHighColor(color);
            break;
        }

        case NSVG_PAINT_LINEAR_GRADIENT:
        case NSVG_PAINT_RADIAL_GRADIENT: {
            if (paint->gradient && paint->gradient->nstops > 0) {
                rgb_color color = _ConvertColor(paint->gradient->stops[0].color, opacity);
                SetHighColor(color);
            }
            break;
        }

        default:
            SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
            break;
    }
}

void
BSVGView::_SetupGradient(NSVGgradient* gradient, BRect bounds, char gradientType, BGradient** outGradient)
{
    if (!gradient || !outGradient || gradient->nstops == 0) {
        *outGradient = NULL;
        return;
    }

    BGradient* bgradient = NULL;

    float inv[6], xform[6];
    memcpy(inv, gradient->xform, sizeof(float) * 6);

    double det = (double)inv[0] * inv[3] - (double)inv[2] * inv[1];
    if (fabs(det) < 1e-6) {
        *outGradient = NULL;
        return;
    }

    double invdet = 1.0 / det;
    xform[0] = (float)(inv[3] * invdet);
    xform[1] = (float)(-inv[1] * invdet);
    xform[2] = (float)(-inv[2] * invdet);
    xform[3] = (float)(inv[0] * invdet);
    xform[4] = (float)(((double)inv[2] * inv[5] - (double)inv[3] * inv[4]) * invdet);
    xform[5] = (float)(((double)inv[1] * inv[4] - (double)inv[0] * inv[5]) * invdet);

    if (gradientType == NSVG_PAINT_LINEAR_GRADIENT) {
        float x1 = xform[4];
        float y1 = xform[5];

        float dx = xform[2];
        float dy = xform[3];

        float x2 = x1 + dx;
        float y2 = y1 + dy;

        x1 = x1 * fScale + fOffsetX;
        y1 = y1 * fScale + fOffsetY;
        x2 = x2 * fScale + fOffsetX;
        y2 = y2 * fScale + fOffsetY;

        bgradient = new BGradientLinear(BPoint(x1, y1), BPoint(x2, y2));
    } else if (gradientType == NSVG_PAINT_RADIAL_GRADIENT) {
        float cx = xform[4] * fScale + fOffsetX;
        float cy = xform[5] * fScale + fOffsetY;
        float radius = sqrtf(xform[0] * xform[0] + xform[3] * xform[3]) * fScale;

        bgradient = new BGradientRadial(BPoint(cx, cy), radius);
    }

    if (bgradient) {
        for (int i = 0; i < gradient->nstops; i++) {
            NSVGgradientStop* stop = &gradient->stops[i];
            rgb_color color = _ConvertColor(stop->color, 1.0f);

            float offset = stop->offset * 255.0f;
            if (offset < 0) offset = 0;
            if (offset > 255) offset = 255;

            bgradient->AddColor(color, offset);
        }
    }

    *outGradient = bgradient;
}

rgb_color
BSVGView::_ConvertColor(unsigned int color, float opacity)
{
    rgb_color result;
    result.red = (color >> 0) & 0xFF;
    result.green = (color >> 8) & 0xFF;
    result.blue = (color >> 16) & 0xFF;

    uint8 alpha = (color >> 24) & 0xFF;

    if (alpha == 0 && ((color >> 24) == 0)) {
        alpha = 255;
    }

    result.alpha = (uint8)(alpha * opacity);

    return result;
}

void
BSVGView::_SetupStrokeStyle(NSVGshape* shape)
{
    SetPenSize(shape->strokeWidth * fScale);

    cap_mode capMode = B_BUTT_CAP;
    join_mode joinMode = B_MITER_JOIN;

    switch (shape->strokeLineCap) {
        case NSVG_CAP_BUTT:
            capMode = B_BUTT_CAP;
            break;
        case NSVG_CAP_ROUND:
            capMode = B_ROUND_CAP;
            break;
        case NSVG_CAP_SQUARE:
            capMode = B_SQUARE_CAP;
            break;
    }

    switch (shape->strokeLineJoin) {
        case NSVG_JOIN_MITER:
            joinMode = B_MITER_JOIN;
            break;
        case NSVG_JOIN_ROUND:
            joinMode = B_ROUND_JOIN;
            break;
        case NSVG_JOIN_BEVEL:
            joinMode = B_BEVEL_JOIN;
            break;
    }

    SetLineMode(capMode, joinMode, shape->miterLimit * fScale);
}

void
BSVGView::_CalculateAutoScale()
{
    if (!fSVGImage)
        return;

    BRect bounds = Bounds();
    if (!bounds.IsValid() || bounds.Width() <= 0 || bounds.Height() <= 0)
        return;

    float padding = 10.0f;
    float availableWidth = bounds.Width() - 2 * padding;
    float availableHeight = bounds.Height() - 2 * padding;

    if (availableWidth <= 0 || availableHeight <= 0) {
        availableWidth = bounds.Width();
        availableHeight = bounds.Height();
        padding = 0;
    }

    float scaleX = availableWidth / fSVGImage->width;
    float scaleY = availableHeight / fSVGImage->height;

    fScale = (scaleX < scaleY) ? scaleX : scaleY;

    float scaledWidth = fSVGImage->width * fScale;
    float scaledHeight = fSVGImage->height * fScale;

    fOffsetX = (bounds.Width() - scaledWidth) / 2.0f;
    fOffsetY = (bounds.Height() - scaledHeight) / 2.0f;
}

void
BSVGView::_TransformGradientCoords(float* x, float* y, float* xform)
{
    if (!x || !y || !xform)
        return;

    float tx = *x * xform[0] + *y * xform[2] + xform[4];
    float ty = *x * xform[1] + *y * xform[3] + xform[5];

    *x = tx;
    *y = ty;
}

float
BSVGView::_GetGradientScale(float* xform)
{
    if (!xform)
        return 1.0f;

    float sx = sqrtf(xform[0] * xform[0] + xform[1] * xform[1]);
    float sy = sqrtf(xform[2] * xform[2] + xform[3] * xform[3]);

    return (sx + sy) / 2.0f;
}
