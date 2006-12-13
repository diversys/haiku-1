/*
 * Copyright 2005-2006, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Stephan Aßmus <superstippi@gmx.de>
 */


#include "WorkspacesLayer.h"

#include "AppServer.h"
#include "Desktop.h"
#include "DrawingEngine.h"
#include "WindowLayer.h"
#include "Workspace.h"

#include <WindowPrivate.h>


WorkspacesLayer::WorkspacesLayer(BRect frame, BPoint scrollingOffset,
		const char* name, int32 token, uint32 resizeMode, uint32 flags)
	: ViewLayer(frame, scrollingOffset, name, token, resizeMode, flags),
	fSelectedWindow(NULL),
	fSelectedWorkspace(-1),
	fHasMoved(false)
{
	fDrawState->SetLowColor(RGBColor(255, 255, 255));
	fDrawState->SetHighColor(RGBColor(0, 0, 0));
}


WorkspacesLayer::~WorkspacesLayer()
{
}


void
WorkspacesLayer::_GetGrid(int32& columns, int32& rows)
{
	DesktopSettings settings(Window()->Desktop());

	int32 count = settings.WorkspacesCount();
	int32 squareRoot = (int32)sqrt(count);

	rows = 1;
	for (int32 i = 2; i <= squareRoot; i++) {
		if (count % i == 0)
			rows = i;
	}

	columns = count / rows;
}


/*!
	\brief Returns the frame of the screen for the specified workspace.
*/
BRect
WorkspacesLayer::_ScreenFrame(int32 i)
{
	// TODO: we don't need the current screen frame, but the one
	//	from the workspace!
	uint16 width, height;
	uint32 colorSpace;
	float frequency;
	Window()->Desktop()->ScreenAt(0)->GetMode(width, height,
		colorSpace, frequency);

	return BRect(0, 0, width - 1, height - 1);
}


/*!
	\brief Returns the frame of the specified workspace within the
		workspaces layer.
*/
BRect
WorkspacesLayer::_WorkspaceAt(int32 i)
{
	int32 columns, rows;
	_GetGrid(columns, rows);

	int32 width = Frame().IntegerWidth() / columns;
	int32 height = Frame().IntegerHeight() / rows;

	int32 column = i % columns;
	int32 row = i / columns;

	BRect rect(column * width, row * height, (column + 1) * width, (row + 1) * height);

	rect.OffsetBy(Frame().LeftTop());

	// make sure there is no gap anywhere
	if (column == columns - 1)
		rect.right = Frame().right;
	if (row == rows - 1)
		rect.bottom = Frame().bottom;

	return rect;
}


/*!
	\brief Returns the workspace frame and index of the workspace
		under \a where.

	If, for some reason, there is no workspace located under \where,
	an empty rectangle is returned, and \a index is set to -1.
*/
BRect
WorkspacesLayer::_WorkspaceAt(BPoint where, int32& index)
{
	int32 columns, rows;
	_GetGrid(columns, rows);

	for (index = columns * rows; index-- > 0;) {
		BRect workspaceFrame = _WorkspaceAt(index);

		if (workspaceFrame.Contains(where))
			return workspaceFrame;
	}

	return BRect();
}


BRect
WorkspacesLayer::_WindowFrame(const BRect& workspaceFrame,
	const BRect& screenFrame, const BRect& windowFrame,
	BPoint windowPosition)
{
	BRect frame = windowFrame;
	frame.OffsetTo(windowPosition);

	float factor = workspaceFrame.Width() / screenFrame.Width();
	frame.left = rintf(frame.left * factor);
	frame.right = rintf(frame.right * factor);

	factor = workspaceFrame.Height() / screenFrame.Height();
	frame.top = rintf(frame.top * factor);
	frame.bottom = rintf(frame.bottom * factor);

	frame.OffsetBy(workspaceFrame.LeftTop());
	return frame;
}


