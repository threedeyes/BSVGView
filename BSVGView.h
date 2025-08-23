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
    void SetOffset(float x, float y);
    void SetAutoScale(bool enable);
    void FitToWindow();
    void CenterImage();
    void ActualSize();

    BRect GetSVGBounds() const;
    float GetSVGWidth() const;
    float GetSVGHeight() const;
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
    void _TransformGradientCoords(float* x, float* y, float* xform);
    float _GetGradientScale(float* xform);

private:
    NSVGimage* fSVGImage;
    float      fScale;
    float      fOffsetX;
    float      fOffsetY;
    bool       fAutoScale;
    BString    fLoadedFile;
};

#endif
