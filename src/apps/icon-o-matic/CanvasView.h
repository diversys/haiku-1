/*
 * Copyright 2006, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus <superstippi@gmx.de>
 */

#ifndef CANVAS_VIEW_H
#define CANVAS_VIEW_H

#include "Icon.h"
#include "StateView.h"

class BBitmap;
class IconRenderer;

enum {
	SNAPPING_OFF	= 0,
	SNAPPING_64,
	SNAPPING_32,
	SNAPPING_16,
};

class CanvasView : public StateView,
				   public IconListener {
 public:
								CanvasView(BRect frame);
	virtual						~CanvasView();

	// StateView interface
	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				Draw(BRect updateRect);

	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
										   const BMessage* dragMessage);

	// IconListener interface
	virtual	void				AreaInvalidated(const BRect& area);

	// CanvasView
			void				SetIcon(Icon* icon);

	inline	float				ZoomLevel() const
									{ return fZoomLevel; }

			void				SetMouseFilterMode(uint32 mode);
			bool				MouseFilterMode() const
									{ return fMouseFilterMode; }

			void				ConvertFromCanvas(BPoint* point) const;
			void				ConvertToCanvas(BPoint* point) const;

			void				ConvertFromCanvas(BRect* rect) const;
			void				ConvertToCanvas(BRect* rect) const;

 protected:
	// StateView interface
	virtual	bool				_HandleKeyDown(uint32 key, uint32 modifiers);

	// CanvasView
			BRect				_CanvasRect() const;

			void				_AllocBackBitmap(float width,
												 float height);
			void				_FreeBackBitmap();
			void				_DrawInto(BView* view,
										  BRect updateRect);

			void				_FilterMouse(BPoint* where) const;

			void				_MakeBackground();

 private:
			BBitmap*			fBitmap;
			BBitmap*			fBackground;

			Icon*				fIcon;
			IconRenderer*		fRenderer;
			BRect				fDirtyIconArea;

			BPoint				fCanvasOrigin;
			float				fZoomLevel;

			uint32				fMouseFilterMode;

			BBitmap*			fOffsreenBitmap;
			BView*				fOffsreenView;
};

#endif // CANVAS_VIEW_H
