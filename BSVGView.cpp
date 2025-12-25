/*
 * Copyright 2020-2025, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS

#include "BSVGView.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const int32 kMaxGradientDimension = 1024;
static const int32 kMaxMaskDimension = 2048;


BSVGView::BSVGView(BRect frame, const char* name, uint32 resizeMask, uint32 flags)
	:
	BView(frame, name, resizeMask, flags)
{
	_InitDefaults();
}


BSVGView::BSVGView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS)
{
	_InitDefaults();
}


BSVGView::~BSVGView()
{
	Unload();
}


void
BSVGView::_InitDefaults()
{
	fSVGImage = NULL;
	fScale = 1.0f;
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	fAutoScale = true;
	fDisplayMode = SVG_DISPLAY_NORMAL;
	fShowTransparency = true;
	fBoundingBoxStyle = SVG_BBOX_NONE;
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
	if (!dataCopy)
		return B_NO_MEMORY;

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
	ClearHighlight();
}


void
BSVGView::Draw(BRect updateRect)
{
	if (!fSVGImage)
		return;

	PushState();

	BRegion region(Bounds());
	ConstrainClippingRegion(&region);

	if (fShowTransparency)
		_DrawTransparencyGrid();

	if (fBoundingBoxStyle != SVG_BBOX_NONE)
		_DrawBoundingBox();

	SetDrawingMode(B_OP_ALPHA);

	int32 shapeIndex = 0;
	for (NSVGshape* shape = fSVGImage->shapes; shape != NULL; shape = shape->next) {
		if (shape->flags & NSVG_FLAGS_VISIBLE)
			_DrawShape(shape, shapeIndex);
		shapeIndex++;
	}

	_DrawHighlight();

	PopState();
}


void
BSVGView::AttachedToWindow()
{
	if (fAutoScale && fSVGImage)
		_CalculateAutoScale();
}


void
BSVGView::FrameResized(float newWidth, float newHeight)
{
	if (fAutoScale && fSVGImage) {
		_CalculateAutoScale();
		Invalidate();
	}
}


void
BSVGView::SetScale(float scale)
{
	if (scale > 0.0f && scale != fScale) {
		fScale = scale;
		Invalidate();
	}
}


void
BSVGView::SetOffset(BPoint point)
{
	if (fOffsetX != point.x || fOffsetY != point.y) {
		fOffsetX = point.x;
		fOffsetY = point.y;
		Invalidate();
	}
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


void
BSVGView::SetDisplayMode(svg_display_mode mode)
{
	if (fDisplayMode != mode) {
		fDisplayMode = mode;
		Invalidate();
	}
}


void
BSVGView::SetShowTransparency(bool show)
{
	if (fShowTransparency != show) {
		fShowTransparency = show;
		Invalidate();
	}
}


void
BSVGView::SetBoundingBoxStyle(svg_boundingbox_style style)
{
	if (fBoundingBoxStyle != style) {
		fBoundingBoxStyle = style;
		Invalidate();
	}
}


void
BSVGView::SetHighlightedShape(int32 shapeIndex)
{
	fHighlightInfo.mode = SVG_HIGHLIGHT_SHAPE;
	fHighlightInfo.shapeIndex = shapeIndex;
	fHighlightInfo.pathIndex = -1;
	fHighlightInfo.showControlPoints = false;
	fHighlightInfo.showBezierHandles = false;
	Invalidate();
}


void
BSVGView::SetHighlightedPath(int32 shapeIndex, int32 pathIndex)
{
	fHighlightInfo.mode = SVG_HIGHLIGHT_PATH;
	fHighlightInfo.shapeIndex = shapeIndex;
	fHighlightInfo.pathIndex = pathIndex;
	fHighlightInfo.showControlPoints = true;
	fHighlightInfo.showBezierHandles = false;
	Invalidate();
}


void
BSVGView::SetHighlightControlPoints(int32 shapeIndex, int32 pathIndex,
	bool showBezierHandles)
{
	fHighlightInfo.mode = SVG_HIGHLIGHT_CONTROL_POINTS;
	fHighlightInfo.shapeIndex = shapeIndex;
	fHighlightInfo.pathIndex = pathIndex;
	fHighlightInfo.showControlPoints = true;
	fHighlightInfo.showBezierHandles = showBezierHandles;
	Invalidate();
}


void
BSVGView::ClearHighlight()
{
	if (fHighlightInfo.mode == SVG_HIGHLIGHT_NONE)
		return;

	fHighlightInfo.mode = SVG_HIGHLIGHT_NONE;
	fHighlightInfo.shapeIndex = -1;
	fHighlightInfo.pathIndex = -1;
	fHighlightInfo.showControlPoints = false;
	fHighlightInfo.showBezierHandles = false;
	Invalidate();
}


BRect
BSVGView::SVGBounds() const
{
	if (!fSVGImage)
		return BRect();
	return BRect(0, 0, fSVGImage->width - 1, fSVGImage->height - 1);
}


BRect
BSVGView::SVGViewBounds() const
{
	if (!fSVGImage)
		return BRect();

	float scaledWidth = fSVGImage->width * fScale;
	float scaledHeight = fSVGImage->height * fScale;

	return BRect(fOffsetX, fOffsetY,
		fOffsetX + scaledWidth - 1,
		fOffsetY + scaledHeight - 1);
}


void
BSVGView::_BuildGradientLUT(NSVGgradient* gradient, float opacity, GradientLUT& lut)
{
	if (!gradient || gradient->nstops == 0) {
		memset(&lut, 0, sizeof(GradientLUT));
		return;
	}

	for (int i = 0; i < 256; i++) {
		float t = i / 255.0f;
		lut.colors[i] = _InterpolateGradientColor(gradient, t, opacity);
	}
}


void
BSVGView::_ApplyGradientToBuffer(uint8* bits, int width, int height, int32 bpr,
	NSVGgradient* gradient, char gradientType, BRect renderBounds, float opacity)
{
	if (!bits || !gradient)
		return;

	GradientLUT lut;
	_BuildGradientLUT(gradient, opacity, lut);

	float* m = gradient->xform;
	float invScale = 1.0f / fScale;

	for (int py = 0; py < height; py++) {
		uint8* row = bits + py * bpr;
		for (int px = 0; px < width; px++) {
			uint8 alpha = row[px * 4 + 3];
			if (alpha == 0)
				continue;

			float viewX = renderBounds.left + px;
			float viewY = renderBounds.top + py;
			float svgX = (viewX - fOffsetX) * invScale;
			float svgY = (viewY - fOffsetY) * invScale;

			float gx = m[0] * svgX + m[2] * svgY + m[4];
			float gy = m[1] * svgX + m[3] * svgY + m[5];

			float t;
			if (gradientType == NSVG_PAINT_LINEAR_GRADIENT) {
				t = gy;
			} else {
				t = sqrtf(gx * gx + gy * gy);
			}

			t = _ApplySpreadMode(gradient->spread, t);

			int idx = (int)(t * 255.0f + 0.5f);
			if (idx < 0) idx = 0;
			if (idx > 255) idx = 255;

			rgb_color c = lut.colors[idx];

			row[px * 4 + 0] = c.blue;
			row[px * 4 + 1] = c.green;
			row[px * 4 + 2] = c.red;
			row[px * 4 + 3] = (uint8)(((uint16)c.alpha * alpha) / 255);
		}
	}
}


float
BSVGView::_ApplySpreadMode(int spread, float t)
{
	switch (spread) {
		case NSVG_SPREAD_PAD:
			if (t < 0.0f)
				return 0.0f;
			if (t > 1.0f)
				return 1.0f;
			return t;

		case NSVG_SPREAD_REPEAT:
			t = t - floorf(t);
			if (t < 0.0f)
				t += 1.0f;
			return t;

		case NSVG_SPREAD_REFLECT:
		{
			t = fabsf(t);
			int period = (int)floorf(t);
			t = t - period;
			if (period % 2 != 0)
				t = 1.0f - t;
			return t;
		}

		default:
			if (t < 0.0f)
				return 0.0f;
			if (t > 1.0f)
				return 1.0f;
			return t;
	}
}


void
BSVGView::_BuildAGGPath(NSVGshape* shape, agg::path_storage& aggPath)
{
	_BuildAGGPathWithOffset(shape, aggPath, fOffsetX, fOffsetY);
}


void
BSVGView::_BuildAGGPathWithOffset(NSVGshape* shape, agg::path_storage& aggPath,
	float offsetX, float offsetY)
{
	if (!shape)
		return;

	for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
		if (path->npts < 2)
			continue;

		float startX = path->pts[0] * fScale + offsetX;
		float startY = path->pts[1] * fScale + offsetY;
		aggPath.move_to(startX, startY);

		for (int i = 1; i + 2 < path->npts; i += 3) {
			float c1x = path->pts[i * 2] * fScale + offsetX;
			float c1y = path->pts[i * 2 + 1] * fScale + offsetY;
			float c2x = path->pts[(i + 1) * 2] * fScale + offsetX;
			float c2y = path->pts[(i + 1) * 2 + 1] * fScale + offsetY;
			float endX = path->pts[(i + 2) * 2] * fScale + offsetX;
			float endY = path->pts[(i + 2) * 2 + 1] * fScale + offsetY;

			aggPath.curve4(c1x, c1y, c2x, c2y, endX, endY);
		}

		if (path->closed)
			aggPath.close_polygon();
	}
}


agg::line_cap_e
BSVGView::_ConvertLineCapAGG(int nsvgCap)
{
	switch (nsvgCap) {
		case NSVG_CAP_ROUND:
			return agg::round_cap;
		case NSVG_CAP_SQUARE:
			return agg::square_cap;
		case NSVG_CAP_BUTT:
		default:
			return agg::butt_cap;
	}
}


agg::line_join_e
BSVGView::_ConvertLineJoinAGG(int nsvgJoin)
{
	switch (nsvgJoin) {
		case NSVG_JOIN_ROUND:
			return agg::round_join;
		case NSVG_JOIN_BEVEL:
			return agg::bevel_join;
		case NSVG_JOIN_MITER:
		default:
			return agg::miter_join;
	}
}


cap_mode
BSVGView::_ConvertLineCapHaiku(int nsvgCap)
{
	switch (nsvgCap) {
		case NSVG_CAP_ROUND:
			return B_ROUND_CAP;
		case NSVG_CAP_SQUARE:
			return B_SQUARE_CAP;
		case NSVG_CAP_BUTT:
		default:
			return B_BUTT_CAP;
	}
}


join_mode
BSVGView::_ConvertLineJoinHaiku(int nsvgJoin)
{
	switch (nsvgJoin) {
		case NSVG_JOIN_ROUND:
			return B_ROUND_JOIN;
		case NSVG_JOIN_BEVEL:
			return B_BEVEL_JOIN;
		case NSVG_JOIN_MITER:
		default:
			return B_MITER_JOIN;
	}
}


float
BSVGView::_ClampMiterLimit(float miterLimit)
{
	if (miterLimit < 1.0f)
		return 1.0f;
	if (miterLimit > 100.0f)
		return 100.0f;
	return miterLimit;
}


BShape*
BSVGView::_ConvertStrokeToFillShape(NSVGshape* shape)
{
	if (!shape || !shape->paths)
		return NULL;

	agg::path_storage aggPath;
	_BuildAGGPath(shape, aggPath);

	typedef agg::conv_curve<agg::path_storage> CurveConverter;
	CurveConverter curve(aggPath);
	curve.approximation_scale(fScale > 1.0f ? fScale : 1.0f);

	typedef agg::conv_stroke<CurveConverter> StrokeConverter;
	StrokeConverter stroke(curve);

	float strokeWidth = shape->strokeWidth * fScale;
	if (strokeWidth < 0.1f)
		strokeWidth = 0.1f;
	stroke.width(strokeWidth);

	stroke.line_cap(_ConvertLineCapAGG(shape->strokeLineCap));
	stroke.line_join(_ConvertLineJoinAGG(shape->strokeLineJoin));
	stroke.miter_limit(_ClampMiterLimit(shape->miterLimit));

	BShape* result = new BShape();
	double x, y;
	unsigned cmd;
	bool isFirstVertex = true;
	bool hasContent = false;

	stroke.rewind(0);

	while (!agg::is_stop(cmd = stroke.vertex(&x, &y))) {
		if (agg::is_move_to(cmd)) {
			result->MoveTo(BPoint(x, y));
			isFirstVertex = false;
			hasContent = true;
		} else if (agg::is_line_to(cmd)) {
			if (isFirstVertex) {
				result->MoveTo(BPoint(x, y));
				isFirstVertex = false;
			} else {
				result->LineTo(BPoint(x, y));
			}
			hasContent = true;
		} else if (agg::is_close(cmd)) {
			result->Close();
			isFirstVertex = true;
		} else if (agg::is_end_poly(cmd)) {
			if (cmd & agg::path_flags_close)
				result->Close();
			isFirstVertex = true;
		}
	}

	if (!hasContent) {
		delete result;
		return NULL;
	}

	return result;
}


void
BSVGView::_StrokeShapeWithRasterizedGradient(NSVGshape* shape, char gradientType)
{
	if (!shape || !shape->stroke.gradient)
		return;

	NSVGgradient* gradient = shape->stroke.gradient;
	if (gradient->nstops == 0)
		return;

	BRect viewBounds = Bounds();

	agg::path_storage aggPath;
	_BuildAGGPath(shape, aggPath);

	typedef agg::conv_curve<agg::path_storage> CurveConverter;
	CurveConverter curve(aggPath);

	float approxScale = fScale;
	if (approxScale < 1.0f)
		approxScale = 1.0f;

	curve.approximation_method(agg::curve_div);
	curve.approximation_scale(approxScale);
	curve.angle_tolerance(0.0);

	typedef agg::conv_stroke<CurveConverter> StrokeConverter;
	StrokeConverter stroke(curve);

	float strokeWidth = shape->strokeWidth * fScale;
	if (strokeWidth < 0.1f)
		strokeWidth = 0.1f;
	stroke.width(strokeWidth);

	stroke.line_cap(_ConvertLineCapAGG(shape->strokeLineCap));
	stroke.line_join(_ConvertLineJoinAGG(shape->strokeLineJoin));
	stroke.miter_limit(_ClampMiterLimit(shape->miterLimit));

	agg::path_storage strokePath;
	double x, y;
	unsigned cmd;

	double minX = 1e30, minY = 1e30, maxX = -1e30, maxY = -1e30;

	stroke.rewind(0);
	while (!agg::is_stop(cmd = stroke.vertex(&x, &y))) {
		if (agg::is_move_to(cmd)) {
			strokePath.move_to(x, y);
			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
		} else if (agg::is_vertex(cmd)) {
			strokePath.line_to(x, y);
			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
		} else if (agg::is_end_poly(cmd)) {
			strokePath.end_poly(cmd);
		}
	}

	if (maxX - minX < 1.0 || maxY - minY < 1.0)
		return;

	BRect strokeBounds(minX - 2, minY - 2, maxX + 2, maxY + 2);

	if (!strokeBounds.Intersects(viewBounds))
		return;

	BRect totalBounds = strokeBounds & viewBounds;
	if (!totalBounds.IsValid())
		return;

	int width = (int)ceilf(totalBounds.Width()) + 1;
	int height = (int)ceilf(totalBounds.Height()) + 1;

	float downsample = 1.0f;
	if (width > kMaxGradientDimension || height > kMaxGradientDimension) {
		downsample = fmaxf((float)width / kMaxGradientDimension,
		                   (float)height / kMaxGradientDimension);
		width = (int)(width / downsample);
		height = (int)(height / downsample);
	}
	if (width < 1) width = 1;
	if (height < 1) height = 1;

	BBitmap* combinedBitmap = new BBitmap(
		BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (!combinedBitmap || combinedBitmap->InitCheck() != B_OK) {
		delete combinedBitmap;
		return;
	}

	uint8* bits = (uint8*)combinedBitmap->Bits();
	int32 bpr = combinedBitmap->BytesPerRow();
	memset(bits, 0, combinedBitmap->BitsLength());

	typedef agg::pixfmt_bgra32 pixfmt;
	typedef agg::renderer_base<pixfmt> renderer_base;
	typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;

	agg::rendering_buffer rbuf(bits, width, height, bpr);
	pixfmt pixf(rbuf);
	renderer_base rb(pixf);
	renderer_solid ren(rb);

	agg::rasterizer_scanline_aa<> ras;
	agg::scanline_p8 sl;

	ras.clip_box(0, 0, width, height);

	agg::trans_affine mtx;
	mtx *= agg::trans_affine_translation(-totalBounds.left, -totalBounds.top);
	mtx *= agg::trans_affine_scaling(1.0 / downsample, 1.0 / downsample);

	agg::conv_transform<agg::path_storage, agg::trans_affine> trans(strokePath, mtx);

	ras.add_path(trans);

	ren.color(agg::rgba8(255, 255, 255, 255));
	agg::render_scanlines(ras, sl, ren);

	if (downsample > 1.0f) {
		float savedScale = fScale;
		float savedOffsetX = fOffsetX;
		float savedOffsetY = fOffsetY;

		fScale = fScale / downsample;
		fOffsetX = (fOffsetX - totalBounds.left) / downsample;
		fOffsetY = (fOffsetY - totalBounds.top) / downsample;

		BRect localBounds(0, 0, width - 1, height - 1);
		_ApplyGradientToBuffer(bits, width, height, bpr, gradient,
			gradientType, localBounds, shape->opacity);

		fScale = savedScale;
		fOffsetX = savedOffsetX;
		fOffsetY = savedOffsetY;
	} else {
		_ApplyGradientToBuffer(bits, width, height, bpr, gradient,
			gradientType, totalBounds, shape->opacity);
	}

	SetDrawingMode(B_OP_ALPHA);
	DrawBitmap(combinedBitmap, combinedBitmap->Bounds(), totalBounds);

	delete combinedBitmap;
}


void
BSVGView::_RenderShapeToBuffer(NSVGshape* shape, BBitmap* bitmap, BRect renderBounds)
{
	if (!shape || !bitmap)
		return;

	uint8* bits = (uint8*)bitmap->Bits();
	int width = bitmap->Bounds().IntegerWidth() + 1;
	int height = bitmap->Bounds().IntegerHeight() + 1;
	int32 bpr = bitmap->BytesPerRow();

	typedef agg::pixfmt_bgra32 pixfmt;
	typedef agg::renderer_base<pixfmt> renderer_base;
	typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;

	agg::rendering_buffer rbuf(bits, width, height, bpr);
	pixfmt pixf(rbuf);
	renderer_base rb(pixf);
	renderer_solid ren(rb);

	agg::rasterizer_scanline_aa<> ras;
	agg::scanline_p8 sl;

	float localOffsetX = fOffsetX - renderBounds.left;
	float localOffsetY = fOffsetY - renderBounds.top;

	agg::path_storage aggPath;
	_BuildAGGPathWithOffset(shape, aggPath, localOffsetX, localOffsetY);

	typedef agg::conv_curve<agg::path_storage> CurveConverter;
	CurveConverter curve(aggPath);
	curve.approximation_scale(fScale > 1.0f ? fScale : 1.0f);

	if (shape->fill.type != NSVG_PAINT_NONE) {
		ras.reset();

		if (shape->fillRule == NSVG_FILLRULE_EVENODD)
			ras.filling_rule(agg::fill_even_odd);
		else
			ras.filling_rule(agg::fill_non_zero);

		ras.add_path(curve);

		if (shape->fill.type == NSVG_PAINT_COLOR) {
			rgb_color color = _ConvertColor(shape->fill.color, shape->opacity);
			ren.color(agg::rgba8(color.red, color.green, color.blue, color.alpha));
			agg::render_scanlines(ras, sl, ren);
		} else if ((shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT ||
					shape->fill.type == NSVG_PAINT_RADIAL_GRADIENT) &&
				   shape->fill.gradient != NULL) {
			ren.color(agg::rgba8(255, 255, 255, 255));
			agg::render_scanlines(ras, sl, ren);

			_ApplyGradientToBuffer(bits, width, height, bpr,
				shape->fill.gradient, shape->fill.type, renderBounds,
				shape->opacity);
		}
	}

	if (shape->stroke.type != NSVG_PAINT_NONE && shape->strokeWidth > 0.0f) {
		typedef agg::conv_stroke<CurveConverter> StrokeConverter;
		StrokeConverter stroke(curve);

		float strokeWidth = shape->strokeWidth * fScale;
		if (strokeWidth < 0.1f)
			strokeWidth = 0.1f;
		stroke.width(strokeWidth);

		stroke.line_cap(_ConvertLineCapAGG(shape->strokeLineCap));
		stroke.line_join(_ConvertLineJoinAGG(shape->strokeLineJoin));
		stroke.miter_limit(_ClampMiterLimit(shape->miterLimit));

		ras.reset();
		ras.filling_rule(agg::fill_non_zero);
		ras.add_path(stroke);

		if (shape->stroke.type == NSVG_PAINT_COLOR) {
			rgb_color color = _ConvertColor(shape->stroke.color, shape->opacity);
			ren.color(agg::rgba8(color.red, color.green, color.blue, color.alpha));
			agg::render_scanlines(ras, sl, ren);
		} else if ((shape->stroke.type == NSVG_PAINT_LINEAR_GRADIENT ||
					shape->stroke.type == NSVG_PAINT_RADIAL_GRADIENT) &&
				   shape->stroke.gradient != NULL) {
			ren.color(agg::rgba8(255, 255, 255, 255));
			agg::render_scanlines(ras, sl, ren);

			_ApplyGradientToBuffer(bits, width, height, bpr,
				shape->stroke.gradient, shape->stroke.type, renderBounds,
				shape->opacity);
		}
	}
}


void
BSVGView::_RenderMaskToBuffer(NSVGmask* mask, BBitmap* bitmap, BRect renderBounds)
{
	if (!mask || !mask->shapes || !bitmap)
		return;

	uint8* bits = (uint8*)bitmap->Bits();
	int width = bitmap->Bounds().IntegerWidth() + 1;
	int height = bitmap->Bounds().IntegerHeight() + 1;
	int32 bpr = bitmap->BytesPerRow();

	typedef agg::pixfmt_bgra32 pixfmt;
	typedef agg::renderer_base<pixfmt> renderer_base;
	typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;

	agg::rendering_buffer rbuf(bits, width, height, bpr);
	pixfmt pixf(rbuf);
	renderer_base rb(pixf);
	renderer_solid ren(rb);

	agg::rasterizer_scanline_aa<> ras;
	agg::scanline_p8 sl;

	float localOffsetX = fOffsetX - renderBounds.left;
	float localOffsetY = fOffsetY - renderBounds.top;

	for (NSVGshape* maskShape = mask->shapes; maskShape != NULL;
		 maskShape = maskShape->next) {
		if (!(maskShape->flags & NSVG_FLAGS_VISIBLE))
			continue;

		agg::path_storage aggPath;
		_BuildAGGPathWithOffset(maskShape, aggPath, localOffsetX, localOffsetY);

		typedef agg::conv_curve<agg::path_storage> CurveConverter;
		CurveConverter curve(aggPath);
		curve.approximation_scale(fScale > 1.0f ? fScale : 1.0f);

		if (maskShape->fill.type != NSVG_PAINT_NONE) {
			ras.reset();

			if (maskShape->fillRule == NSVG_FILLRULE_EVENODD)
				ras.filling_rule(agg::fill_even_odd);
			else
				ras.filling_rule(agg::fill_non_zero);

			ras.add_path(curve);

			if (maskShape->fill.type == NSVG_PAINT_COLOR) {
				rgb_color color = _ConvertColor(maskShape->fill.color,
					maskShape->opacity);
				ren.color(agg::rgba8(color.red, color.green, color.blue,
					color.alpha));
				agg::render_scanlines(ras, sl, ren);
			} else if ((maskShape->fill.type == NSVG_PAINT_LINEAR_GRADIENT ||
						maskShape->fill.type == NSVG_PAINT_RADIAL_GRADIENT) &&
					   maskShape->fill.gradient != NULL) {
				ren.color(agg::rgba8(255, 255, 255, 255));
				agg::render_scanlines(ras, sl, ren);

				_ApplyGradientToBuffer(bits, width, height, bpr,
					maskShape->fill.gradient, maskShape->fill.type,
					renderBounds, maskShape->opacity);
			}
		}

		if (maskShape->stroke.type != NSVG_PAINT_NONE
			&& maskShape->strokeWidth > 0.0f) {
			typedef agg::conv_stroke<CurveConverter> StrokeConverter;
			StrokeConverter stroke(curve);

			float strokeWidth = maskShape->strokeWidth * fScale;
			if (strokeWidth < 0.1f)
				strokeWidth = 0.1f;
			stroke.width(strokeWidth);

			stroke.line_cap(_ConvertLineCapAGG(maskShape->strokeLineCap));
			stroke.line_join(_ConvertLineJoinAGG(maskShape->strokeLineJoin));
			stroke.miter_limit(_ClampMiterLimit(maskShape->miterLimit));

			ras.reset();
			ras.filling_rule(agg::fill_non_zero);
			ras.add_path(stroke);

			if (maskShape->stroke.type == NSVG_PAINT_COLOR) {
				rgb_color color = _ConvertColor(maskShape->stroke.color,
					maskShape->opacity);
				ren.color(agg::rgba8(color.red, color.green, color.blue,
					color.alpha));
				agg::render_scanlines(ras, sl, ren);
			}
		}
	}
}


void
BSVGView::_ApplyMaskToBitmap(BBitmap* content, BBitmap* mask)
{
	if (!content || !mask)
		return;

	int width = content->Bounds().IntegerWidth() + 1;
	int height = content->Bounds().IntegerHeight() + 1;
	int32 contentBpr = content->BytesPerRow();
	int32 maskBpr = mask->BytesPerRow();

	uint8* contentBits = (uint8*)content->Bits();
	uint8* maskBits = (uint8*)mask->Bits();

	for (int py = 0; py < height; py++) {
		uint8* contentRow = contentBits + py * contentBpr;
		uint8* maskRow = maskBits + py * maskBpr;

		for (int px = 0; px < width; px++) {
			uint8 maskB = maskRow[px * 4 + 0];
			uint8 maskG = maskRow[px * 4 + 1];
			uint8 maskR = maskRow[px * 4 + 2];
			uint8 maskA = maskRow[px * 4 + 3];

			uint32 luminance = (54 * maskR + 183 * maskG + 19 * maskB) >> 8;
			if (luminance > 255)
				luminance = 255;

			uint32 maskOpacity = (luminance * maskA) / 255;

			uint8 contentA = contentRow[px * 4 + 3];
			uint32 newAlpha = (contentA * maskOpacity) / 255;

			contentRow[px * 4 + 3] = (uint8)newAlpha;

			if (contentA > 0 && newAlpha < contentA) {
				uint32 ratio = (newAlpha * 255) / contentA;
				contentRow[px * 4 + 0] = (uint8)((contentRow[px * 4 + 0] * ratio) / 255);
				contentRow[px * 4 + 1] = (uint8)((contentRow[px * 4 + 1] * ratio) / 255);
				contentRow[px * 4 + 2] = (uint8)((contentRow[px * 4 + 2] * ratio) / 255);
			}
		}
	}
}


void
BSVGView::_DrawShapeWithMask(NSVGshape* shape, int32 shapeIndex)
{
	NSVGmask* mask = shape->mask;
	if (!mask || !mask->shapes)
		return;

	BRect viewBounds = Bounds();
	BRect shapeBounds(
		shape->bounds[0] * fScale + fOffsetX,
		shape->bounds[1] * fScale + fOffsetY,
		shape->bounds[2] * fScale + fOffsetX,
		shape->bounds[3] * fScale + fOffsetY);

	float expand = shape->strokeWidth * fScale * shape->miterLimit;
	shapeBounds.InsetBy(-expand, -expand);

	if (!shapeBounds.Intersects(viewBounds))
		return;

	BRect renderBounds = shapeBounds & viewBounds;
	if (!renderBounds.IsValid())
		return;

	int width = (int)ceilf(renderBounds.Width()) + 1;
	int height = (int)ceilf(renderBounds.Height()) + 1;

	if (width <= 0 || height <= 0)
		return;

	float downsample = 1.0f;
	if (width > kMaxMaskDimension || height > kMaxMaskDimension) {
		downsample = fmaxf((float)width / kMaxMaskDimension,
						   (float)height / kMaxMaskDimension);
		width = (int)(width / downsample);
		height = (int)(height / downsample);
	}

	if (width <= 0)
		width = 1;
	if (height <= 0)
		height = 1;

	BBitmap* contentBitmap = new BBitmap(
		BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (!contentBitmap || contentBitmap->InitCheck() != B_OK) {
		delete contentBitmap;
		return;
	}

	BBitmap* maskBitmap = new BBitmap(
		BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (!maskBitmap || maskBitmap->InitCheck() != B_OK) {
		delete contentBitmap;
		delete maskBitmap;
		return;
	}

	memset(contentBitmap->Bits(), 0, contentBitmap->BitsLength());
	memset(maskBitmap->Bits(), 0, maskBitmap->BitsLength());

	BRect adjustedRenderBounds = renderBounds;
	if (downsample > 1.0f) {
		float savedScale = fScale;
		fScale = fScale / downsample;

		float savedOffsetX = fOffsetX;
		float savedOffsetY = fOffsetY;
		fOffsetX = (fOffsetX - renderBounds.left) / downsample;
		fOffsetY = (fOffsetY - renderBounds.top) / downsample;

		adjustedRenderBounds = BRect(0, 0, width - 1, height - 1);

		_RenderShapeToBuffer(shape, contentBitmap, adjustedRenderBounds);
		_RenderMaskToBuffer(mask, maskBitmap, adjustedRenderBounds);

		fScale = savedScale;
		fOffsetX = savedOffsetX;
		fOffsetY = savedOffsetY;
	} else {
		_RenderShapeToBuffer(shape, contentBitmap, renderBounds);
		_RenderMaskToBuffer(mask, maskBitmap, renderBounds);
	}

	_ApplyMaskToBitmap(contentBitmap, maskBitmap);

	SetDrawingMode(B_OP_ALPHA);
	DrawBitmap(contentBitmap, contentBitmap->Bounds(), renderBounds);

	delete contentBitmap;
	delete maskBitmap;
}


void
BSVGView::_DrawShape(NSVGshape* shape, int32 shapeIndex)
{
	if (!shape)
		return;

	if (shape->mask != NULL && shape->mask->shapes != NULL) {
		_DrawShapeWithMask(shape, shapeIndex);
		return;
	}

	BRect viewBounds = Bounds();
	BRect shapeBounds(
		shape->bounds[0] * fScale + fOffsetX,
		shape->bounds[1] * fScale + fOffsetY,
		shape->bounds[2] * fScale + fOffsetX,
		shape->bounds[3] * fScale + fOffsetY);

	float expand = shape->strokeWidth * fScale * shape->miterLimit;
	shapeBounds.InsetBy(-expand, -expand);

	if (!shapeBounds.Intersects(viewBounds))
		return;

	SetDrawingMode(B_OP_ALPHA);

	bool drawFill = (fDisplayMode == SVG_DISPLAY_NORMAL
		|| fDisplayMode == SVG_DISPLAY_FILL_ONLY);
	bool drawStroke = (fDisplayMode == SVG_DISPLAY_NORMAL
		|| fDisplayMode == SVG_DISPLAY_STROKE_ONLY);
	bool drawOutline = (fDisplayMode == SVG_DISPLAY_OUTLINE);

	if (drawOutline) {
		PushState();
		SetHighColor(0, 0, 0);
		SetPenSize(1.0f);
		SetLineMode(B_BUTT_CAP, B_MITER_JOIN, 4.0f);

		for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
			BShape bShape;
			_ConvertPath(path, bShape);
			StrokeShape(&bShape);
		}

		PopState();
		return;
	}

	if (drawFill && shape->fill.type != NSVG_PAINT_NONE && shape->paths) {
		PushState();

		BShape fillShape;
		for (NSVGpath* path = shape->paths; path != NULL; path = path->next)
			_ConvertPath(path, fillShape);

		BRect fillBounds = fillShape.Bounds();

		switch (shape->fill.type) {
			case NSVG_PAINT_COLOR:
			{
				rgb_color color = _ConvertColor(shape->fill.color, shape->opacity);
				SetHighColor(color);
				FillShape(&fillShape);
				break;
			}

			case NSVG_PAINT_LINEAR_GRADIENT:
			case NSVG_PAINT_RADIAL_GRADIENT:
			{
				BGradient* gradient = NULL;
				_SetupGradient(shape->fill.gradient, fillBounds,
					shape->fill.type, &gradient, shape->opacity);

				if (gradient) {
					FillShape(&fillShape, *gradient);
					delete gradient;
				} else {
					if (!fillBounds.Intersects(viewBounds))
						break;

					BRect clippedBounds = fillBounds & viewBounds;
					if (!clippedBounds.IsValid())
						break;

					BBitmap* gradientBitmap = _RasterizeGradient(
						shape->fill.gradient, shape->fill.type,
						fillBounds, clippedBounds, shape->opacity);

					if (gradientBitmap) {
						_FillShapeWithGradientBitmap(fillShape,
							gradientBitmap, fillBounds, clippedBounds);
						delete gradientBitmap;
					} else if (shape->fill.gradient
						&& shape->fill.gradient->nstops > 0) {
						rgb_color color = _ConvertColor(
							shape->fill.gradient->stops[0].color,
							shape->opacity);
						SetHighColor(color);
						FillShape(&fillShape);
					}
				}
				break;
			}

			default:
				break;
		}

		PopState();
	}

	if (drawStroke && shape->stroke.type != NSVG_PAINT_NONE
		&& shape->strokeWidth > 0.0f) {

		switch (shape->stroke.type) {
			case NSVG_PAINT_COLOR:
			{
				PushState();
				_SetupStrokeStyle(shape);

				rgb_color color = _ConvertColor(shape->stroke.color, shape->opacity);
				SetHighColor(color);

				for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
					BShape strokeShape;
					_ConvertPath(path, strokeShape);
					StrokeShape(&strokeShape);
				}

				PopState();
				break;
			}

			case NSVG_PAINT_LINEAR_GRADIENT:
			case NSVG_PAINT_RADIAL_GRADIENT:
			{
				BShape* strokeAsFill = _ConvertStrokeToFillShape(shape);
				if (!strokeAsFill) {
					if (shape->stroke.gradient && shape->stroke.gradient->nstops > 0) {
						PushState();
						_SetupStrokeStyle(shape);
						int midIdx = shape->stroke.gradient->nstops / 2;
						rgb_color color = _ConvertColor(
							shape->stroke.gradient->stops[midIdx].color,
							shape->opacity);
						SetHighColor(color);
						for (NSVGpath* path = shape->paths; path != NULL;
							path = path->next) {
							BShape strokeShape;
							_ConvertPath(path, strokeShape);
							StrokeShape(&strokeShape);
						}
						PopState();
					}
					break;
				}

				BRect strokeFillBounds = strokeAsFill->Bounds();

				BGradient* gradient = NULL;
				_SetupGradient(shape->stroke.gradient, strokeFillBounds,
					shape->stroke.type, &gradient, shape->opacity);

				if (gradient) {
					PushState();
					SetDrawingMode(B_OP_ALPHA);
					FillShape(strokeAsFill, *gradient);
					PopState();
					delete gradient;
					delete strokeAsFill;
				} else {
					delete strokeAsFill;
					_StrokeShapeWithRasterizedGradient(shape, shape->stroke.type);
				}
				break;
			}

			default:
			{
				PushState();
				_SetupStrokeStyle(shape);
				SetHighColor(0, 0, 0);

				for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
					BShape strokeShape;
					_ConvertPath(path, strokeShape);
					StrokeShape(&strokeShape);
				}

				PopState();
				break;
			}
		}
	}
}


void
BSVGView::_DrawHighlight()
{
	if (fHighlightInfo.mode == SVG_HIGHLIGHT_NONE || !fSVGImage)
		return;

	int32 currentIndex = 0;
	NSVGshape* shape = fSVGImage->shapes;

	while (shape && currentIndex < fHighlightInfo.shapeIndex) {
		shape = shape->next;
		currentIndex++;
	}

	if (!shape)
		return;

	PushState();
	SetDrawingMode(B_OP_ALPHA);

	if (fHighlightInfo.mode == SVG_HIGHLIGHT_SHAPE) {
		_DrawShapeHighlight(shape);
	} else if (fHighlightInfo.mode == SVG_HIGHLIGHT_PATH
		|| fHighlightInfo.mode == SVG_HIGHLIGHT_CONTROL_POINTS) {
		int32 pathIndex = 0;
		NSVGpath* path = shape->paths;

		while (path && pathIndex < fHighlightInfo.pathIndex) {
			path = path->next;
			pathIndex++;
		}

		if (path) {
			_DrawPathHighlight(shape, path);

			if (fHighlightInfo.showControlPoints)
				_DrawControlPoints(path);

			if (fHighlightInfo.showBezierHandles)
				_DrawBezierHandles(path);
		}
	}

	PopState();
}


void
BSVGView::_DrawShapeHighlight(NSVGshape* shape)
{
	if (!shape)
		return;

	for (NSVGpath* path = shape->paths; path != NULL; path = path->next)
		_DrawHighlightOutline(path, 4.0f);

	if (shape->mask != NULL && shape->mask->shapes != NULL) {
		for (NSVGshape* maskShape = shape->mask->shapes; maskShape != NULL;
			 maskShape = maskShape->next) {
			for (NSVGpath* path = maskShape->paths; path != NULL; path = path->next)
				_DrawHighlightOutline(path, 4.0f);
		}
	}
}


void
BSVGView::_DrawPathHighlight(NSVGshape* shape, NSVGpath* path)
{
	if (!path)
		return;

	_DrawHighlightOutline(path, 3.0f);
}


void
BSVGView::_DrawHighlightOutline(NSVGpath* path, float width)
{
	if (!path || path->npts < 2)
		return;

	BShape highlightShape;
	_ConvertPath(path, highlightShape);

	SetHighColor(255, 255, 255, 180);
	SetPenSize(width + 2.0f);
	SetLineMode(B_ROUND_CAP, B_ROUND_JOIN, 10.0f);
	StrokeShape(&highlightShape);

	SetHighColor(255, 100, 0, 220);
	SetPenSize(width);
	StrokeShape(&highlightShape);
}


void
BSVGView::_DrawControlPoints(NSVGpath* path)
{
	if (!path)
		return;

	for (int i = 0; i < path->npts; i++) {
		BPoint point = _ConvertSVGPoint(path->pts[i * 2], path->pts[i * 2 + 1]);
		bool isEndPoint = (i == 0) || ((i > 0) && ((i - 1) % 3 == 2));
		_DrawControlPoint(point, isEndPoint, false);
	}
}


void
BSVGView::_DrawBezierHandles(NSVGpath* path)
{
	if (!path)
		return;

	for (int i = 0; i < path->npts; i += 3) {
		if (i + 2 < path->npts) {
			BPoint anchor1 = _ConvertSVGPoint(path->pts[i * 2],
				path->pts[i * 2 + 1]);
			BPoint control1 = _ConvertSVGPoint(path->pts[(i + 1) * 2],
				path->pts[(i + 1) * 2 + 1]);
			BPoint control2 = _ConvertSVGPoint(path->pts[(i + 2) * 2],
				path->pts[(i + 2) * 2 + 1]);

			BPoint anchor2;
			if (i + 3 < path->npts) {
				anchor2 = _ConvertSVGPoint(path->pts[(i + 3) * 2],
					path->pts[(i + 3) * 2 + 1]);
			} else if (path->closed && path->npts > 3) {
				anchor2 = _ConvertSVGPoint(path->pts[0], path->pts[1]);
			} else {
				continue;
			}

			SetHighColor(100, 100, 100, 196);
			SetPenSize(1.0f);
			SetLineMode(B_BUTT_CAP, B_MITER_JOIN, 4.0f);

			if (control1 != anchor1)
				StrokeLine(anchor1, control1);

			if (control2 != anchor2)
				StrokeLine(anchor2, control2);

			if (control1 != anchor1)
				_DrawControlPoint(control1, false, false);

			if (control2 != anchor2)
				_DrawControlPoint(control2, false, false);
		}
	}
}


void
BSVGView::_DrawControlPoint(BPoint point, bool isEndPoint, bool isSelected)
{
	float size = _GetControlPointSize();
	BRect pointRect(point.x - size / 2, point.y - size / 2,
		point.x + size / 2, point.y + size / 2);

	SetHighColor(255, 255, 255, 240);

	if (isEndPoint)
		FillRect(pointRect);
	else
		FillEllipse(pointRect);

	if (isSelected)
		SetHighColor(255, 0, 0, 255);
	else if (isEndPoint)
		SetHighColor(0, 0, 0, 255);
	else
		SetHighColor(100, 100, 255, 255);

	SetPenSize(1.5f);
	if (isEndPoint)
		StrokeRect(pointRect);
	else
		StrokeEllipse(pointRect);

	rgb_color fillColor;
	if (isSelected)
		fillColor = (rgb_color){255, 100, 100, 200};
	else if (isEndPoint)
		fillColor = (rgb_color){220, 220, 220, 200};
	else
		fillColor = (rgb_color){180, 180, 255, 200};

	SetHighColor(fillColor);
	pointRect.InsetBy(1, 1);
	if (isEndPoint)
		FillRect(pointRect);
	else
		FillEllipse(pointRect);
}


BPoint
BSVGView::_ConvertSVGPoint(float x, float y) const
{
	return BPoint(x * fScale + fOffsetX, y * fScale + fOffsetY);
}


float
BSVGView::_GetControlPointSize() const
{
	return 8.0f;
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
			BPoint control1(path->pts[i * 2] * fScale + fOffsetX,
				path->pts[i * 2 + 1] * fScale + fOffsetY);
			BPoint control2(path->pts[(i + 1) * 2] * fScale + fOffsetX,
				path->pts[(i + 1) * 2 + 1] * fScale + fOffsetY);
			BPoint endPoint(path->pts[(i + 2) * 2] * fScale + fOffsetX,
				path->pts[(i + 2) * 2 + 1] * fScale + fOffsetY);

			shape.BezierTo(control1, control2, endPoint);
		}
	}

	if (path->closed)
		shape.Close();
}


bool
BSVGView::_IsUniformScale(float* xform)
{
	float scaleX = sqrtf(xform[0] * xform[0] + xform[1] * xform[1]);
	float scaleY = sqrtf(xform[2] * xform[2] + xform[3] * xform[3]);

	const float epsilon = 0.001f;
	return fabsf(scaleX - scaleY) < epsilon;
}


bool
BSVGView::_HasRotationOrSkew(float* xform)
{
	const float epsilon = 0.001f;
	return fabsf(xform[1]) > epsilon || fabsf(xform[2]) > epsilon;
}


void
BSVGView::_SetupGradient(NSVGgradient* gradient, BRect bounds, char gradientType,
	BGradient** outGradient, float shapeOpacity)
{
	if (!gradient || !outGradient || gradient->nstops == 0) {
		*outGradient = NULL;
		return;
	}

	float* t = gradient->xform;

	double det = (double)t[0] * t[3] - (double)t[2] * t[1];
	if (fabs(det) < 1e-6) {
		*outGradient = NULL;
		return;
	}

	double invdet = 1.0 / det;
	float inv[6];
	inv[0] = (float)(t[3] * invdet);
	inv[1] = (float)(-t[1] * invdet);
	inv[2] = (float)(-t[2] * invdet);
	inv[3] = (float)(t[0] * invdet);
	inv[4] = (float)(((double)t[2] * t[5] - (double)t[3] * t[4]) * invdet);
	inv[5] = (float)(((double)t[1] * t[4] - (double)t[0] * t[5]) * invdet);

	BGradient* bgradient = NULL;

	if (gradientType == NSVG_PAINT_LINEAR_GRADIENT) {
		if (_HasRotationOrSkew(t)) {
			*outGradient = NULL;
			return;
		}

		float x1_obj = inv[4];
		float y1_obj = inv[5];

		float x2_obj = inv[2] + inv[4];
		float y2_obj = inv[3] + inv[5];

		BPoint start(x1_obj * fScale + fOffsetX, y1_obj * fScale + fOffsetY);
		BPoint end(x2_obj * fScale + fOffsetX, y2_obj * fScale + fOffsetY);

		bgradient = new BGradientLinear(start, end);

	} else if (gradientType == NSVG_PAINT_RADIAL_GRADIENT) {
		if (!_IsUniformScale(t)) {
			*outGradient = NULL;
			return;
		}

		float cx_obj = inv[4];
		float cy_obj = inv[5];

		float radius_obj = sqrtf(inv[0] * inv[0] + inv[1] * inv[1]);

		float fx = gradient->fx;
		float fy = gradient->fy;
		float fx_obj = inv[0] * fx + inv[2] * fy + inv[4];
		float fy_obj = inv[1] * fx + inv[3] * fy + inv[5];

		BPoint center(cx_obj * fScale + fOffsetX, cy_obj * fScale + fOffsetY);
		BPoint focal(fx_obj * fScale + fOffsetX, fy_obj * fScale + fOffsetY);
		float radius_view = radius_obj * fScale;

		float focalDistSq = (fx_obj - cx_obj) * (fx_obj - cx_obj)
			+ (fy_obj - cy_obj) * (fy_obj - cy_obj);

		if (focalDistSq > 0.0001f)
			bgradient = new BGradientRadialFocus(center, radius_view, focal);
		else
			bgradient = new BGradientRadial(center, radius_view);
	}

	if (bgradient) {
		for (int i = 0; i < gradient->nstops; i++) {
			NSVGgradientStop* stop = &gradient->stops[i];
			unsigned int stopColor = stop->color;
			uint8 stopAlpha = (stopColor >> 24) & 0xFF;
			float alphaFloat = stopAlpha / 255.0f;
			float combinedAlpha = alphaFloat * shapeOpacity;
			rgb_color color = _ConvertColor(stopColor, combinedAlpha);
			bgradient->AddColor(color, stop->offset * 255.0f);
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
	result.alpha = (uint8)(alpha * opacity);

	return result;
}


rgb_color
BSVGView::_InterpolateGradientColor(NSVGgradient* gradient, float t, float opacity)
{
	if (!gradient || gradient->nstops == 0)
		return (rgb_color){0, 0, 0, 0};

	if (t < 0.0f)
		t = 0.0f;
	if (t > 1.0f)
		t = 1.0f;

	int stop0 = 0;
	int stop1 = gradient->nstops - 1;

	for (int i = 0; i < gradient->nstops - 1; i++) {
		if (t >= gradient->stops[i].offset && t <= gradient->stops[i + 1].offset) {
			stop0 = i;
			stop1 = i + 1;
			break;
		}
	}

	float offset0 = gradient->stops[stop0].offset;
	float offset1 = gradient->stops[stop1].offset;
	float range = offset1 - offset0;
	float localT = (range > 0.0001f) ? (t - offset0) / range : 0.0f;
	if (localT < 0.0f)
		localT = 0.0f;
	if (localT > 1.0f)
		localT = 1.0f;

	unsigned int c0 = gradient->stops[stop0].color;
	unsigned int c1 = gradient->stops[stop1].color;

	uint8 r0 = (c0 >> 0) & 0xFF;
	uint8 g0 = (c0 >> 8) & 0xFF;
	uint8 b0 = (c0 >> 16) & 0xFF;
	uint8 a0 = (c0 >> 24) & 0xFF;

	uint8 r1 = (c1 >> 0) & 0xFF;
	uint8 g1 = (c1 >> 8) & 0xFF;
	uint8 b1 = (c1 >> 16) & 0xFF;
	uint8 a1 = (c1 >> 24) & 0xFF;

	rgb_color result;
	result.red = (uint8)(r0 + (int)(r1 - r0) * localT);
	result.green = (uint8)(g0 + (int)(g1 - g0) * localT);
	result.blue = (uint8)(b0 + (int)(b1 - b0) * localT);

	float alpha0 = a0 / 255.0f;
	float alpha1 = a1 / 255.0f;
	float interpolatedAlpha = alpha0 + (alpha1 - alpha0) * localT;
	result.alpha = (uint8)(interpolatedAlpha * opacity * 255.0f);

	return result;
}


BBitmap*
BSVGView::_RasterizeGradient(NSVGgradient* gradient, char gradientType,
	BRect shapeBounds, BRect clippedBounds, float shapeOpacity)
{
	if (!gradient)
		return NULL;

	if (!clippedBounds.IsValid())
		return NULL;

	int width = (int)ceilf(clippedBounds.Width()) + 1;
	int height = (int)ceilf(clippedBounds.Height()) + 1;

	float downsample = 1.0f;

	if (width > kMaxGradientDimension || height > kMaxGradientDimension) {
		float ratioW = (float)width / kMaxGradientDimension;
		float ratioH = (float)height / kMaxGradientDimension;
		downsample = (ratioW > ratioH) ? ratioW : ratioH;
		width = (int)(width / downsample);
		height = (int)(height / downsample);
	}

	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	BBitmap* bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (!bitmap || bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}

	uint8* bits = (uint8*)bitmap->Bits();
	int32 bpr = bitmap->BytesPerRow();

	GradientLUT lut;
	_BuildGradientLUT(gradient, shapeOpacity, lut);

	float* m = gradient->xform;
	float invScale = 1.0f / fScale;
	float baseX = (clippedBounds.left - fOffsetX) * invScale;
	float baseY = (clippedBounds.top - fOffsetY) * invScale;
	float stepX = downsample * invScale;
	float stepY = downsample * invScale;

	if (gradientType == NSVG_PAINT_LINEAR_GRADIENT) {
		float dtdx = m[1] * stepX;
		float dtdy = m[3] * stepY;
		float tBase = m[1] * baseX + m[3] * baseY + m[5];

		for (int py = 0; py < height; py++) {
			uint8* row = bits + py * bpr;
			float tRow = tBase + dtdy * py;

			for (int px = 0; px < width; px++) {
				float t = _ApplySpreadMode(gradient->spread, tRow);

				int idx = (int)(t * 255.0f + 0.5f);
				if (idx < 0)
					idx = 0;
				if (idx > 255)
					idx = 255;

				rgb_color color = lut.colors[idx];

				row[px * 4 + 0] = color.blue;
				row[px * 4 + 1] = color.green;
				row[px * 4 + 2] = color.red;
				row[px * 4 + 3] = color.alpha;

				tRow += dtdx;
			}
		}
	} else {
		float fx = gradient->fx;
		float fy = gradient->fy;
		float focalDist = sqrtf(fx * fx + fy * fy);
		bool hasFocalPoint = focalDist >= 0.001f;

		for (int py = 0; py < height; py++) {
			uint8* row = bits + py * bpr;
			float svgY = baseY + py * stepY;

			for (int px = 0; px < width; px++) {
				float svgX = baseX + px * stepX;

				float gx = m[0] * svgX + m[2] * svgY + m[4];
				float gy = m[1] * svgX + m[3] * svgY + m[5];

				float t;

				if (!hasFocalPoint) {
					t = sqrtf(gx * gx + gy * gy);
				} else {
					float angle = atan2f(gy, gx);
					float cosA = cosf(angle);
					float sinA = sinf(angle);

					float a = 1.0f;
					float b = -2.0f * (fx * cosA + fy * sinA);
					float c = fx * fx + fy * fy - 1.0f;
					float discriminant = b * b - 4.0f * a * c;

					if (discriminant >= 0) {
						float sqrtD = sqrtf(discriminant);
						float t1 = (-b + sqrtD) / (2.0f * a);
						float t2 = (-b - sqrtD) / (2.0f * a);
						float edgeDist = (t1 > 0) ? t1 : t2;

						float focalToPoint = sqrtf((gx - fx) * (gx - fx)
							+ (gy - fy) * (gy - fy));
						float focalToEdgeX = edgeDist * cosA - fx;
						float focalToEdgeY = edgeDist * sinA - fy;
						float focalToEdge = sqrtf(focalToEdgeX * focalToEdgeX
							+ focalToEdgeY * focalToEdgeY);

						t = (focalToEdge > 0.001f) ? focalToPoint / focalToEdge : 0.0f;
					} else {
						t = sqrtf(gx * gx + gy * gy);
					}
				}

				t = _ApplySpreadMode(gradient->spread, t);

				int idx = (int)(t * 255.0f + 0.5f);
				if (idx < 0)
					idx = 0;
				if (idx > 255)
					idx = 255;

				rgb_color color = lut.colors[idx];

				row[px * 4 + 0] = color.blue;
				row[px * 4 + 1] = color.green;
				row[px * 4 + 2] = color.red;
				row[px * 4 + 3] = color.alpha;
			}
		}
	}

	return bitmap;
}


void
BSVGView::_FillShapeWithGradientBitmap(BShape& shape, BBitmap* bitmap,
	BRect shapeBounds, BRect clippedBounds)
{
	if (!bitmap)
		return;

	PushState();

	ClipToShape(&shape);

	SetDrawingMode(B_OP_ALPHA);
	DrawBitmap(bitmap, bitmap->Bounds(), clippedBounds);

	PopState();
}


void
BSVGView::_SetupStrokeStyle(NSVGshape* shape)
{
	float scaledWidth = shape->strokeWidth * fScale;
	if (scaledWidth < 0.1f)
		scaledWidth = 0.1f;
	SetPenSize(scaledWidth);

	SetLineMode(_ConvertLineCapHaiku(shape->strokeLineCap),
		_ConvertLineJoinHaiku(shape->strokeLineJoin),
		_ClampMiterLimit(shape->miterLimit));
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
BSVGView::_DrawTransparencyGrid()
{
	float cellSize = 24.0f;
	BRect bounds = Bounds();

	int cols = (int)ceilf(bounds.Width() / cellSize) + 1;
	int rows = (int)ceilf(bounds.Height() / cellSize) + 1;

	for (int x = 0; x < cols; x++) {
		for (int y = 0; y < rows; y++) {
			if ((x + y) % 2)
				SetHighColor(230, 230, 230);
			else
				SetHighColor(200, 200, 200);

			FillRect(BRect(x * cellSize, y * cellSize,
				(x + 1) * cellSize, (y + 1) * cellSize));
		}
	}
}


void
BSVGView::_DrawBoundingBox()
{
	if (!fSVGImage || fBoundingBoxStyle == SVG_BBOX_NONE)
		return;

	BRect svgBounds = SVGViewBounds();

	switch (fBoundingBoxStyle) {
		case SVG_BBOX_DOCUMENT:
			_DrawDocumentStyle(svgBounds);
			break;
		case SVG_BBOX_SIMPLE_FRAME:
			_DrawSimpleFrame(svgBounds);
			break;
		case SVG_BBOX_TRANSPARENT_GRAY:
			_DrawTransparentGray(svgBounds);
			break;
		default:
			break;
	}
}


void
BSVGView::_DrawDocumentStyle(BRect bounds)
{
	PushState();

	BRect shadowRect = bounds;
	shadowRect.OffsetBy(3, 3);
	SetHighColor(0, 0, 0, 60);
	SetDrawingMode(B_OP_ALPHA);
	FillRect(shadowRect);

	SetHighColor(255, 255, 255);
	SetDrawingMode(B_OP_COPY);
	FillRect(bounds);

	SetHighColor(180, 180, 180);
	SetPenSize(1.0f);
	StrokeRect(bounds);

	PopState();
}


void
BSVGView::_DrawSimpleFrame(BRect bounds)
{
	PushState();

	SetHighColor(100, 100, 100);
	SetPenSize(1.0f);
	SetDrawingMode(B_OP_COPY);
	StrokeRect(bounds);

	PopState();
}


void
BSVGView::_DrawTransparentGray(BRect bounds)
{
	PushState();

	SetHighColor(128, 128, 128, 80);
	SetDrawingMode(B_OP_ALPHA);
	FillRect(bounds);

	PopState();
}
