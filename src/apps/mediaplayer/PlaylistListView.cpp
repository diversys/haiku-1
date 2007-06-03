/*
 * Copyright 2007, Haiku. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus <superstippi@gmx.de>
 */
#include "PlaylistListView.h"

#include <new>
#include <stdio.h>

#include <Autolock.h>
#include <Message.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <Window.h>

#include "Controller.h"
#include "ControllerObserver.h"
#include "ListViews.h"
#include "PlaybackState.h"
#include "Playlist.h"
#include "PlaylistObserver.h"

using std::nothrow;


enum {
	DISPLAY_NAME	= 0,
	DISPLAY_PATH	= 1
};


static float
playback_mark_size(const font_height& fh)
{
	return ceilf(fh.ascent * 0.7);
}


static float
text_offset(const font_height& fh)
{
	return ceilf(fh.ascent * 0.8);
}


class PlaylistItem : public SimpleItem {
 public:
								PlaylistItem(const entry_ref& ref);
		virtual					~PlaylistItem();

				void			Draw(BView* owner, BRect frame,
									const font_height& fh,
									bool tintedLine, uint32 mode,
									bool active,
									uint32 playbackState);

 private:
				entry_ref		fRef;

};


PlaylistItem::PlaylistItem(const entry_ref& ref)
	: SimpleItem(ref.name),
	  fRef(ref)
{
}


PlaylistItem::~PlaylistItem()
{
}


void
PlaylistItem::Draw(BView* owner, BRect frame, const font_height& fh,
	bool tintedLine, uint32 mode, bool active, uint32 playbackState)
{
	rgb_color color = (rgb_color){ 255, 255, 255, 255 };
	if (tintedLine)
		color = tint_color(color, 1.04);
	// background
	if (IsSelected())
		color = tint_color(color, B_DARKEN_2_TINT);
	owner->SetLowColor(color);
	owner->FillRect(frame, B_SOLID_LOW);
	// label
	rgb_color black = (rgb_color){ 0, 0, 0, 255 };
	owner->SetHighColor(black);
	const char* text = Text();
	switch (mode) {
		case DISPLAY_NAME:
			// TODO
			break;
		case DISPLAY_PATH:
			// TODO
			break;
		default:
			break;
	}

	float playbackMarkSize = playback_mark_size(fh);
	float textOffset = text_offset(fh);

	BString truncatedString(text);
	owner->TruncateString(&truncatedString, B_TRUNCATE_MIDDLE,
		frame.Width() - playbackMarkSize - textOffset);
	owner->DrawString(truncatedString.String(),
		BPoint(frame.left + playbackMarkSize + textOffset,
			floorf(frame.top + frame.bottom + fh.ascent) / 2 - 1));

	// playmark
	if (active) {
		rgb_color green = (rgb_color){ 0, 255, 0, 255 };
		if (playbackState != PLAYBACK_STATE_PLAYING)
			green = tint_color(color, B_DARKEN_1_TINT);

		BRect r(0, 0, playbackMarkSize, playbackMarkSize);
		r.OffsetTo(frame.left + 4,
			ceilf((frame.top + frame.bottom) / 2) - 5);

		BPoint arrow[3];
		arrow[0] = r.LeftTop();
		arrow[1] = r.LeftBottom();
		arrow[2].x = r.right;
		arrow[2].y = (r.top + r.bottom) / 2;
#ifdef __HAIKU__
		owner->SetPenSize(2);
		owner->StrokePolygon(arrow, 3);
		owner->SetPenSize(1);
#else
		rgb_color lightGreen = tint_color(green, B_LIGHTEN_2_TINT);
		rgb_color darkGreen = tint_color(green, B_DARKEN_2_TINT);
 		owner->BeginLineArray( 6 );
			// black outline
			owner->AddLine(arrow[0], arrow[1], black);
			owner->AddLine(BPoint(arrow[1].x + 1.0, arrow[1].y - 1.0),
				arrow[2], black);
			owner->AddLine(arrow[0], arrow[2], black);
			// inset arrow
			arrow[0].x += 1.0;
			arrow[0].y += 2.0;
			arrow[1].x += 1.0;
			arrow[1].y -= 2.0;
			arrow[2].x -= 2.0;
			// highlights and shadow
			owner->AddLine(arrow[1], arrow[2], darkGreen);
			owner->AddLine(arrow[0], arrow[2], lightGreen);
			owner->AddLine(arrow[0], arrow[1], lightGreen);
		owner->EndLineArray();
		// fill green
		arrow[0].x += 1.0;
		arrow[0].y += 1.0;
		arrow[1].x += 1.0;
		arrow[1].y -= 1.0;
		arrow[2].x -= 2.0;
#endif // __HAIKU__
		owner->SetLowColor(owner->HighColor());
		owner->SetHighColor(green);
		owner->FillPolygon(arrow, 3);
	}
}


