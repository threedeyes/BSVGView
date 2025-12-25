/*
 * Copyright 2020-2025, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#ifndef B_SVGVIEW_H
#define B_SVGVIEW_H

#include <View.h>
#include <Shape.h>
#include <Rect.h>
#include <Region.h>
#include <String.h>
#include <Bitmap.h>
#include <Gradient.h>
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <GradientRadialFocus.h>

#include <agg_path_storage.h>
#include <agg_conv_stroke.h>
#include <agg_conv_curve.h>
#include <agg_conv_transform.h>
#include <agg_trans_affine.h>
#include <agg_rendering_buffer.h>
#include <agg_pixfmt_rgba.h>
#include <agg_renderer_base.h>
#include <agg_renderer_scanline.h>
#include <agg_rasterizer_scanline_aa.h>
#include <agg_scanline_p.h>

#include "nanosvg.h"

enum svg_display_mode {
	SVG_DISPLAY_NORMAL = 0,
	SVG_DISPLAY_OUTLINE,
	SVG_DISPLAY_FILL_ONLY,
	SVG_DISPLAY_STROKE_ONLY
};

enum svg_boundingbox_style {
	SVG_BBOX_NONE = 0,
	SVG_BBOX_DOCUMENT,
	SVG_BBOX_SIMPLE_FRAME,
	SVG_BBOX_TRANSPARENT_GRAY
};

enum svg_highlight_mode {
	SVG_HIGHLIGHT_NONE = 0,
	SVG_HIGHLIGHT_SHAPE,
	SVG_HIGHLIGHT_PATH,
	SVG_HIGHLIGHT_CONTROL_POINTS
};

struct HighlightInfo {
	svg_highlight_mode mode;
	int32 shapeIndex;
	int32 pathIndex;
	bool showControlPoints;
	bool showBezierHandles;

	HighlightInfo() : mode(SVG_HIGHLIGHT_NONE), shapeIndex(-1),
		pathIndex(-1), showControlPoints(false), showBezierHandles(false) {}
};

struct GradientLUT {
	rgb_color colors[256];
};

class BSVGView : public BView {
public:
							BSVGView(BRect frame, const char* name,
								uint32 resizeMask = B_FOLLOW_ALL_SIDES,
								uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS);
							BSVGView(const char* name = "svg_view");
	virtual					~BSVGView();

	status_t				LoadFromFile(const char* filename,
								const char* units = "px", float dpi = 96.0f);
	status_t				LoadFromMemory(const char* data,
								const char* units = "px", float dpi = 96.0f);
	void					Unload();

	virtual void			Draw(BRect updateRect);
	virtual void			AttachedToWindow();
	virtual void			FrameResized(float newWidth, float newHeight);

	void					SetScale(float scale);
	void					SetOffset(BPoint point);
	void					SetAutoScale(bool enable);
	void					FitToWindow();
	void					CenterImage();
	void					ActualSize();

	void					SetDisplayMode(svg_display_mode mode);
	svg_display_mode		DisplayMode() const { return fDisplayMode; }

	void					SetShowTransparency(bool show);
	bool					ShowTransparency() const { return fShowTransparency; }

	void					SetBoundingBoxStyle(svg_boundingbox_style style);
	svg_boundingbox_style	BoundingBoxStyle() const { return fBoundingBoxStyle; }

	void					SetHighlightedShape(int32 shapeIndex);
	void					SetHighlightedPath(int32 shapeIndex, int32 pathIndex);
	void					SetHighlightControlPoints(int32 shapeIndex,
								int32 pathIndex, bool showBezierHandles = true);
	void					ClearHighlight();
	HighlightInfo			GetHighlightInfo() const { return fHighlightInfo; }

	BRect					SVGBounds() const;
	BRect					SVGViewBounds() const;
	float					SVGWidth() const
								{ return fSVGImage ? fSVGImage->width : 0.0f; }
	float					SVGHeight() const
								{ return fSVGImage ? fSVGImage->height : 0.0f; }
	float					Scale() const { return fScale; }
	BPoint					Offset() const { return BPoint(fOffsetX, fOffsetY); }
	NSVGimage*				SVGImage() const { return fSVGImage; }

	bool					IsLoaded() const { return fSVGImage != NULL; }

protected:
	void					_DrawShape(NSVGshape* shape, int32 shapeIndex);
	void					_ConvertPath(NSVGpath* path, BShape& shape);
	void					_ApplyFillPaint(NSVGpaint* paint, float opacity);
	void					_ApplyStrokePaint(NSVGpaint* paint, float opacity);
	void					_SetupGradient(NSVGgradient* gradient, BRect bounds,
								char gradientType, BGradient** outGradient,
								float shapeOpacity = 1.0f);
	rgb_color				_ConvertColor(unsigned int color,
								float opacity = 1.0f);
	void					_CalculateAutoScale();
	void					_SetupStrokeStyle(NSVGshape* shape);
	void					_DrawTransparencyGrid();
	void					_DrawBoundingBox();
	void					_DrawDocumentStyle(BRect bounds);
	void					_DrawSimpleFrame(BRect bounds);
	void					_DrawTransparentGray(BRect bounds);

	void					_DrawHighlight();
	void					_DrawShapeHighlight(NSVGshape* shape);
	void					_DrawPathHighlight(NSVGshape* shape, NSVGpath* path);
	void					_DrawControlPoints(NSVGpath* path);
	void					_DrawBezierHandles(NSVGpath* path);
	void					_DrawControlPoint(BPoint point,
								bool isEndPoint = false, bool isSelected = false);
	void					_DrawHighlightOutline(NSVGpath* path,
								float width = 3.0f);
	BPoint					_ConvertSVGPoint(float x, float y) const;
	float					_GetControlPointSize() const;

	bool					_IsUniformScale(float* xform);
	bool					_HasRotationOrSkew(float* xform);
	BBitmap*				_RasterizeGradient(NSVGgradient* gradient,
								char gradientType, BRect shapeBounds,
								BRect clippedBounds, float shapeOpacity);
	void					_FillShapeWithGradientBitmap(BShape& shape,
								BBitmap* bitmap, BRect shapeBounds,
								BRect clippedBounds);
	rgb_color				_SampleGradientAt(NSVGgradient* gradient,
								char gradientType, float svgX, float svgY,
								float opacity);
	rgb_color				_InterpolateGradientColor(NSVGgradient* gradient,
								float t, float opacity);
	void					_BuildGradientLUT(NSVGgradient* gradient,
								float opacity, GradientLUT& lut);
	float					_ApplySpreadMode(int spread, float t);

	BShape*					_ConvertStrokeToFillShape(NSVGshape* shape);
	void					_BuildAGGPath(NSVGshape* shape,
								agg::path_storage& aggPath);
	void					_StrokeShapeWithRasterizedGradient(NSVGshape* shape,
								char gradientType);

	void					_DrawShapeWithMask(NSVGshape* shape, int32 shapeIndex);
	void					_RenderShapeToBuffer(NSVGshape* shape, BBitmap* bitmap,
								BRect renderBounds);
	void					_RenderMaskToBuffer(NSVGmask* mask, BBitmap* bitmap,
								BRect renderBounds);
	void					_ApplyMaskToBitmap(BBitmap* content, BBitmap* mask);
	void					_BuildAGGPathWithOffset(NSVGshape* shape,
								agg::path_storage& aggPath,
								float offsetX, float offsetY);

protected:
	NSVGimage*				fSVGImage;
	float					fScale;
	float					fOffsetX;
	float					fOffsetY;
	bool					fAutoScale;
	BString					fLoadedFile;
	svg_display_mode		fDisplayMode;
	bool					fShowTransparency;
	svg_boundingbox_style	fBoundingBoxStyle;
	HighlightInfo			fHighlightInfo;
};

#endif
