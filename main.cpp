/*
 * Copyright 2020-2025, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include "BSVGView.h"
#include <Application.h>
#include <Window.h>
#include <Alert.h>
#include <String.h>
#include <MenuBar.h>
#include <Menu.h>
#include <MenuItem.h>
#include <FilePanel.h>
#include <stdio.h>
#include <Path.h>

const uint32 MSG_OPEN_FILE = 'open';
const uint32 MSG_FIT_WINDOW = 'fitw';
const uint32 MSG_ACTUAL_SIZE = 'acts';
const uint32 MSG_CENTER = 'cent';

const uint32 MSG_DISPLAY_NORMAL = 'dpnm';
const uint32 MSG_DISPLAY_OUTLINE = 'dpot';
const uint32 MSG_DISPLAY_FILL = 'dpfl';
const uint32 MSG_DISPLAY_STROKE = 'dpst';

const uint32 MSG_BBOX_NONE = 'bbn0';
const uint32 MSG_BBOX_DOCUMENT = 'bbdc';
const uint32 MSG_BBOX_FRAME = 'bbfr';
const uint32 MSG_BBOX_GRAY = 'bbgr';

const uint32 MSG_TOGGLE_TRANSPARENCY = 'tgtr';

const uint32 MSG_SVG_STATUS_UPDATE = 'svgu';

const uint32 MSG_SHAPE_SELECTED = 'shps';
const uint32 MSG_PATH_SELECTED = 'pths';
const uint32 MSG_CONTROL_POINTS_SELECTED = 'ctps';
const uint32 MSG_CLEAR_SELECTION = 'clrs';


class SVGWindow : public BWindow {
public:
	SVGWindow(const char* filePath = NULL)
		: BWindow(BRect(100, 100, 800, 700), "SVG Viewer",
				B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE),
		fOpenPanel(NULL)
	{
		BRect bounds = Bounds();

		BMenuBar* menuBar = new BMenuBar(BRect(0, 0, bounds.right, 20), "menubar");

		BMenu* fileMenu = new BMenu("File");
		fileMenu->AddItem(new BMenuItem("Open...", new BMessage(MSG_OPEN_FILE), 'O'));
		fileMenu->AddSeparatorItem();
		fileMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
		menuBar->AddItem(fileMenu);

		BMenu* viewMenu = new BMenu("View");
		viewMenu->AddItem(new BMenuItem("Fit to Window", new BMessage(MSG_FIT_WINDOW), 'F'));
		viewMenu->AddItem(new BMenuItem("Actual Size", new BMessage(MSG_ACTUAL_SIZE), '1'));
		viewMenu->AddItem(new BMenuItem("Center", new BMessage(MSG_CENTER), 'C'));
		viewMenu->AddSeparatorItem();

		BMenu* displayMenu = new BMenu("Display Mode");
		displayMenu->AddItem(new BMenuItem("Normal", new BMessage(MSG_DISPLAY_NORMAL)));
		displayMenu->AddItem(new BMenuItem("Outline", new BMessage(MSG_DISPLAY_OUTLINE)));
		displayMenu->AddItem(new BMenuItem("Fill Only", new BMessage(MSG_DISPLAY_FILL)));
		displayMenu->AddItem(new BMenuItem("Stroke Only", new BMessage(MSG_DISPLAY_STROKE)));
		viewMenu->AddItem(displayMenu);

		viewMenu->AddSeparatorItem();
		viewMenu->AddItem(new BMenuItem("Show Transparency Grid", new BMessage(MSG_TOGGLE_TRANSPARENCY), 'T'));
		menuBar->AddItem(viewMenu);

		BMenu* bboxMenu = new BMenu("BoundingBox");
		bboxMenu->AddItem(new BMenuItem("None", new BMessage(MSG_BBOX_NONE)));
		bboxMenu->AddItem(new BMenuItem("Document Style", new BMessage(MSG_BBOX_DOCUMENT)));
		bboxMenu->AddItem(new BMenuItem("Simple Frame", new BMessage(MSG_BBOX_FRAME)));
		bboxMenu->AddItem(new BMenuItem("Transparent Gray", new BMessage(MSG_BBOX_GRAY)));
		menuBar->AddItem(bboxMenu);

		BMenu* highlightMenu = new BMenu("Highlight");
		BMessage* shapeMsg = new BMessage(MSG_SHAPE_SELECTED);
		shapeMsg->AddInt32("shape_index", 0);
		highlightMenu->AddItem(new BMenuItem("Highlight Shape 0", shapeMsg));

		BMessage* pathMsg = new BMessage(MSG_PATH_SELECTED);
		pathMsg->AddInt32("shape_index", 0);
		pathMsg->AddInt32("path_index", 0);
		highlightMenu->AddItem(new BMenuItem("Highlight Path 0:0", pathMsg));

		BMessage* ctrlMsg = new BMessage(MSG_CONTROL_POINTS_SELECTED);
		ctrlMsg->AddInt32("shape_index", 0);
		ctrlMsg->AddInt32("path_index", 0);
		ctrlMsg->AddBool("show_bezier_handles", true);
		highlightMenu->AddItem(new BMenuItem("Show Control Points", ctrlMsg));

		highlightMenu->AddItem(new BMenuItem("Clear Selection", new BMessage(MSG_CLEAR_SELECTION)));
		menuBar->AddItem(highlightMenu);

		AddChild(menuBar);

		BRect svgRect = bounds;
		svgRect.top = menuBar->Bounds().bottom + 1;

		fSVGView = new BSVGView(svgRect, "svg_view");
		AddChild(fSVGView);

		if (filePath)
			LoadFile(filePath);

		_UpdateMenuStates();
	}

	~SVGWindow()
	{
		delete fOpenPanel;
	}

	void LoadFile(const char* filePath)
	{
		if (!filePath) {
			ShowError("File path not specified");
			return;
		}

		status_t result = fSVGView->LoadFromFile(filePath);
		if (result != B_OK) {
			BString error;
			error.SetToFormat("Error loading SVG file: %s", filePath);
			ShowError(error.String());
		} else {
			BPath path(filePath);
			BString title("SVG Viewer - ");
			title << path.Leaf();
			SetTitle(title.String());
		}
	}

	virtual bool QuitRequested()
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
		return true;
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case MSG_OPEN_FILE:
				if (!fOpenPanel) {
					fOpenPanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL, 0, false);
				}
				fOpenPanel->Show();
				break;
			case B_REFS_RECEIVED:
				HandleRefsReceived(message);
				break;
			case MSG_FIT_WINDOW:
				fSVGView->FitToWindow();
				break;
			case MSG_ACTUAL_SIZE:
				fSVGView->ActualSize();
				break;
			case MSG_CENTER:
				fSVGView->CenterImage();
				break;
			case MSG_DISPLAY_NORMAL:
				fSVGView->SetDisplayMode(SVG_DISPLAY_NORMAL);
				_UpdateMenuStates();
				break;
			case MSG_DISPLAY_OUTLINE:
				fSVGView->SetDisplayMode(SVG_DISPLAY_OUTLINE);
				_UpdateMenuStates();
				break;
			case MSG_DISPLAY_FILL:
				fSVGView->SetDisplayMode(SVG_DISPLAY_FILL_ONLY);
				_UpdateMenuStates();
				break;
			case MSG_DISPLAY_STROKE:
				fSVGView->SetDisplayMode(SVG_DISPLAY_STROKE_ONLY);
				_UpdateMenuStates();
				break;
			case MSG_BBOX_NONE:
				fSVGView->SetBoundingBoxStyle(SVG_BBOX_NONE);
				_UpdateMenuStates();
				break;
			case MSG_BBOX_DOCUMENT:
				fSVGView->SetBoundingBoxStyle(SVG_BBOX_DOCUMENT);
				_UpdateMenuStates();
				break;
			case MSG_BBOX_FRAME:
				fSVGView->SetBoundingBoxStyle(SVG_BBOX_SIMPLE_FRAME);
				_UpdateMenuStates();
				break;
			case MSG_BBOX_GRAY:
				fSVGView->SetBoundingBoxStyle(SVG_BBOX_TRANSPARENT_GRAY);
				_UpdateMenuStates();
				break;
			case MSG_TOGGLE_TRANSPARENCY:
				fSVGView->SetShowTransparency(!fSVGView->ShowTransparency());
				_UpdateMenuStates();
				break;
			case MSG_SHAPE_SELECTED:
			{
				int32 shapeIndex;
				if (message->FindInt32("shape_index", &shapeIndex) == B_OK) {
					fSVGView->SetHighlightedShape(shapeIndex);
				}
				break;
			}
			case MSG_PATH_SELECTED:
			{
				int32 shapeIndex, pathIndex;
				if (message->FindInt32("shape_index", &shapeIndex) == B_OK &&
					message->FindInt32("path_index", &pathIndex) == B_OK) {
					fSVGView->SetHighlightedPath(shapeIndex, pathIndex);
				}
				break;
			}
			case MSG_CONTROL_POINTS_SELECTED:
			{
				int32 shapeIndex, pathIndex;
				bool showBezierHandles = false;
				if (message->FindInt32("shape_index", &shapeIndex) == B_OK &&
					message->FindInt32("path_index", &pathIndex) == B_OK) {
					message->FindBool("show_bezier_handles", &showBezierHandles);
					fSVGView->SetHighlightControlPoints(shapeIndex, pathIndex, showBezierHandles);
				}
				break;
			}
			case MSG_CLEAR_SELECTION:
				fSVGView->ClearHighlight();
				break;
			default:
				BWindow::MessageReceived(message);
				break;
		}
	}

private:
	void ShowError(const char* message)
	{
		BAlert* alert = new BAlert("Error", message, "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go();
	}

	void HandleRefsReceived(BMessage* message)
	{
		entry_ref ref;
		if (message->FindRef("refs", &ref) == B_OK) {
			BPath path(&ref);
			if (path.InitCheck() == B_OK) {
				LoadFile(path.Path());
			}
		}
	}

	void _UpdateMenuStates()
	{
		BMenuBar* menuBar = KeyMenuBar();
		if (!menuBar)
			return;

		BMenu* viewMenu = menuBar->SubmenuAt(1);
		if (viewMenu) {
			BMenu* displayMenu = viewMenu->FindItem("Display Mode")->Submenu();
			if (displayMenu) {
				svg_display_mode currentMode = fSVGView->DisplayMode();
				for (int32 i = 0; i < displayMenu->CountItems(); i++) {
					BMenuItem* item = displayMenu->ItemAt(i);
					if (item) {
						bool marked = false;
						switch (i) {
							case 0: marked = (currentMode == SVG_DISPLAY_NORMAL); break;
							case 1: marked = (currentMode == SVG_DISPLAY_OUTLINE); break;
							case 2: marked = (currentMode == SVG_DISPLAY_FILL_ONLY); break;
							case 3: marked = (currentMode == SVG_DISPLAY_STROKE_ONLY); break;
						}
						item->SetMarked(marked);
					}
				}
			}

			BMenuItem* transparencyItem = viewMenu->FindItem("Show Transparency Grid");
			if (transparencyItem) {
				transparencyItem->SetMarked(fSVGView->ShowTransparency());
			}
		}

		BMenu* bboxMenu = menuBar->SubmenuAt(2);
		if (bboxMenu) {
			svg_boundingbox_style currentStyle = fSVGView->BoundingBoxStyle();
			for (int32 i = 0; i < bboxMenu->CountItems(); i++) {
				BMenuItem* item = bboxMenu->ItemAt(i);
				if (item) {
					bool marked = false;
					switch (i) {
						case 0: marked = (currentStyle == SVG_BBOX_NONE); break;
						case 1: marked = (currentStyle == SVG_BBOX_DOCUMENT); break;
						case 2: marked = (currentStyle == SVG_BBOX_SIMPLE_FRAME); break;
						case 3: marked = (currentStyle == SVG_BBOX_TRANSPARENT_GRAY); break;
					}
					item->SetMarked(marked);
				}
			}
		}
	}

private:
	BSVGView* fSVGView;
	BFilePanel* fOpenPanel;
};

class SVGApp : public BApplication {
public:
	SVGApp() : BApplication("application/x-vnd.svg-viewer"), fFilePath(NULL) {}

	virtual void ReadyToRun()
	{
		SVGWindow* window = new SVGWindow(fFilePath);
		window->Show();
	}

	virtual void RefsReceived(BMessage* message)
	{
		BWindow* window = WindowAt(0);
		if (window) {
			window->PostMessage(message);
		}
	}

private:
	const char* fFilePath;
};

int main(int argc, char* argv[])
{
	SVGApp app;
	app.Run();
	return 0;
}
