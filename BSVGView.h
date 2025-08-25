/*
 * Copyright 2020-2025, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#ifndef BSVGVIEW_H
#define BSVGVIEW_H

#include <View.h>
#include <Shape.h>
#include <Rect.h>
#include <String.h>
#include <Gradient.h>
#include <GradientLinear.h>
#include <GradientRadial.h>

#include "nanosvg.h"

class BSVGView : public BView {
public:
    BSVGView(BRect frame, const char* name, 
             uint32 resizeMask = B_FOLLOW_ALL_SIDES, 
             uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS);
    virtual ~BSVGView();

    status_t LoadFromFile(const char* filename, const char* units = "px", float dpi = 96.0f);
    status_t LoadFromMemory(const char* data, const char* units = "px", float dpi = 96.0f);
    void     Unload();

    virtual void Draw(BRect updateRect);
    virtual void AttachedToWindow();
    virtual void FrameResized(float newWidth, float newHeight);

    void SetScale(float scale);
    void SetOffset(BPoint point);
    void SetAutoScale(bool enable);
    void FitToWindow();
    void CenterImage();
    void ActualSize();

    BRect SVGBounds() const {  return fSVGImage ? BRect() : BRect(0, 0, fSVGImage->width - 1, fSVGImage->height - 1); }
    float SVGWidth() const { return fSVGImage ? fSVGImage->width : 0.0f; }
    float SVGHeight() const { return fSVGImage ? fSVGImage->height : 0.0f; }
    float Scale() const { return fScale; }
    BPoint Offset() const { return BPoint(fOffsetX, fOffsetY); }

    bool  IsLoaded() const { return fSVGImage != NULL; }

private:
    void _DrawShape(NSVGshape* shape);
    void _ConvertPath(NSVGpath* path, BShape& shape);
    void _ApplyFillPaint(NSVGpaint* paint, float opacity);
    void _ApplyStrokePaint(NSVGpaint* paint, float opacity);
    void _SetupGradient(NSVGgradient* gradient, BRect bounds, char gradientType, BGradient** outGradient);
    rgb_color _ConvertColor(unsigned int color, float opacity = 1.0f);
    void _CalculateAutoScale();
    void _SetupStrokeStyle(NSVGshape* shape);

private:
    NSVGimage* fSVGImage;
    float      fScale;
    float      fOffsetX;
    float      fOffsetY;
    bool       fAutoScale;
    BString    fLoadedFile;
};

#endif
