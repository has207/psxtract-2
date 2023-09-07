// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <cstdint>
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>

#include "cdrom.h"
#include "lz.h"
#include "crypto.h"

#define GAP_SIZE	2 * 75 * SECTOR_SIZE  // 2 seconds * 75 frames * 2352 sector size
#define ISO_BLOCK_SIZE	16 * SECTOR_SIZE
#define ISO_HEADER_OFFSET 0x400
#define ISO_HEADER_SIZE	0xB6600
#define ISO_BASE_OFFSET	0x100000
#define CUE_LEADOUT_OFFSET	0x414
#define MAX_DISCS	5
#define NBYTES		0x180

// Multidisc ISO image signature.
char multi_iso_magic[0x10] = {
	0x50,  // P
	0x53,  // S
	0x54,  // T
	0x49,  // I
	0x54,  // T
	0x4C,  // L
	0x45,  // E
	0x49,  // I
	0x4D,  // M
	0x47,  // G
	0x30,  // 0
	0x30,  // 0
	0x30,  // 0
	0x30,  // 0
	0x30,  // 0
	0x30   // 0
};

// ISO image signature.
char iso_magic[0xC] = {
	0x50,  // P
	0x53,  // S
	0x49,  // I
	0x53,  // S
	0x4F,  // O
	0x49,  // I
	0x4D,  // M
	0x47,  // G
	0x30,  // 0
	0x30,  // 0
	0x30,  // 0
	0x30   // 0
};

// CUE structure
typedef struct {
	unsigned short    type;		    // Track Type = 41h for DATA, 01h for CDDA, A2h for lead out
	unsigned char    number;		// Track Number (01h to 99h)
	unsigned char    I0m;		    // INDEX 00 MM
	unsigned char    I0s;		    // INDEX 00 SS
	unsigned char    I0f;		    // INDEX 00 FF
	unsigned char	 padding;       // NULL
	unsigned char    I1m;		    // INDEX 01 MM
	unsigned char    I1s;		    // INDEX 01 SS
	unsigned char    I1f;		    // INDEX 01 FF
} CUE_ENTRY;

// CDDA table entry structure.
typedef struct {
	unsigned int     offset;
	unsigned int     size;
	unsigned char    padding[0x4];
	unsigned int	 checksum;
} CDDA_ENTRY;

// ISO table entry structure.
typedef struct {
	unsigned int     offset;
	unsigned short   size;
	unsigned short   marker;			// 0x01 or 0x00
	unsigned char	 checksum[0x10];	// first 0x10 bytes of sha1 sum of 0x10 disc sectors
	unsigned char	 padding[0x8];
} ISO_ENTRY;

// STARTDAT header structure.
typedef struct {
	unsigned char    magic[8];		// STARTDAT
	unsigned int	 unk1;			// 0x01
	unsigned int	 unk2;			// 0x01
	unsigned int	 header_size;
	unsigned int	 data_size;
} STARTDAT_HEADER;

// ATRAC3 RIFF WAVE header structure.
typedef struct {
	unsigned char	riff_id[4];		// "RIFF"
	uint32_t		riff_size;		// size of rest of the file: audio + header - 8
	unsigned char	riff_format[4]; // "WAVE"
	unsigned char	fmt_id[4];		// "fmt\x20"
	uint32_t		fmt_size;		// size of format, always 32
	uint16_t		codec_id;		// always 624 (ATRAC3+?)
	uint16_t		channels;		// number of channels, always 2
	uint32_t		sample_rate;	// sample rate - always 44100
	uint32_t		unknown1;		// always 16538, seems connected to next value
	uint32_t		bytes_per_frame; // always 384
	uint16_t		param_size;		// likely the length of remaining params, always 14
	uint16_t		param1;			// unknown, always 1
	uint16_t		param2;			// unknown, always 4096
	uint16_t		param3;			// unknown, always 0
	uint16_t		param4;			// unknown, always 0
	uint16_t		param5;			// unknown, always 0
	uint16_t		param6;			// unknown, always 1
	uint16_t		param7;			// unknown, always 0
	unsigned char	fact_id[4];		// "fact"
	uint32_t		fact_size;		// always 8
	uint32_t		fact_param1;	// size of (in|out)put file sans 44byte WAVE header, divided by 4
									// we can get this by taking the track time in mm:ss:ff
									// convert to frames (mm*60*75 + ss*75 + ff)
									// and multiply by sector size 2352
	uint32_t		fact_param2;	// unknown, always 1024
	unsigned char	data_id[4];		// "data"
	uint32_t		data_size;		// size of data segment
} AT3_HEADER;