void
WorkspacesLayer::_DrawWindow(DrawingEngine* drawingEngine, const BRect& workspaceFrame,
	const BRect& screenFrame, WindowLayer* window, BPoint windowPosition,
	BRegion& backgroundRegion, bool active)
{
	if (window->Feel() == kDesktopWindowFeel || window->IsHidden())
		return;

	BPoint offset = window->Frame().LeftTop() - windowPosition;
	BRect frame = _WindowFrame(workspaceFrame, screenFrame, window->Frame(),
		windowPosition);
	Decorator *decorator = window->Decorator();
	BRect tabFrame(0, 0, 0, 0);
	if (decorator != NULL)
		tabFrame = decorator->TabRect();

	tabFrame = _WindowFrame(workspaceFrame, screenFrame,
		tabFrame, tabFrame.LeftTop() - offset);
	if (!workspaceFrame.Intersects(frame) && !workspaceFrame.Intersects(tabFrame))
		return;

	// ToDo: let decorator do this!
	RGBColor yellow;
	if (decorator != NULL)
		yellow = decorator->UIColor(B_WINDOW_TAB_COLOR);
	RGBColor frameColor(180, 180, 180);
	RGBColor white(255, 255, 255);

	if (!active) {
		_DarkenColor(yellow);
		_DarkenColor(frameColor);
		_DarkenColor(white);
	}
	if (window == fSelectedWindow) {
		// TODO: what about standard navigation color here?
		frameColor.SetColor(80, 80, 80);
	}

	if (tabFrame.left < frame.left)
		tabFrame.left = frame.left;
	if (tabFrame.right >= frame.right)
		tabFrame.right = frame.right - 1;

	tabFrame.top = frame.top - 1;
	tabFrame.bottom = frame.top - 1;
	tabFrame = tabFrame & workspaceFrame;

	if (decorator != NULL && tabFrame.IsValid()) {
		drawingEngine->StrokeLine(tabFrame.LeftTop(), tabFrame.RightBottom(), yellow);
		backgroundRegion.Exclude(tabFrame);
	}

	drawingEngine->StrokeRect(frame, frameColor);

	frame = frame & workspaceFrame;
	if (frame.IsValid()) {
		backgroundRegion.Exclude(frame);

		frame.InsetBy(1, 1);
		drawingEngine->FillRect(frame, white);
	}

	// draw title

	// TODO: disabled because it's much too slow this way - the mini-window
	//	functionality should probably be moved into the WindowLayer class,
	//	so that it has only to be recalculated on demand. With double buffered
	//	windows, this would also open up the door to have a more detailed
	//	preview.
#if 0
	BString title(window->Title());

	ServerFont font = fDrawState->Font();
	font.SetSize(7);
	fDrawState->SetFont(font);

	fDrawState->Font().TruncateString(&title, B_TRUNCATE_END, frame.Width() - 4);
	float width = drawingEngine->StringWidth(title.String(), title.Length(),
		fDrawState, NULL);
	float height = drawingEngine->StringHeight(title.String(), title.Length(),
		fDrawState);

	drawingEngine->DrawString(title.String(), title.Length(),
		BPoint(frame.left + (frame.Width() - width) / 2,
			frame.top + (frame.Height() + height) / 2),
		fDrawState, NULL);
#endif
}


void
WorkspacesLayer::_DrawWorkspace(DrawingEngine* drawingEngine,
	BRegion& redraw, int32 index)
{
	BRect rect = _WorkspaceAt(index);

	Workspace workspace(*Window()->Desktop(), index);
	bool active = workspace.IsCurrent();
	if (active) {
		// draw active frame
		RGBColor black(0, 0, 0);
		drawingEngine->StrokeRect(rect, black);
	} else if (index == fSelectedWorkspace) {
		RGBColor gray(80, 80, 80);
		drawingEngine->StrokeRect(rect, gray);
	}

	rect.InsetBy(1, 1);

	RGBColor color = workspace.Color();
	if (!active)
		_DarkenColor(color);

	// draw windows

	BRegion backgroundRegion = redraw;

	// ToDo: would be nice to get the real update region here

	BRect screenFrame = _ScreenFrame(index);

	BRegion workspaceRegion(rect);
	backgroundRegion.IntersectWith(&workspaceRegion);
	drawingEngine->ConstrainClippingRegion(&backgroundRegion);

	WindowLayer* window;
	BPoint leftTop;
	while (workspace.GetNextWindow(window, leftTop) == B_OK) {
		_DrawWindow(drawingEngine, rect, screenFrame, window,
			leftTop, backgroundRegion, active);
	}

	// draw background

	drawingEngine->ConstrainClippingRegion(&backgroundRegion);
	drawingEngine->FillRect(rect, color);

	drawingEngine->ConstrainClippingRegion(&redraw);
}


