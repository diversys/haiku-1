/*
 * Copyright 2005, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef ICO_H
#define ICO_H


#include <GraphicsDefs.h>
#include <BufferIO.h>
#include <TranslatorFormats.h>


namespace ICO {

// All ICO structures are written in little endian format

struct ico_header {
	uint16	reserved;
	uint16	type;
	uint16	entry_count;

	bool IsValid() const { return reserved == 0 && type == 1 && entry_count < 32; }
	void SwapToHost();
	void SwapFromHost();
} _PACKED;

struct ico_dir_entry {
	uint8	width;
	uint8	height;
	uint8	color_count;
	uint8	reserved;

	uint16	planes;
	uint16	bits_per_pixel;
	uint32	size;
	uint32	offset;

	bool IsValid() const { return bits_per_pixel <= 24; }
	void SwapToHost();
	void SwapFromHost();
} _PACKED;

struct ico_bitmap_header {
	uint32	size;				// size of this structure
	uint32	width;
	uint32	height;
	uint16	planes;
	uint16	bits_per_pixel;		// 1, 4, 8, 16, or 24 bits per pixel
	uint32	compression;
	uint32	image_size;
	uint32	x_pixels_per_meter;	// aspect ratio
	uint32	y_pixels_per_meter;
	uint32	colors_used;		// number of actually used colors
	uint32	important_colors;	// number of important colors (zero = all)

	bool IsValid() const;
	void SwapToHost();
	void SwapFromHost();
} _PACKED;

struct ico_color {
	uint8	blue;
	uint8	green;
	uint8	red;
	uint8	alpha;

	inline bool
	operator==(const ico_color& other) const
	{
		return red == other.red && green == other.green && blue == other.blue;
	}
};


extern status_t identify(BPositionIO &stream, int32 &bitsPerPixel);
extern status_t convert_ico_to_bits(BPositionIO &source, BPositionIO &target);
extern status_t convert_bits_to_ico(BPositionIO &source, TranslatorBitmap &bitsHeader, BPositionIO &target);

}	// namespace ICO

#endif	/* ICO_H */