// #pragma mark -


PlaylistListView::PlaylistListView(BRect frame, Playlist* playlist,
		Controller* controller)
	: SimpleListView(frame, "playlist listview", NULL)

	, fPlaylist(playlist)
	, fPlaylistObserver(new PlaylistObserver(this))

	, fController(controller)
	, fControllerObserver(new ControllerObserver(this,
			OBSERVE_PLAYBACK_STATE_CHANGES))

	, fCurrentPlaylistIndex(-1)
	, fPlaybackState(PLAYBACK_STATE_STOPPED)

	, fLastClickedItem(NULL)
{
	fPlaylist->AddListener(fPlaylistObserver);
	fController->AddListener(fControllerObserver);

#ifdef __HAIKU__
	SetFlags(Flags() | B_SUBPIXEL_PRECISE);
#endif
}


PlaylistListView::~PlaylistListView()
{
	fPlaylist->RemoveListener(fPlaylistObserver);
	delete fPlaylistObserver;
	fController->RemoveListener(fControllerObserver);
	delete fControllerObserver;
}


void
PlaylistListView::AttachedToWindow()
{
	_FullSync();
	SimpleListView::AttachedToWindow();

	GetFontHeight(&fFontHeight);
	MakeFocus(true);
}


void
PlaylistListView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		// PlaylistObserver messages
		case MSG_PLAYLIST_REF_ADDED: {
			entry_ref ref;
			int32 index;
			if (message->FindRef("refs", &ref) == B_OK
				&& message->FindInt32("index", &index) == B_OK)
				_AddItem(ref, index);
			break;
		}
		case MSG_PLAYLIST_REF_REMOVED: {
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				_RemoveItem(index);
			break;
		}
		case MSG_PLAYLIST_REFS_SORTED:
			_FullSync();
			break;
		case MSG_PLAYLIST_CURRENT_REF_CHANGED: {
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				_SetCurrentPlaylistIndex(index);
			break;
		}

		// ControllerObserver messages
		case MSG_CONTROLLER_PLAYBACK_STATE_CHANGED: {
			uint32 state;
			if (message->FindInt32("state", (int32*)&state) == B_OK)
				_SetPlaybackState(state);
			break;
		}

		case B_SIMPLE_DATA:
			if (message->HasRef("refs"))
				RefsReceived(message, fDropIndex);
			else if (message->HasPointer("list"))
				SimpleListView::MessageReceived(message);
			break;
		case B_REFS_RECEIVED:
			RefsReceived(message, fDropIndex);
			break;

		default:
			SimpleListView::MessageReceived(message);
			break;
	}
}


void
PlaylistListView::MouseDown(BPoint where)
{
	if (!IsFocus())
		MakeFocus(true);

	int32 clicks;
	if (Window()->CurrentMessage()->FindInt32("clicks", &clicks) < B_OK)
		clicks = 1;

	bool handled = false;

	float playbackMarkSize = playback_mark_size(fFontHeight);
	float textOffset = text_offset(fFontHeight);

	for (int32 i = 0;
		PlaylistItem* item = dynamic_cast<PlaylistItem*>(ItemAt(i)); i++) {
		BRect r = ItemFrame(i);
		if (r.Contains(where)) {
			if (clicks == 2) {
				// only do something if user clicked the same item twice
				if (fLastClickedItem == item) {
					BAutolock _(fPlaylist);
					fPlaylist->SetCurrentRefIndex(i);
					handled = true;
				}
			} else {
				// remember last clicked item
				fLastClickedItem = item;
				if (i == fCurrentPlaylistIndex) {
					r.right = r.left + playbackMarkSize + textOffset;
					if (r.Contains (where)) {
						fController->TogglePlaying();
						handled = true;
					}
				}
			}
			break;
		}
	}

	if (!handled)
		SimpleListView::MouseDown(where);
}


void
PlaylistListView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes < 1)
		return;
		
	if ((bytes[0] == B_BACKSPACE) || (bytes[0] == B_DELETE))
		RemoveSelected();

	DragSortableListView::KeyDown(bytes, numBytes);
}


