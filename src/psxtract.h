// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <cstdint>
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#include <cstring>

#include "cdrom.h"
#include "lz.h"
#include "crypto.h"

#define GAP_FRAMES	2 * 75	// 2 seconds
#define GAP_SIZE	GAP_FRAMES * SECTOR_SIZE  // 2 seconds * 75 frames * 2352 sector size
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

// Pregap mapping
typedef struct {
    int mm, ss, ff;
} TIMESTAMP;

typedef struct {
    const char* game_id;
    size_t num_tracks;
    TIMESTAMP timestamps[99];
} PREGAP_OVERRIDE;


// Game ID, number of audio tracks, timestamps for pregap on each, taken from redump.org
// We calculate track 2 pregap automatically based on on diff with data track 1, but it's included here anyway though is not used
static const PREGAP_OVERRIDE pregap_overrides [] = {
    // '99 Koushien
    { "SLPS_02110", 10, {{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00} }},
    // A.IV Evolution Global
    { "SCES_00290", 6, {{00 , 15, 26},{00 , 02, 18},{00 , 02, 26},{00 , 02, 43},{00 , 02, 06},{00 , 02, 42}} },
    // Bowling
    { "SLUS_01288", 14, {{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 02, 00}} },
    // Centipede (US)
    { "SLUS_00807", 14, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00}} },
    // Centipede (Eng, Spa, Swe)
    { "SLES_01664", 14, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00}} },
    // Centipede (Dut, Fra, Ger, Ita)
    { "SLES_01900", 14, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{03, 00, 00}} },
    // Dai-4-Ji Super Robot Taisen S
    { "SLPS_00196", 2, {{00 , 02, 00}, {03, 00, 00}} },
    // GTA - not sure which ID is used so account for both
    { "SLPM_87007", 10, {{00 , 02, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00}} },
    { "SLPS_01554", 10, {{00 , 02, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00}} },
    // Hanabi Fantast
    { "SLPS_01439", 22, {{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 04, 46},{00 , 04, 18},{00 , 04, 35},{00 , 04, 8},{00 , 04, 23},{00 , 04, 61},{00 , 04, 10},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00},{00 , 05, 00}} },
    // Jet Copter X
    { "SLPM_86894", 9, {{00 , 02, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 02, 00},{00 , 02, 00}} },
    // KOF '96
    { "SLPS_00834", 40, {{00 , 06, 47},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 57},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00}} },
    // KOF Kyo
    { "SLPM_86095", 3, {{00 , 03, 00}, {00 , 02, 00}, {00 , 03, 00}} },
    // Koushien V
    { "SLPS_00729", 16, {{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00}} },
    // Motteke Tamago With Ganbare
    { "SLPS_01242", 22, {{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00},{00 , 03, 00}} },
    // Perfect Weapon (US)
    { "SLUS_00341", 15, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00},{00 , 02, 00},{00 , 02, 00},{00 , 28, 00}} },
    // Perfect Weapon (EU)
    { "SLES_00681", 15, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00},{00 , 02, 00},{00 , 02, 00},{00 , 28, 00}} },
    // Perfect Weapon (Fra)
    { "SLES_00685", 15, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00},{00 , 02, 00},{00 , 02, 00},{00 , 28, 00}} },
    // Perfect Weapon (Ger)
    { "SLES_00686", 15, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00},{00 , 02, 00},{00 , 02, 00},{00 , 28, 00}} },
    // Perfect Weapon (Ita)
    { "SLES_00687", 15, {{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 04, 00},{00 , 02, 00},{00 , 02, 00},{00 , 28, 00}} },
    // Touge Max Saisoku Drift Master - not sure which ID is used so account for all 3
    { "SCPS_45006", 11, {{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00}} },
    { "SLPS_00592", 11, {{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00}} },
    { "SLPS_91041", 11, {{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00},{00 , 04, 00}} },
    // Tsuukai!! Slot Shooting
    { "SLPS_00334", 11, {{00 , 02, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00},{00 , 01, 00}} },
    // Vib-Ribbon JP
    { "SCPS_18012", 7, {{00 , 04, 04},{00 , 02, 03},{00 , 02, 31},{00 , 02, 00},{00 , 02, 18},{00 , 02, 66},{00 , 02, 61}} },
    // Vib-Ribbon EU
    { "SCES_02873", 7, {{00 , 11, 8},{00 , 02, 03},{00 , 02, 31},{00 , 02, 00},{00 , 02, 18},{00 , 02, 66},{00 , 02, 61}} },
    //Yamasa Digi Guide Hyper Rush
    { "SLPS_02989", 10, {{00 , 03, 00},{00 , 03, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00},{00 , 02, 00}} },
};