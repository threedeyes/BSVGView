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
		menuBar->AddItem(viewMenu);

		AddChild(menuBar);

		BRect svgRect = bounds;
		svgRect.top = menuBar->Bounds().bottom + 1;

		fSVGView = new BSVGView(svgRect, "svg_view");
		AddChild(fSVGView);

		if (filePath) {
			LoadFile(filePath);
		}
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