void
PlaylistListView::MoveItems(BList& indices, int32 toIndex)
{
	if (!fPlaylist->Lock())
		return;

	entry_ref currentRef;
	bool adjustCurrentRef = fPlaylist->GetRefAt(fPlaylist->CurrentRefIndex(),
		&currentRef) == B_OK;

	int32 count = indices.CountItems();
	entry_ref refs[count];
	for (int32 i = 0; i < count; i++) {
		int32 index = (int32)indices.ItemAtFast(i) - i;
			// "-i" to account for items already removed in the
			// target list
		if (index < 0) {
			// asynchronous message is out of date
			return;
		}
		refs[i] = fPlaylist->RemoveRef(index, false);
		if (index < toIndex)
			toIndex --;
	}

	for (int32 i = 0; i < count; i++) {
		fPlaylist->AddRef(refs[i], toIndex++);
	}

	if (adjustCurrentRef)
		fPlaylist->SetCurrentRefIndex(fPlaylist->IndexOf(currentRef));

	fPlaylist->Unlock();
}


void
PlaylistListView::CopyItems(BList& indices, int32 toIndex)
{
	if (!fPlaylist->Lock())
		return;

	int32 count = indices.CountItems();
	entry_ref refs[count];
	for (int32 i = 0; i < count; i++) {
		int32 index = (int32)indices.ItemAtFast(i);
		if (index < 0) {
			// asynchronous message is out of date
			return;
		}
		if (fPlaylist->GetRefAt(index, &refs[i]) < B_OK)
			return;
	}

	for (int32 i = 0; i < count; i++) {
		fPlaylist->AddRef(refs[i], toIndex++);
	}

	fPlaylist->Unlock();
}


void
PlaylistListView::RemoveItemList(BList& indices)
{
	if (!fPlaylist->Lock())
		return;

	int32 count = indices.CountItems();
	int32 lastRemovedIndex = -1;
	for (int32 i = 0; i < count; i++) {
		lastRemovedIndex = (int32)indices.ItemAtFast(i) - i;
			// "-i" to account for items already removed in the
			// target list
		fPlaylist->RemoveRef(lastRemovedIndex);
	}

	// in case we removed the currently playing file
	if (fPlaylist->CurrentRefIndex() == -1)
		fPlaylist->SetCurrentRefIndex(lastRemovedIndex);

	fPlaylist->Unlock();
}


void
PlaylistListView::DrawListItem(BView* owner, int32 index, BRect frame) const
{
	if (PlaylistItem* item = dynamic_cast<PlaylistItem*>(ItemAt(index))) {
		item->Draw(owner, frame, fFontHeight, index % 2,
			DISPLAY_NAME, index == fCurrentPlaylistIndex, fPlaybackState);
	}
}


void
PlaylistListView::RefsReceived(BMessage* message, int32 appendIndex)
{
	if (!fPlaylist->Lock())
		return;

	fPlaylist->AppendRefs(message, appendIndex);

	fPlaylist->Unlock();
}


// #pragma mark -


void
PlaylistListView::_FullSync()
{
	if (!fPlaylist->Lock())
		return;

	// TODO: detach scrollbar, and this will be quick...
//	BScrollBar* scrollBar = ScrollBar(B_VERTICAL);
//	SetScrollBar();
//
	MakeEmpty();

	int32 count = fPlaylist->CountItems();
	for (int32 i = 0; i < count; i++) {
		entry_ref ref;
		if (fPlaylist->GetRefAt(i, &ref) == B_OK)
			_AddItem(ref, i);
	}

	_SetCurrentPlaylistIndex(fPlaylist->CurrentRefIndex());
	_SetPlaybackState(fController->PlaybackState());

	fPlaylist->Unlock();
}


void
PlaylistListView::_AddItem(const entry_ref& ref, int32 index)
{
	PlaylistItem* item = new (nothrow) PlaylistItem(ref);
	if (item)
		AddItem(item, index);
}


void
PlaylistListView::_RemoveItem(int32 index)
{
	delete RemoveItem(index);
}


void
PlaylistListView::_SetCurrentPlaylistIndex(int32 index)
{
	if (fCurrentPlaylistIndex == index)
		return;

	InvalidateItem(fCurrentPlaylistIndex);
	fCurrentPlaylistIndex = index;
	InvalidateItem(fCurrentPlaylistIndex);
}


void
PlaylistListView::_SetPlaybackState(uint32 state)
{
	if (fPlaybackState == state)
		return;

	fPlaybackState = state;
	InvalidateItem(fCurrentPlaylistIndex);
}


