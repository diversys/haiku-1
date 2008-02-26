/*
 * Copyright 2007-2008, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ithamar Adema, ithamar AT unet DOT nl
 */
#ifndef HDA_CODEC_H
#define HDA_CODEC_H

typedef enum {
	WT_AUDIO_OUTPUT		= 0,
	WT_AUDIO_INPUT		= 1,
	WT_AUDIO_MIXER		= 2,
	WT_AUDIO_SELECTOR	= 3,
	WT_PIN_COMPLEX		= 4,
	WT_POWER			= 5,
	WT_VOLUME_KNOB		= 6,
	WT_BEEP_GENERATOR	= 7,
	WT_VENDOR_DEFINED	= 15
} hda_widget_type;


typedef enum {
	PIN_DEV_LINE_OUT = 0,
	PIN_DEV_SPEAKER,
	PIN_DEV_HP_OUT,
	PIN_DEV_CD,
	PIN_DEV_SPDIF_OUT,
	PIN_DEV_DIGITAL_OTHER_OUT,
	PIN_DEV_MODEM_LINE_SIDE,
	PIN_DEV_MODEM_HAND_SIDE,
	PIN_DEV_LINE_IN,
	PIN_DEV_AUX,
	PIN_DEV_MIC_IN,
	PIN_DEV_TELEPHONY,
	PIN_DEV_SPDIF_IN,
	PIN_DEV_DIGITAL_OTHER_IN,
	PIN_DEV_RESERVED,
	PIN_DEV_OTHER
} pin_dev_type;


/* Verb Helper Macro */
#define MAKE_VERB(cad, nid, vid, payload) \
	(((cad) << 28) | ((nid) << 20) | (vid) | (payload))

/* Verb IDs */
#define VID_GET_PARAM			0xF0000
#define VID_GET_CONNSEL			0xF0100
#define VID_SET_CONNSEL			0x70100
#define VID_GET_CONNLENTRY		0xF0200
#define VID_GET_PROCSTATE		0xF0300
#define VID_SET_PROCSTATE		0x70300
#define VID_GET_COEFFIDX		0xD0000
#define VID_SET_COEFFIDX		0x50000
#define VID_GET_PROCCOEFF		0xC0000
#define VID_SET_PROCCOEFF		0x40000
#define VID_GET_AMPGAINMUTE		0xB0000
#define VID_SET_AMPGAINMUTE		0x30000
#define VID_GET_CONVERTER_FORMAT			0xa0000
#define VID_SET_CONVERTER_FORMAT			0x20000
#define VID_GET_CONVERTER_STREAM_CHANNEL	0xf0600
#define VID_SET_CONVERTER_STREAM_CHANNEL	0x70600
#define VID_GET_DIGCVTCTRL		0xF0D00
#define VID_SET_DIGCVTCTRL1		0x70D00
#define VID_SET_DIGCVTCTRL2		0x70E00
#define VID_GET_POWERSTATE		0xF0500
#define VID_SET_POWERSTATE		0x70500
#define VID_GET_SDISELECT		0xF0400
#define VID_SET_SDISELECT		0x70400
#define VID_GET_PINWCTRL		0xF0700
#define VID_SET_PINWCTRL		0x70700
#define VID_GET_UNSOLRESP		0xF0800
#define VID_SET_UNSOLRESP		0x70800
#define VID_GET_PINSENSE		0xF0900
#define VID_SET_PINSENSE		0x70900
#define VID_GET_EAPDBTL_EN		0xF0C00
#define VID_SET_EAPDBTL_EN		0x70C00
#define VID_GET_GPIDATA			0xF1000
#define VID_SET_GPIDATA			0x71000
#define VID_GET_GPIWAKE_EN		0xF1100
#define VID_SET_GPIWAKE_EN		0x71100
#define VID_GET_GPIUNSOL		0xF1200
#define VID_SET_GPIUNSOL		0x71200
#define VID_GET_GPISTICKY		0xF1300
#define VID_SET_GPISTICKY		0x71300
#define VID_GET_GPODATA			0xF1400
#define VID_SET_GPODATA			0x71400
#define VID_GET_GPIODATA		0xF1500
#define VID_SET_GPIODATA		0x71500
#define VID_GET_GPIO_EN			0xF1600
#define VID_SET_GPIO_EN			0x71600
#define VID_GET_GPIO_DIR		0xF1700
#define VID_SET_GPIO_DIR		0x71700
#define VID_GET_GPIOWAKE_EN		0xF1800
#define VID_SET_GPIOWAKE_EN		0x71800
#define VID_GET_GPIOUNSOL_EN	0xF1900
#define VID_SET_GPIOUNSOL_EN	0x71900
#define VID_GET_GPIOSTICKY		0xF1A00
#define VID_SET_GPIOSTICKY		0x71A00
#define VID_GET_BEEPGEN			0xF0A00
#define VID_SET_BEEPGEN			0x70A00
#define VID_GET_VOLUMEKNOB		0xF0F00
#define VID_SET_VOLUMEKNOB		0x70F00
#define VID_GET_SUBSYSTEMID		0xF2000
#define VID_SET_SUBSYSTEMID1	0x72000
#define VID_SET_SUBSYSTEMID2	0x72100
#define VID_SET_SUBSYSTEMID3	0x72200
#define VID_SET_SUBSYSTEMID4	0x72300
#define VID_GET_CFGDEFAULT		0xF1C00
#define VID_SET_CFGDEFAULT1		0x71C00
#define VID_SET_CFGDEFAULT2		0x71D00
#define VID_SET_CFGDEFAULT3		0x71E00
#define VID_SET_CFGDEFAULT4		0x71F00
#define VID_GET_STRIPECTRL		0xF2400
#define VID_SET_STRIPECTRL		0x72000
#define VID_FUNCTION_RESET		0x7FF00

/* Parameter IDs */
#define PID_VENDORID			0x00
#define PID_REVISIONID			0x02
#define PID_SUBORD_NODE_COUNT	0x04
#define PID_FUNCGRP_TYPE		0x05
#define PID_AUDIO_FG_CAP		0x08
#define PID_AUDIO_WIDGET_CAP	0x09
#define PID_PCM_SUPPORT			0x0A
#define PID_STREAM_SUPPORT		0x0B
#define PID_PIN_CAP				0x0C
#define PID_INPUT_AMP_CAP		0x0D
#define PID_CONNLIST_LEN		0x0E
#define PID_POWERSTATE_SUPPORT	0x0F
#define PID_PROCESSING_CAP		0x10
#define PID_GPIO_COUNT			0x11
#define PID_OUTPUT_AMP_CAP		0x12
#define PID_VOLUMEKNOB_CAP		0x13

/* PCM support */
#define PCM_8_BIT				(1L << 16)
#define PCM_16_BIT				(1L << 17)
#define PCM_20_BIT				(1L << 18)
#define PCM_24_BIT				(1L << 19)
#define PCM_32_BIT				(1L << 20)

/* stream support */
#define STREAM_AC3				0x00000004
#define STREAM_FLOAT			0x00000002
#define STREAM_PCM				0x00000001

#endif /* HDA_CODEC_H */
