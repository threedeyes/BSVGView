/*
 * Copyright 2020-2025, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS

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
	fAutoScale(true),
	fDisplayMode(SVG_DISPLAY_NORMAL),
	fShowTransparency(true),
	fBoundingBoxStyle(SVG_BBOX_NONE)
{
}

BSVGView::BSVGView(const char* name)
	: BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fSVGImage(NULL),
	fScale(1.0f),
	fOffsetX(0.0f),
	fOffsetY(0.0f),
	fAutoScale(true),
	fDisplayMode(SVG_DISPLAY_NORMAL),
	fShowTransparency(true),
	fBoundingBoxStyle(SVG_BBOX_NONE)
{
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

	if (fBoundingBoxStyle != SVG_BBOX_NONE) {
		_DrawBoundingBox();
	}

	int32 shapeIndex = 0;
	for (NSVGshape* shape = fSVGImage->shapes; shape != NULL; shape = shape->next) {
		if (shape->flags & NSVG_FLAGS_VISIBLE) {
			_DrawShape(shape, shapeIndex);
		}
		shapeIndex++;
	}

	// Draw highlight on top of everything
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
	if (scale > 0.0f) {
		fScale = scale;
		Invalidate();
	}
}

void
BSVGView::SetOffset(BPoint point)
{
	fOffsetX = point.x;
	fOffsetY = point.y;
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
BSVGView::SetHighlightControlPoints(int32 shapeIndex, int32 pathIndex, bool showBezierHandles)
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
	return fSVGImage ? BRect(0, 0, fSVGImage->width - 1, fSVGImage->height - 1) : BRect();
}

BRect
BSVGView::SVGViewBounds() const
{
	if (!fSVGImage)
		return BRect();

	float scaledWidth = fSVGImage->width * fScale;
	float scaledHeight = fSVGImage->height * fScale;

	return BRect(fOffsetX, fOffsetY,
				fOffsetX + scaledWidth,
				fOffsetY + scaledHeight);
}

void
BSVGView::_DrawShape(NSVGshape* shape, int32 shapeIndex)
{
	if (!shape)
		return;

	SetDrawingMode(B_OP_ALPHA);

	bool drawFill = (fDisplayMode == SVG_DISPLAY_NORMAL || fDisplayMode == SVG_DISPLAY_FILL_ONLY);
	bool drawStroke = (fDisplayMode == SVG_DISPLAY_NORMAL || fDisplayMode == SVG_DISPLAY_STROKE_ONLY);
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
		for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
			_ConvertPath(path, fillShape);
		}

		_ApplyFillPaint(&shape->fill, shape->opacity);

		if (shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT ||
			shape->fill.type == NSVG_PAINT_RADIAL_GRADIENT) {
			BGradient* gradient = NULL;
			_SetupGradient(shape->fill.gradient, fillShape.Bounds(), shape->fill.type, &gradient, shape->opacity);
			if (gradient) {
				FillShape(&fillShape, *gradient);
				delete gradient;
			} else {
				if (shape->fill.gradient && shape->fill.gradient->nstops > 0) {
					rgb_color color = _ConvertColor(shape->fill.gradient->stops[0].color, shape->opacity);
					SetHighColor(color);
				}
				FillShape(&fillShape);
			}
		} else {
			FillShape(&fillShape);
		}

		PopState();
	}

	if (drawStroke && shape->stroke.type != NSVG_PAINT_NONE && shape->strokeWidth > 0.0f) {
		PushState();

		_ApplyStrokePaint(&shape->stroke, shape->opacity);

		if (shape->stroke.type == NSVG_PAINT_LINEAR_GRADIENT ||
			shape->stroke.type == NSVG_PAINT_RADIAL_GRADIENT) {
			BGradient* strokeGradient = NULL;
			_SetupGradient(shape->stroke.gradient, Bounds(), shape->stroke.type, &strokeGradient, shape->opacity);
			if (strokeGradient)
				delete strokeGradient;
			if (shape->stroke.gradient && shape->stroke.gradient->nstops > 0) {
				rgb_color color = _ConvertColor(shape->stroke.gradient->stops[0].color, shape->opacity);
				SetHighColor(color);
			}
		}

		_SetupStrokeStyle(shape);

		for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
			BShape strokeShape;
			_ConvertPath(path, strokeShape);
			StrokeShape(&strokeShape);
		}

		PopState();
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
	} else if (fHighlightInfo.mode == SVG_HIGHLIGHT_PATH ||
			   fHighlightInfo.mode == SVG_HIGHLIGHT_CONTROL_POINTS) {
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

	SetHighColor(255, 100, 0, 220); // Orange
	SetPenSize(width);
	StrokeShape(&highlightShape);
}

void
BSVGView::_DrawControlPoints(NSVGpath* path)
{
	if (!path)
		return;

	for (int i = 0; i < path->npts; i++) {
		BPoint point = _ConvertSVGPoint(path->pts[i*2], path->pts[i*2 + 1]);
		bool isEndPoint = (i % 3 == 0);
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
			BPoint anchor1 = _ConvertSVGPoint(path->pts[i*2], path->pts[i*2 + 1]);
			BPoint control1 = _ConvertSVGPoint(path->pts[(i+1)*2], path->pts[(i+1)*2 + 1]);
			BPoint control2 = _ConvertSVGPoint(path->pts[(i+2)*2], path->pts[(i+2)*2 + 1]);

			BPoint anchor2;
			if (i + 3 < path->npts) {
				anchor2 = _ConvertSVGPoint(path->pts[(i+3)*2], path->pts[(i+3)*2 + 1]);
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
	BRect pointRect(point.x - size/2, point.y - size/2,
					point.x + size/2, point.y + size/2);

	SetHighColor(255, 255, 255, 240);

	if (isEndPoint)
		FillRect(pointRect);
	else
		FillEllipse(pointRect);

	if (isSelected) {
		SetHighColor(255, 0, 0, 255); // Red for selected
	} else if (isEndPoint) {
		SetHighColor(0, 0, 0, 255); // Black for endpoints
	} else {
		SetHighColor(100, 100, 255, 255); // Blue for control points
	}

	SetPenSize(1.5f);
	if (isEndPoint)
		StrokeRect(pointRect);
	else
		StrokeEllipse(pointRect);

	rgb_color fillColor;
	if (isSelected) {
		fillColor = {255, 100, 100, 200};
	} else if (isEndPoint) {
		fillColor = {220, 220, 220, 200};
	} else {
		fillColor = {180, 180, 255, 200};
	}

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
	// TODO: add logic for dynamic size calculation
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
BSVGView::_SetupGradient(NSVGgradient* gradient, BRect bounds, char gradientType, BGradient** outGradient, float shapeOpacity)
{
	if (!gradient || !outGradient || gradient->nstops == 0) {
		*outGradient = NULL;
		return;
	}

	float* t = gradient->xform;
	float inv[6];

	double det = (double)t[0] * t[3] - (double)t[2] * t[1];
	if (fabs(det) < 1e-6) {
		*outGradient = NULL;
		return;
	}

	double invdet = 1.0 / det;
	inv[0] = (float)(t[3] * invdet);
	inv[1] = (float)(-t[1] * invdet);
	inv[2] = (float)(-t[2] * invdet);
	inv[3] = (float)(t[0] * invdet);
	inv[4] = (float)(((double)t[2] * t[5] - (double)t[3] * t[4]) * invdet);
	inv[5] = (float)(((double)t[1] * t[4] - (double)t[0] * t[5]) * invdet);

	BGradient* bgradient = NULL;

	if (gradientType == NSVG_PAINT_LINEAR_GRADIENT) {
		float x1_obj = inv[4];
		float y1_obj = inv[5];

		float x2_obj = inv[2] + inv[4];
		float y2_obj = inv[3] + inv[5];

		BPoint start(x1_obj * fScale + fOffsetX, y1_obj * fScale + fOffsetY);
		BPoint end(x2_obj * fScale + fOffsetX, y2_obj * fScale + fOffsetY);

		bgradient = new BGradientLinear(start, end);

	} else if (gradientType == NSVG_PAINT_RADIAL_GRADIENT) {
		float cx_obj = inv[4];
		float cy_obj = inv[5];
		float rvx_obj = inv[0];
		float rvy_obj = inv[1];
		float radius_obj = sqrtf(rvx_obj*rvx_obj + rvy_obj*rvy_obj);

		BPoint center(cx_obj * fScale + fOffsetX, cy_obj * fScale + fOffsetY);
		float radius_view = radius_obj * fScale;

		bgradient = new BGradientRadial(center, radius_view);
	}

	if (bgradient) {
		for (int i = 0; i < gradient->nstops; i++) {
			NSVGgradientStop* stop = &gradient->stops[i];
			float stopAlpha = ((stop->color >> 24) & 0xFF) / 255.0f;
			float combinedAlpha = stopAlpha * shapeOpacity;
			rgb_color color = _ConvertColor(stop->color, combinedAlpha);
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

	if (alpha == 0 && ((color >> 24) == 0)) {
		alpha = 255;
	}

	result.alpha = (uint8)(alpha * opacity);

	return result;
}

void
BSVGView::_SetupStrokeStyle(NSVGshape* shape)
{
	float scaledWidth = shape->strokeWidth * fScale;
	if (scaledWidth < 0.1f) scaledWidth = 0.1f;
	SetPenSize(scaledWidth);

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
		default:
			capMode = B_BUTT_CAP;
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
		default:
			joinMode = B_MITER_JOIN;
			break;
	}

	float miterLimit = shape->miterLimit;
	if (miterLimit < 1.0f) miterLimit = 1.0f;
	if (miterLimit > 100.0f) miterLimit = 100.0f;

	SetLineMode(capMode, joinMode, miterLimit);
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
	float cellSize = 24.0;
	BRect bounds = Bounds();

	for (int x = 0; x < bounds.Width() / cellSize; x++) {
		for (int y = 0; y < bounds.Height() / cellSize; y++) {
			if ((x + y) % 2)
				SetHighColor(230, 230, 230);
			else
				SetHighColor(200, 200, 200);

			FillRect(BRect(x * cellSize, y * cellSize, (x + 1) * cellSize, (y + 1) * cellSize));
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
