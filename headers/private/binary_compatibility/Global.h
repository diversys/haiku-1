/*
 * Copyright 2008, Oliver Tappe, zooey@hirschkaefer.de.
 * Copyright 2008, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BINARY_COMPATIBILITY_GLOBAL_H_
#define _BINARY_COMPATIBILITY__GLOBAL_H_


// method codes
enum {
	// app kit

	// interface kit
	PERFORM_CODE_MIN_SIZE				= 1000,
	PERFORM_CODE_MAX_SIZE				= 1001,
	PERFORM_CODE_PREFERRED_SIZE			= 1002,
	PERFORM_CODE_LAYOUT_ALIGNMENT		= 1003,
	PERFORM_CODE_HAS_HEIGHT_FOR_WIDTH	= 1004,
	PERFORM_CODE_GET_HEIGHT_FOR_WIDTH	= 1005,
	PERFORM_CODE_SET_LAYOUT				= 1006,
	PERFORM_CODE_INVALIDATE_LAYOUT		= 1007,
	PERFORM_CODE_DO_LAYOUT				= 1008

	// support kit
};


#endif // _BINARY_COMPATIBILITY__GLOBAL_H_