void
WorkspacesLayer::_DarkenColor(RGBColor& color) const
{
	color = tint_color(color.GetColor32(), B_DARKEN_2_TINT);
}


void
WorkspacesLayer::_Invalidate() const
{
	BRegion region(Frame());
	Window()->MarkContentDirty(region);
}


void
WorkspacesLayer::Draw(DrawingEngine* drawingEngine, BRegion* effectiveClipping,
	BRegion* windowContentClipping, bool deep)
{
	// we can only draw within our own area
	BRegion redraw(ScreenClipping(windowContentClipping));
	// add the current clipping
	redraw.IntersectWith(effectiveClipping);

	int32 columns, rows;
	_GetGrid(columns, rows);

	// draw grid

	// make sure the grid around the active workspace is not drawn
	// to reduce flicker
	BRect activeRect = _WorkspaceAt(Window()->Desktop()->CurrentWorkspace());
	BRegion gridRegion(redraw);
	gridRegion.Exclude(activeRect);
	drawingEngine->ConstrainClippingRegion(&gridRegion);

	BRect frame = Frame();
		// top ViewLayer frame is in screen coordinates

	// horizontal lines

	drawingEngine->StrokeLine(BPoint(frame.left, frame.top),
		BPoint(frame.right, frame.top), ViewColor());

	for (int32 row = 0; row < rows; row++) {
		BRect rect = _WorkspaceAt(row * columns);
		drawingEngine->StrokeLine(BPoint(frame.left, rect.bottom),
			BPoint(frame.right, rect.bottom), ViewColor());
	}

	// vertical lines

	drawingEngine->StrokeLine(BPoint(frame.left, frame.top),
		BPoint(frame.left, frame.bottom), ViewColor());

	for (int32 column = 0; column < columns; column++) {
		BRect rect = _WorkspaceAt(column);
		drawingEngine->StrokeLine(BPoint(rect.right, frame.top),
			BPoint(rect.right, frame.bottom), ViewColor());
	}

	drawingEngine->ConstrainClippingRegion(&redraw);

	// draw workspaces

	for (int32 i = rows * columns; i-- > 0;) {
		_DrawWorkspace(drawingEngine, redraw, i);
	}
}


void
WorkspacesLayer::MouseDown(BMessage* message, BPoint where)
{
	// reset tracking variables
	fSelectedWorkspace = -1;
	fSelectedWindow = NULL;
	fHasMoved = false;

	// check if the correct mouse button is pressed
	int32 buttons;
	if (message->FindInt32("buttons", &buttons) != B_OK
		|| (buttons & B_PRIMARY_MOUSE_BUTTON) == 0)
		return;

	int32 index;
	BRect workspaceFrame = _WorkspaceAt(where, index);
	if (index < 0)
		return;

	Workspace workspace(*Window()->Desktop(), index);
	workspaceFrame.InsetBy(1, 1);

	BRect screenFrame = _ScreenFrame(index);

	WindowLayer* window;
	BRect windowFrame;
	BPoint leftTop;
	while (workspace.GetNextWindow(window, leftTop) == B_OK) {
		BRect frame = _WindowFrame(workspaceFrame, screenFrame, window->Frame(),
			leftTop);
		if (frame.Contains(where) && window->Feel() != kDesktopWindowFeel
			&& window->Feel() != kWindowScreenFeel) {
			// We can't exit the loop here, as we traverse the window
			// list in the wrong direction...
			fSelectedWindow = window;
			windowFrame = frame;
		}
	}

	// Some special functionality (clicked with modifiers)

	int32 modifiers;
	if (fSelectedWindow != NULL
		&& message->FindInt32("modifiers", &modifiers) == B_OK) {
		if ((modifiers & B_CONTROL_KEY) != 0) {
			// Activate window if clicked with the control key pressed,
			// minimize it if control+shift - this mirrors Deskbar
			// shortcuts (when pressing a team menu item).
			if ((modifiers & B_SHIFT_KEY) != 0)
				fSelectedWindow->ServerWindow()->NotifyMinimize(true);
			else
				Window()->Desktop()->ActivateWindow(fSelectedWindow);
			fSelectedWindow = NULL;
		} else if ((modifiers & B_OPTION_KEY) != 0) {
			// Also, send window to back if clicked with the option
			// key pressed.
			Window()->Desktop()->SendWindowBehind(fSelectedWindow);
			fSelectedWindow = NULL;
		}
	}

	// If this window is movable, we keep it selected

	if (fSelectedWindow != NULL && (fSelectedWindow->Flags() & B_NOT_MOVABLE) != 0
		|| fSelectedWindow == Window()) {
		fSelectedWindow = NULL;
		index = -1;
	}

	fLeftTopOffset = where - windowFrame.LeftTop();
	fSelectedWorkspace = index;

	if (index >= 0)
		_Invalidate();
}


void
WorkspacesLayer::MouseUp(BMessage* message, BPoint where)
{
	if (!fHasMoved && fSelectedWorkspace >= 0) {
		int32 index;
		_WorkspaceAt(where, index);
		if (index >= 0)
			Window()->Desktop()->SetWorkspace(index);
	}

	if (fSelectedWindow != NULL) {
		// We need to hide the selection frame again
		_Invalidate();
	}

	fSelectedWindow = NULL;
	fSelectedWorkspace = -1;
}


void
WorkspacesLayer::MouseMoved(BMessage* message, BPoint where)
{
	if (fSelectedWindow == NULL && fSelectedWorkspace < 0)
		return;

	// check if the correct mouse button is pressed
	int32 buttons;
	if (message->FindInt32("buttons", &buttons) != B_OK
		|| (buttons & B_PRIMARY_MOUSE_BUTTON) == 0)
		return;

	if (!fHasMoved) {
		Window()->Desktop()->SetMouseEventWindow(Window());
			// don't let us off the mouse
	}

	int32 index;
	BRect workspaceFrame = _WorkspaceAt(where, index);

	if (fSelectedWindow == NULL) {
		if (fSelectedWorkspace >= 0 && fSelectedWorkspace != index) {
			fSelectedWorkspace = index;
			_Invalidate();
		}
		return;
	}

	workspaceFrame.InsetBy(1, 1);

	if (index != fSelectedWorkspace) {
		if (!fSelectedWindow->InWorkspace(index) && fSelectedWindow->IsNormal()) {
			// move window to this new workspace
			uint32 newWorkspaces = fSelectedWindow->Workspaces()
				& ~(1UL << fSelectedWorkspace) | (1UL << index);

			Window()->Desktop()->SetWindowWorkspaces(fSelectedWindow,
				newWorkspaces);
		}
		fSelectedWorkspace = index;
	}

	BRect screenFrame = _ScreenFrame(index);
	float left = rintf((where.x - workspaceFrame.left - fLeftTopOffset.x)
		* screenFrame.Width() / workspaceFrame.Width());
	float top = rintf((where.y - workspaceFrame.top - fLeftTopOffset.y)
		* screenFrame.Height() / workspaceFrame.Height());

	BPoint leftTop;
	if (fSelectedWorkspace == Window()->Desktop()->CurrentWorkspace())
		leftTop = fSelectedWindow->Frame().LeftTop();
	else {
		if (fSelectedWindow->Anchor(fSelectedWorkspace).position == kInvalidWindowPosition)
			fSelectedWindow->Anchor(fSelectedWorkspace).position = fSelectedWindow->Frame().LeftTop();
		leftTop = fSelectedWindow->Anchor(fSelectedWorkspace).position;
	}

	Window()->Desktop()->MoveWindowBy(fSelectedWindow, left - leftTop.x, top - leftTop.y,
		fSelectedWorkspace);

	fHasMoved = true;
}


void
WorkspacesLayer::WindowChanged(WindowLayer* window)
{
	// TODO: be smarter about this!
	_Invalidate();
}


void
WorkspacesLayer::WindowRemoved(WindowLayer* window)
{
	if (fSelectedWindow == window)
		fSelectedWindow = NULL;
}

