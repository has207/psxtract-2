// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "psxtract.h"

int extract_startdat(FILE *psar, bool isMultidisc)
{
	if (psar == NULL)
	{
		printf("ERROR: Can't open input file for STARTDAT!\n");
		return -1;
	}

	// Get the STARTDAT offset (0xC for single disc and 0x10 for multidisc due to header magic length).
	int startdat_offset;
	if (isMultidisc)
		fseek(psar, 0x10, SEEK_SET);
	else
		fseek(psar, 0xC, SEEK_SET);
	fread(&startdat_offset, sizeof(startdat_offset), 1, psar);

	if (startdat_offset)
	{
		printf("Found STARTDAT offset: 0x%08x\n", startdat_offset);

		// Read the STARTDAT header.
		STARTDAT_HEADER startdat_header[sizeof(STARTDAT_HEADER)];
		memset(startdat_header, 0, sizeof(STARTDAT_HEADER));

		// Save the header as well.
		fseek(psar, startdat_offset, SEEK_SET);
		fread(startdat_header, sizeof(STARTDAT_HEADER), 1, psar);
		fseek(psar, startdat_offset, SEEK_SET);

		// Read the STARTDAT data.
		int startdat_size = startdat_header->header_size + startdat_header->data_size;
		unsigned char *startdat_data = new unsigned char[startdat_size];   
		fread(startdat_data, 1, startdat_size, psar);

		// Store the STARTDAT.
		FILE* startdat = fopen("STARTDAT.BIN", "wb");
		fwrite(startdat_data, startdat_size, 1, startdat);
		fclose(startdat);

		// Store the STARTDAT.PNG
		FILE* startdatpng = fopen("STARTDAT.PNG", "wb");
		fwrite(startdat_data + startdat_header->header_size, startdat_header->data_size, 1, startdatpng);
		fclose(startdatpng);

		delete[] startdat_data;

		printf("Saving STARTDAT as STARTDAT.BIN...\n\n");
	}

	return startdat_offset;
}

int decrypt_document(FILE* document)
{
	// Get DOCUMENT.DAT size.
	fseek(document, 0, SEEK_END);
	int document_size = ftell(document);
	fseek(document, 0, SEEK_SET);

	// Read the DOCUMENT.DAT.
	unsigned char *document_data = new unsigned char[document_size];  
	fread(document_data, document_size, 1, document);

	printf("Decrypting DOCUMENT.DAT...\n");

	// Try to decrypt as PGD.
	int pgd_size = decrypt_pgd(document_data, document_size, 2, pops_key);

	if (pgd_size > 0) 
	{
		printf("DOCUMENT.DAT successfully decrypted! Saving as DOCUMENT_DEC.DAT...\n\n");

		// Store the decrypted DOCUMENT.DAT.
		FILE* dec_document = fopen("DOCUMENT_DEC.DAT", "wb");
		fwrite(document_data, document_size, 1, dec_document);
		fclose(dec_document);
	}
	else
	{
		// If the file is not a valid PGD, then it may be DES encrypted.
		if (decrypt_doc(document_data, document_size) < 0)
		{
			printf("ERROR: DOCUMENT.DAT decryption failed!\n\n");
			delete[] document_data;
			return -1;
		}
		else
		{
			printf("DOCUMENT.DAT successfully decrypted! Saving as DOCUMENT_DEC.DAT...\n\n");

			// Store the decrypted DOCUMENT.DAT.
			FILE* dec_document = fopen("DOCUMENT_DEC.DAT", "wb");
			fwrite(document_data, document_size - 0x10, 1, dec_document);
			fclose(dec_document);
		}
	}
	delete[] document_data;
	return 0;
}

int decrypt_special_data(FILE *psar, int psar_size, int special_data_offset)
{
	if ((psar == NULL))
	{
		printf("ERROR: Can't open input file for special data!\n");
		return -1;
	}

	if (special_data_offset)
	{
		printf("Found special data offset: 0x%08x\n", special_data_offset);

		// Seek to the special data.
		fseek(psar, special_data_offset, SEEK_SET);

		// Read the data.
		int special_data_size = psar_size - special_data_offset;  // Always the last portion of the DATA.PSAR.
		unsigned char *special_data = new unsigned char[special_data_size];
		fread(special_data, special_data_size, 1, psar);

		printf("Decrypting special data...\n");

		// Decrypt the PGD and save the data.
		int pgd_size = decrypt_pgd(special_data, special_data_size, 2, NULL);

		if (pgd_size > 0)
			printf("Special data successfully decrypted! Saving as SPECIAL_DATA.BIN...\n\n");
		else
		{
			printf("ERROR: Special data decryption failed!\n\n");
			return -1;
		}

		// Store the decrypted special data.
		FILE* dec_special_data = fopen("SPECIAL_DATA.BIN", "wb");
		fwrite(special_data + 0x90, pgd_size, 1, dec_special_data);
		fclose(dec_special_data);

		// Store the decrypted special data png.
		FILE* dec_special_data_png = fopen("SPECIAL_DATA.PNG", "wb");
		fwrite(special_data + 0xAC, pgd_size - 0x1C, 1, dec_special_data_png);
		fclose(dec_special_data_png);

		delete[] special_data;
	}

	return 0;
}

int decrypt_unknown_data(FILE *psar, int unknown_data_offset, int startdat_offset)
{
	if ((psar == NULL))
	{
		printf("ERROR: Can't open input file for unknown data!\n");
		return -1;
	}

	if (unknown_data_offset)
	{
		printf("Found unknown data offset: 0x%08x\n", unknown_data_offset);

		// Seek to the unknown data.
		fseek(psar, unknown_data_offset, SEEK_SET);

		// Read the data.
		int unknown_data_size = startdat_offset - unknown_data_offset;   // Always located before the STARDAT and after the ISO.
		unsigned char *unknown_data = new unsigned char[unknown_data_size];
		fread(unknown_data, unknown_data_size, 1, psar);

		printf("Decrypting unknown data...\n");

		// Decrypt the PGD and save the data.
		int pgd_size = decrypt_pgd(unknown_data, unknown_data_size, 2, NULL);

		if (pgd_size > 0)
			printf("Unknown data successfully decrypted! Saving as UNKNOWN_DATA.BIN...\n\n");
		else
		{
			printf("ERROR: Unknown data decryption failed!\n\n");
			return -1;
		}

		// Store the decrypted unknown data.
		FILE* dec_unknown_data = fopen("UNKNOWN_DATA.BIN", "wb");
		fwrite(unknown_data + 0x90, pgd_size, 1, dec_unknown_data);
		fclose(dec_unknown_data);
		delete[] unknown_data;
	}

	return 0;
}

int decrypt_iso_header(FILE *psar, int header_offset, int header_size, unsigned char *pgd_key, int disc_num)
{
	if (psar == NULL)
	{
		printf("ERROR: Can't open input file for ISO header!\n");
		return -1;
	}

	// Seek to the ISO header.
	fseek(psar, header_offset, SEEK_SET);

	// Read the ISO header.
	unsigned char *iso_header = new unsigned char[header_size];
	fread(iso_header, header_size, 1, psar);

	printf("Decrypting ISO header...\n");

	// Decrypt the PGD and get the block table.
	int pgd_size = decrypt_pgd(iso_header, header_size, 2, pgd_key);

	if (pgd_size > 0)
		printf("ISO header successfully decrypted! Saving as ISO_HEADER.BIN...\n\n");
	else
	{
		printf("ERROR: ISO header decryption failed!\n\n");
		return -1;
	}

	// Choose the output ISO header file name based on the disc number.
	char iso_header_filename[0x12];
	sprintf(iso_header_filename, "ISO_HEADER.BIN");

	// Store the decrypted ISO header.
	FILE* dec_iso_header = fopen(iso_header_filename, "wb");
	fwrite(iso_header + 0x90, pgd_size, 1, dec_iso_header);
	fclose(dec_iso_header);
	if (disc_num > 0)
	{
		sprintf(iso_header_filename, "ISO_HEADER_%d.BIN", disc_num);
		dec_iso_header = fopen(iso_header_filename, "wb");
		fwrite(iso_header + 0x90, pgd_size, 1, dec_iso_header);
		fclose(dec_iso_header);
	}
	delete[] iso_header;

	return 0;
}

int decrypt_iso_map(FILE *psar, int map_offset, int map_size, unsigned char *pgd_key)
{
	if (psar == NULL)
	{
		printf("ERROR: Can't open input file for ISO disc map!\n");
		return -1;
	}

	// Seek to the ISO map.
	fseek(psar, map_offset, SEEK_SET);

	// Read the ISO map.
	unsigned char *iso_map = new unsigned char[map_size];
	fread(iso_map, map_size, 1, psar);

	printf("Decrypting ISO disc map...\n");

	// Decrypt the PGD and get the block table.
	int pgd_size = decrypt_pgd(iso_map, map_size, 2, pgd_key);

	if (pgd_size > 0)
		printf("ISO disc map successfully decrypted! Saving as ISO_MAP.BIN...\n\n");
	else
	{
		printf("ERROR: ISO disc map decryption failed!\n\n");
		return -1;
	}

	// Store the decrypted ISO disc map.
	FILE* dec_iso_map = fopen("ISO_MAP.BIN", "wb");
	fwrite(iso_map + 0x90, pgd_size, 1, dec_iso_map);
	fclose(dec_iso_map);
	delete[] iso_map;

	return 0;
}

int build_iso(FILE *psar, FILE *iso_table, int base_offset, int disc_num)
{
	if ((psar == NULL) || (iso_table == NULL))
	{
		printf("ERROR: Can't open input files for ISO!\n");
		return -1;
	}

	// Setup buffers.
	int iso_block_size = 0x9300;
	unsigned char iso_block_comp[0x9300];   // Compressed block.
	unsigned char iso_block_decomp[0x9300]; // Decompressed block.
	memset(iso_block_comp, 0, iso_block_size);
	memset(iso_block_decomp, 0, iso_block_size);

	// Locate the block table.
	int table_offset = 0x3C00;  // Fixed offset.
	fseek(iso_table, table_offset, SEEK_SET);

	// Choose the output ISO file name based on the disc number.
	char iso_filename[0x10];
	if (disc_num > 0)
		sprintf(iso_filename, "ISO_%d.BIN", disc_num);
	else
		sprintf(iso_filename, "ISO.BIN");

	// Open a new file to write overdump
	FILE* overdump = fopen("OVERDUMP.BIN", "wb");
	// Open a new file to write the ISO image.
	FILE* iso = fopen(iso_filename, "wb");
	if (iso == NULL)
	{
		printf("ERROR: Can't open output file for ISO!\n");
		return -1;
	}

	int iso_base_offset = 0x100000 + base_offset;  // Start of compressed ISO data.
	ISO_ENTRY entry[sizeof(ISO_ENTRY)];
	memset(entry, 0, sizeof(ISO_ENTRY));

	// Read the first entry.
	fread(entry, sizeof(ISO_ENTRY), 1, iso_table);

	// Keep reading entries until we reach the end of the table.
	while (entry->size > 0)
	{
		// Locate the block offset in the DATA.PSAR.
		fseek(psar, iso_base_offset + entry->offset, SEEK_SET);
		fread(iso_block_comp, entry->size, 1, psar);

		// Decompress if necessary.
		if (entry->size < iso_block_size)   // Compressed.
			decompress(iso_block_decomp, iso_block_comp, iso_block_size);
		else								// Not compressed.
			memcpy(iso_block_decomp, iso_block_comp, iso_block_size);

		// trash and overdump generating
		if (entry->marker == 0)
		{
			int trash_start = 0, trash_size = 0;
			unsigned int sector;			
			do
			{
				// search for first non 00 FF FF FF
				sector = iso_block_decomp[trash_start] + 256 * (iso_block_decomp[trash_start + 1] + 256 * (iso_block_decomp[trash_start + 2] + 256 * iso_block_decomp[trash_start + 3]));
				trash_start = trash_start + 2352;
			} while (sector == 0xFFFFFF00);
			trash_start = trash_start - 2352;
			do
			{
				// search for first zero padding (4 bytes length)
				sector = iso_block_decomp[trash_start + trash_size] + 256 * (iso_block_decomp[trash_start + trash_size + 1] + 256 * (iso_block_decomp[trash_start + trash_size + 2] + 256 * iso_block_decomp[trash_start + trash_size + 3]));
				trash_size = trash_size + 4;
			} while (sector != 0);
			trash_size = trash_size - 4;
			if (trash_size != 0)
			{
				FILE* trash = fopen("TRASH.BIN", "wb");
				fwrite(iso_block_decomp + trash_start, trash_size, 1, trash);
				fclose(trash);
				// write before trash
				// fwrite(iso_block_decomp, trash_start, 1, iso);
				// write after trash
				// fwrite(iso_block_decomp + trash_start + trash_size, iso_block_size - trash_start - trash_size, 1, iso);
				// start writing overdump
				fwrite(iso_block_decomp + trash_start + trash_size, iso_block_size - trash_start - trash_size, 1, overdump);
			}
			else
				fwrite(iso_block_decomp, iso_block_size, 1, overdump);
		}

		// Write it to the output file.
		fwrite(iso_block_decomp, iso_block_size, 1, iso);
					
		// Clear buffers.
		memset(iso_block_comp, 0, iso_block_size);
		memset(iso_block_decomp, 0, iso_block_size);

		// Go to next entry.
		table_offset += sizeof(ISO_ENTRY);
		fseek(iso_table, table_offset, SEEK_SET);
		fread(entry, sizeof(ISO_ENTRY), 1, iso_table);
	}
	fclose(overdump);
	fclose(iso);
	return 0;
}
						             
unsigned int ROTR32(unsigned int v, int n)
{
  return (((n &= 32 - 1) == 0) ? v : (v >> n) | (v << (32 - n)));
}



int unscramble_atrac_data(unsigned char *track_data, CDDA_ENTRY *track)
{
	unsigned int blocks = (track->size / NBYTES) / 0x10;
	unsigned int chunks_rest = (track->size / NBYTES) % 0x10;
	printf("chunk_rest is %d\n", chunks_rest);
	unsigned int *ptr = (unsigned int*)track_data;
	unsigned int tmp = 0, tmp2 = track->checksum, value = 0;
	
	
	
	// for each block
	while(blocks)
	{
		// for each chunk of block
		for(int i = 0; i < 0x10; i++)
		{
			tmp = tmp2;
			
			// for each value of chunk
			for(int k = 0; k < (NBYTES / 4); k++)
			{
				value = ptr[k];
				ptr[k] = (tmp ^ value);
		    tmp = tmp2 + (value * 123456789);
			}
			
			tmp2 = ROTR32(tmp2, 1);
			ptr += (NBYTES / 4); // pointer on next chunk
		}
		
		blocks--;
	}
	
	// do rest chunks
	for(unsigned int i = 0; i < chunks_rest; i++)
  {
    tmp = tmp2;
	  
	  // for each value of chunk
	  for(int k = 0; k < (NBYTES / 4); k++)
	  {
	    value = ptr[k];
		  ptr[k] = (tmp ^ value);
		  tmp = tmp2 + (value * 123456789);
	  }
    
    tmp2 = ROTR32(tmp2, 1);
	  ptr += (NBYTES / 4); // next chunk
  }
  
	return 0;
}

int fill_at3_header(AT3_HEADER *header, CDDA_ENTRY *audio_entry, int track_sectors) {
	memset(header, 0, sizeof(AT3_HEADER));  // reset before filling
	memcpy(header->riff_id, "RIFF", 4);
	header->riff_size = audio_entry->size + sizeof(AT3_HEADER) - 8;
	memcpy(header->riff_format, "WAVE", 4);
	memcpy(header->fmt_id, "fmt\x20", 4);
	header->fmt_size = 32;
	header->codec_id = 624;
	header->channels = 2;
	header->sample_rate = 44100;
	header->unknown1 = 16538;
	header->bytes_per_frame = 384;
	header->param_size = 14;
	header->param1 = 1;
	header->param2 = 4096;
	header->param3 = 0;
	header->param4 = 0;
	header->param5 = 0;
	header->param6 = 1;
	header->param7 = 0;
	memcpy(header->fact_id, "fact", 4);
	header->fact_size = 8;
	header->fact_param1 = track_sectors * 2352 / 4;
	header->fact_param2 = 1024;
	memcpy(header->data_id, "data", 4);
	header->data_size = audio_entry->size;
	return 0;
}

int extract_frames_from_cue(FILE *iso_table, int cue_offset, int gap)
{
	CUE_ENTRY cue_entry[sizeof(CUE_ENTRY)];
	memset(cue_entry, 0, sizeof(CUE_ENTRY));

	fseek(iso_table, cue_offset, SEEK_SET);
	fread(cue_entry, sizeof(CUE_ENTRY), 1, iso_table);
	int mm1, ss1, ff1;
	unsigned char mm = cue_entry->I1m;
	unsigned char ss = cue_entry->I1s;
	unsigned char ff = cue_entry->I1f;
	if (cue_entry->type == 0x01) // Sanity check that this is an Audio track
	{
		// convert 0xXY into decimal XY
		mm1 = 10 * (mm - mm % 16) / 16 + mm % 16;
		ss1 = (10 * (ss - ss % 16) / 16 + ss % 16) - gap;
		ff1 = 10 * (ff - ff % 16) / 16 + ff % 16;
		//printf("Offset %dm:%ds:%df\n", mm1, ss1, ff1);
		return (mm1 * 60 * 75) + (ss1 * 75) + ff1;
	}
	else
		printf("Last track found\n");
	return -1;
}

void audio_file_name(char* filename, int track_num, char* extension)
{
	sprintf(filename, "TRACK %02d.%s", track_num, extension);
}

int build_audio_at3(FILE *psar, FILE *iso_table, int base_offset, unsigned char *pgd_key)
{	
	if ((psar == NULL) || (iso_table == NULL))
	{
		printf("ERROR: Can't open input files for extracting audio tracks!\n");
		return -1;
	}
	char track_filename[0x10];
	int track_num = 1;
	int iso_base_offset = 0x100000 + base_offset;  // Start of audio tracks.
	
	AT3_HEADER at3_header[sizeof(AT3_HEADER)];
	CDDA_ENTRY audio_entry[sizeof(CDDA_ENTRY)];
	memset(audio_entry, 0, sizeof(CDDA_ENTRY));

	CUE_ENTRY cue_entry_cur[sizeof(CUE_ENTRY)];
	memset(cue_entry_cur, 0, sizeof(CUE_ENTRY));

	CUE_ENTRY cue_entry_next[sizeof(CUE_ENTRY)];
	memset(cue_entry_next, 0, sizeof(CUE_ENTRY));

	// Read track 02
	int cue_offset = 0x428;  // track 02 offset
	int audio_offset = 0x800;  // Fixed audio table offset.
	
	fseek(iso_table, audio_offset, SEEK_SET);
	fread(audio_entry, sizeof(CDDA_ENTRY), 1, iso_table);
	if (audio_entry->offset == 0)
	{
		printf("there is no audio tracks in the ISO!\n");
		return 0;
	}
	int track_size, cur_track_offset, next_track_offset;
	while (audio_entry->offset)
	{
		cur_track_offset = extract_frames_from_cue(iso_table, cue_offset, 2);
		next_track_offset = extract_frames_from_cue(iso_table, cue_offset + sizeof(CUE_ENTRY), 2);
		if (next_track_offset < 0)
		{
			// get disc size to calculate last track, no gap after last track
			next_track_offset = extract_frames_from_cue(iso_table, 0x414, 0);
		}
		track_size = next_track_offset - cur_track_offset;
		printf("Track size %d sectors\n", track_size);
		// Choose the output track file name based on the counter.
		track_num++;
		
		// Locate the block offset in the DATA.PSAR.
		printf("Seeking into position %x\n", iso_base_offset + audio_entry->offset);
		fseek(psar, iso_base_offset + audio_entry->offset, SEEK_SET);

		// Read the data.
		unsigned char *track_data = new unsigned char[audio_entry->size + NBYTES];
		fread(track_data, audio_entry->size + NBYTES, 1, psar);
		
		
		// Store the decrypted track data.
		// Open a new file to write the track image with AT3 header
		audio_file_name(track_filename, track_num, "AT3");
		FILE* track = fopen(track_filename, "wb");
		if (track == NULL)
		{
			printf("ERROR: Can't open output file for audio track %d!\n", track_num);
			return -1;
		}

		printf("Extracting audio track...%02d!\n", track_num);
		
		unscramble_atrac_data(track_data, audio_entry);
		
		fill_at3_header(at3_header, audio_entry, track_size);

		fwrite(at3_header, sizeof(AT3_HEADER), 1, track);
		fwrite(track_data, audio_entry->size, 1, track);
		
		fclose(track);
		delete[] track_data;
		
		audio_offset += sizeof(CDDA_ENTRY);
		cue_offset += sizeof(CUE_ENTRY);
		// Go to next entry.
		fseek(iso_table, audio_offset, SEEK_SET);
		fread(audio_entry, sizeof(CDDA_ENTRY), 1, iso_table);
	}
	return track_num - 1;
}

int convert_at3_to_wav(int num_tracks)
{
	for (int i = 2; i <= num_tracks + 1; i++)
	{
		char at3_filename[0x10];
		audio_file_name(at3_filename, i, "AT3");
		struct stat st;
		if (stat(at3_filename, &st) != 0 || st.st_size == 0)
		{
			printf("%s doesn't exist or empty, aborting...\n", at3_filename);
			return -1;
		}

		char wav_filename[0x10];
		audio_file_name(wav_filename, i, "WAV");

		char wdir[_MAX_PATH];
		char command[_MAX_PATH + 50];

		if (GetModuleFileName(NULL, wdir, MAX_PATH) == 0)
		{
			printf("ERROR: Failed to obtained current directory\n%d\n", GetLastError());
			return 1;
		}

		// Find the last backslash in the path, and null-terminate there to remove the executable name
		char* last_backslash = strrchr(wdir, '\\');
		if (last_backslash) {
			*last_backslash = '\0';
		}
		sprintf(command, "%s\\at3tool.exe", wdir);
		if (stat(command, &st) != 0)
		{
			printf("ERROR: Failed to find at3tool.exe, aborting...");
			return -1;
		}
		sprintf(command, "%s\\msvcr71.dll", wdir);
		if (stat(command, &st) != 0)
		{
			printf("ERROR: Failed to find msvcr71.dll needed by at3tool.exe, aborting...");
			return -1;
		}
		sprintf(command, "%s\\at3tool.exe -d \"%s\" \"%s\"", wdir, at3_filename, wav_filename);
		printf("%s\n", command);
		// It would be nice to check error codes/output but we just assume it goes as planned
		// and check that a WAV file is created and non-zero later
		system(command);
		printf("\n");

	}
	return num_tracks;
}

int convert_wav_to_bin(int num_tracks)
{
	for (int i = 2; i <= num_tracks + 1; i++)
	{
		char wav_filename[0x10];
		audio_file_name(wav_filename, i, "WAV");
		struct stat st;
		if (stat(wav_filename, &st) != 0 || st.st_size == 0)
		{
			printf("%s doesn't exist or empty, aborting...\n", wav_filename);
			return -1;
		}
		FILE* wav_file = fopen(wav_filename, "rb");
		if (wav_file == NULL)
		{
			printf("ERROR: Can't open %s, aborting...\n", wav_filename);
			return -1;
		}
		// For some reason extracted AT3 files end up having the 2 second gap at the end rather
		// than at the start.
		// So we use the opportunity to move it to the front while stripping the WAVE header.
		// Unfortunately we also have an issue with the last track ending up being too short
		// by what appears to be roughly 2 seconds, so
		// I suspect something in the decoding/unscrambling step is a little bit off,
		// that results with the tracks to be read with the gaps shifted over,
		// and the last track missing its 2 second gap.
		// So we introduce a gross hack here, where we move the gaps to the front and also 
		// pad the last track with zeroes until its expected length.
		// This tends to work out in practice, but is pretty gross
		// and it would be better to figure out what's wrong in the earlier decoding step.
		unsigned char gap[GAP_SIZE];
		fseek(wav_file, st.st_size - GAP_SIZE, SEEK_SET);
		fread(gap, GAP_SIZE, 1, wav_file);

		char bin_filename[0x10];
		audio_file_name(bin_filename, i, "BIN");
		FILE* bin_file = fopen(bin_filename, "wb");
		if (bin_file == NULL)
		{
			printf("ERROR: Can't open %s, aborting...\n", bin_filename);
			return -1;
		}
		fwrite(gap, GAP_SIZE, 1, bin_file);
		fseek(wav_file, 44, SEEK_SET);  // skip the WAVE header
		int data_size = st.st_size - GAP_SIZE - 44;
		unsigned char* audio_data = (unsigned char*)malloc(data_size);
		fread(audio_data, data_size, 1, wav_file);
		fwrite(audio_data, data_size, 1, bin_file);
		if (audio_data != NULL) free(audio_data);

		// grab the expected size from the AT3 header
		char at3_filename[0x10];
		audio_file_name(at3_filename, i, "AT3");
		FILE* at3_file = fopen(at3_filename, "rb");
		if (at3_file != NULL)
		{
			AT3_HEADER at3_header[sizeof(AT3_HEADER)];
			fread(at3_header, sizeof(AT3_HEADER), 1, at3_file);
			long expected_size = at3_header->fact_param1 * 4;
			fseek(bin_file, 0, SEEK_END);
			long file_size = ftell(bin_file);
			if (file_size < expected_size)
			{
				printf("Need to pad %d bytes\n", expected_size - file_size);
				unsigned char zero[1];
				fwrite(zero, 1, expected_size - file_size, bin_file);
			}
		}
		else
			printf("WARNING: Can't open %s, skipping padding step...\n", at3_filename);
		fclose(bin_file);
	}
	return num_tracks;
}

int convert_iso(FILE *iso_table, char *iso_file_name, char *cdrom_file_name, char *cue_file_name, unsigned char *iso_disc_name)
{
	// Set the CD-ROM file path.
	char cdrom_file_path[256] = "../CDROM/", cue_file_path[256] = "../CDROM/";
	strcat(cdrom_file_path, cdrom_file_name);
	strcat(cue_file_path, cue_file_name);

	// Patch ECC/EDC and build a new proper CD-ROM image for this ISO.
	make_cdrom(iso_file_name, cdrom_file_path, false);

	// Generate a CUE file for mounting/burning.
	FILE* cue_file = fopen(cue_file_path, "wb");
	if (cue_file == NULL)
	{
		printf("ERROR: Can't write CUE file!\n");
		return -1;
	}

	printf("Generating CUE file...\n");

	char cue[0x100];
	memset(cue, 0, 0x100);
	sprintf(cue, "FILE \"%s\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n", cdrom_file_name);
	fputs(cue, cue_file);

	// genereating cue table
	CUE_ENTRY cue_entry[sizeof(CUE_ENTRY)];
	memset(cue_entry, 0, sizeof(CUE_ENTRY));

	int cue_offset = 0x428;  // track 02 offset
	int i = 1;

	// Read track 02
	fseek(iso_table, cue_offset, SEEK_SET);
	fread(cue_entry, sizeof(CUE_ENTRY), 1, iso_table);
	int track_num = 2;
	FILE* bin_file = fopen(cdrom_file_path, "ab");
	if (bin_file == NULL)
	{
		printf("ERROR: Can't open %s for updating!\n", cdrom_file_path);
		return -1;
	}
	while (cue_entry->type)
	{
		char track_filename[0x10];
		audio_file_name(track_filename, track_num, "BIN");
		FILE* track_file = fopen(track_filename, "rb");
		if (track_file == NULL)
		{
			printf("ERROR: %s cannot be opened\n", track_filename);
			return -1;
		}
		fseek(track_file, 0, SEEK_END);
		int track_size = ftell(track_file);
		unsigned char* track_data = (unsigned char*)malloc(track_size);
		fseek(track_file, 0, SEEK_SET);
		fread(track_data, track_size, 1, track_file);
		fwrite(track_data, track_size, 1, bin_file);
		fclose(track_file);
		int ff1, ss1, mm1, mm0, ss0;
		i++;
		// convert 0xXY into decimal XY
		mm1 = 10 * (cue_entry->I1m - cue_entry->I1m % 16) / 16 + cue_entry->I1m % 16;
		ss1 = 10 * (cue_entry->I1s - cue_entry->I1s % 16) / 16 + cue_entry->I1s % 16 - 2; // minus 2 seconds pregap
		if (ss1 < 0)
		{
			ss1 = 60 + ss1;
			mm1 = mm1 - 1;
		}
		ff1 = 10 * (cue_entry->I1f - cue_entry->I1f % 16) / 16 + cue_entry->I1f % 16;

		memset(cue, 0, 0x100);
		sprintf(cue, "  TRACK %02d AUDIO\n", i);
		fputs(cue, cue_file);
		memset(cue, 0, 0x100);
		ss0 = ss1 - 2;
		mm0 = mm1;
		if (ss0 < 0)
		{
			ss0 = 60 + ss0;
			mm0 = mm1 - 1;
		}
		sprintf(cue, "    INDEX 00 %02d:%02d:%02d\n", mm0, ss0, ff1);
		fputs(cue, cue_file);
		
		memset(cue, 0, 0x100);
		sprintf(cue, "    INDEX 01 %02d:%02d:%02d\n", mm1, ss1, ff1);
		fputs(cue, cue_file);

		cue_offset += sizeof(CUE_ENTRY);
		// Read next track
		fseek(iso_table, cue_offset, SEEK_SET);
		fread(cue_entry, sizeof(CUE_ENTRY), 1, iso_table);
		track_num++;
	}

	fclose(cue_file);
	fclose(bin_file);

	return 0;
}

int decrypt_single_disc(FILE *psar, int psar_size, int startdat_offset, unsigned char *pgd_key, bool conv)
{
	// Decrypt the ISO header and get the block table.
	// NOTE: In a single disc, the ISO header is located at offset 0x400 and has a length of 0xB6600.
	if (decrypt_iso_header(psar, 0x400, 0xB6600, pgd_key, 0))
		printf("Aborting...\n");

	// Re-open in read mode (just to be safe).
	FILE* iso_table = fopen("ISO_HEADER.BIN", "rb");
	if (iso_table == NULL)
	{
		printf("ERROR: No decrypted ISO header found!\n");
		return -1;
	}

	// Save the ISO disc name and title (UTF-8).
	unsigned char iso_title[0x80];
	unsigned char iso_disc_name[0x10];
	memset(iso_title, 0, 0x80);
	memset(iso_disc_name, 0, 0x10);

	fseek(iso_table, 0, SEEK_SET);
	fread(iso_disc_name, 0x10, 1, iso_table);
	fseek(iso_table, 0xE2C, SEEK_SET);
	fread(iso_title, 0x80, 1, iso_table);

	printf("ISO disc: %s\n", strip_utf8(iso_disc_name, 0x10));
	printf("ISO title: %s\n\n", strip_utf8(iso_title, 0x80));

	// Seek inside the ISO table to find the special data offset.
	int special_data_offset;
	fseek(iso_table, 0xE20, SEEK_SET);  // Always at 0xE20.
	fread(&special_data_offset, sizeof(special_data_offset), 1, iso_table);

	// Decrypt the special data if it's present.
	// NOTE: Special data is normally a PNG file with an intro screen of the game.
	decrypt_special_data(psar, psar_size, special_data_offset);

	// Seek inside the ISO table to find the unknown data offset.
	int unknown_data_offset;
	fseek(iso_table, 0xED4, SEEK_SET);  // Always at 0xED4.
	fread(&unknown_data_offset, sizeof(unknown_data_offset), 1, iso_table);

	// Decrypt the unknown data if it's present.
	// NOTE: Unknown data is a binary chunk with unknown purpose (memory snapshot?).
	if (startdat_offset > 0)
		decrypt_unknown_data(psar, unknown_data_offset, startdat_offset);

	// Build the ISO image.
	printf("Building the final ISO image...\n");
	if (build_iso(psar, iso_table, 0, 0))
		printf("ERROR: Failed to reconstruct the ISO image!\n\n");
	else
		printf("ISO image successfully reconstructed! Saving as ISO.BIN...\n\n");

	int num_tracks = build_audio_at3(psar, iso_table, 0, pgd_key);
	if (num_tracks < 0)
		printf("ERROR: Audio track extraction failed!\n");
	else
		printf("%d audio tracks extracted to ATRAC3\n", num_tracks);

	if (convert_at3_to_wav(num_tracks) < 0)
		printf("ERROR: ATRAC3 to WAV conversion failed!\n");
	else
		printf("%d audio tracks converted to WAV\n", num_tracks);

	if (convert_wav_to_bin(num_tracks) < 0)
		printf("ERROR: WAV to BIN conversion failed!\n");
	else
		printf("%d audio tracks converted to BIN\n", num_tracks);

	printf("\n");

	// Convert the final ISO image if required.
	if (conv)
	{
		printf("Converting the final ISO image...\n");
		if (convert_iso(iso_table, "ISO.BIN", "CDROM.BIN", "CDROM.CUE", iso_disc_name))
			printf("ERROR: Failed to convert the ISO image!\n");
		else
			printf("ISO image successfully converted to CD-ROM format!\n");
	}

	fclose(iso_table);
	return 0;
}

int decrypt_multi_disc(FILE *psar, int psar_size, int startdat_offset, unsigned char *pgd_key, bool conv)
{
	// Decrypt the multidisc ISO map header and get the disc map.
	// NOTE: The ISO map header is located at offset 0x200 and 
	// has a length of 0x2A0 (0x200 of real size + 0xA0 for the PGD header).
	if (decrypt_iso_map(psar, 0x200, 0x2A0, pgd_key))
		printf("Aborting...\n");

	// Re-open in read mode (just to be safe).
	FILE* iso_map = fopen("ISO_MAP.BIN", "rb");
	if (iso_map == NULL)
	{
		printf("ERROR: No decrypted ISO disc map found!\n");
		return -1;
	}

	// Parse the ISO disc map:
	// - First 0x14 bytes are discs' offsets (maximum of 5 discs);
	// - The following 0x50 bytes contain a 0x10 hash for each disc (maximum of 5 hashes);
	// - The next 0x20 bytes contain the disc ID;
	// - Next 4 bytes represent the special data offset followed by 4 bytes of padding (NULL);
	// - Next 0x80 bytes form an unknown data block (discs' signatures?);
	// - The final data block contains the disc title, NULL padding and some unknown integers.

	// Get the discs' offsets.
	int disc_offset[5];
	for (int i = 0; i < 5; i++)
		fread(&disc_offset[i], sizeof(int), 1, iso_map);


	// Get the disc collection ID and title (UTF-8).
	unsigned char iso_title[0x80];
	unsigned char iso_disc_name[0x10];
	memset(iso_title, 0, 0x80);
	memset(iso_disc_name, 0, 0x10);

	fseek(iso_map, 0x64, SEEK_SET);
	fread(iso_disc_name, 0x10, 1, iso_map);
	fseek(iso_map, 0x10C, SEEK_SET);
	fread(iso_title, 0x80, 1, iso_map);

	printf("ISO disc: %s\n", strip_utf8(iso_disc_name, 0x10));
	printf("ISO title: %s\n\n", strip_utf8(iso_title, 0x80));

	// Seek inside the ISO map to find the special data offset.
	int special_data_offset;
	fseek(iso_map, 0x84, SEEK_SET);  // Always at 0x84 (after disc ID space).
	fread(&special_data_offset, sizeof(special_data_offset), 1, iso_map);

	// Decrypt the special data if it's present.
	// NOTE: Special data is normally a PNG file with an intro screen of the game.
	decrypt_special_data(psar, psar_size, special_data_offset);

	// Build each valid ISO image.
	int disc_count = 0;

	for (int i = 0; i < MAX_DISCS; i++)
	{
		if (disc_offset[i] > 0)
		{
			// Decrypt the ISO header and get the block table.
			// NOTE: In multidisc, the ISO header is located at the disc offset + 0x400 bytes. 
			if (decrypt_iso_header(psar, disc_offset[i] + 0x400, 0xB6600, pgd_key, i + 1))
				printf("Aborting...\n");

			// Re-open in read mode (just to be safe).
			FILE* iso_table = fopen("ISO_HEADER.BIN", "rb");
			if (iso_table == NULL)
			{
				printf("ERROR: No decrypted ISO header found!\n");
				return -1;
			}
			// Build the first ISO image.
			printf("Building the ISO image number %d...\n", i + 1);
			if (build_iso(psar, iso_table, disc_offset[i], i + 1))
				printf("ERROR: Failed to reconstruct the ISO image number %d!\n\n", i + 1);
			else
				printf("ISO image successfully reconstructed! Saving as ISO_%d.BIN...\n\n", i + 1);

			// Convert the ISO image if required.
			if (conv)
			{
				printf("Converting ISO image number %d...\n", i + 1);
				char iso_x_bin[0x10];
				sprintf(iso_x_bin, "ISO_%d.BIN", i + 1);
				char cdrom_x_bin[0x10];
				sprintf(cdrom_x_bin, "CDROM_%d.BIN", i + 1);
				char cdrom_x_cue[0x10];
				sprintf(cdrom_x_cue, "CDROM_%d.CUE", i + 1);
				if (convert_iso(iso_table, iso_x_bin, cdrom_x_bin, cdrom_x_cue, iso_disc_name))
					printf("ERROR: Failed to convert ISO image number %d!\n\n", i + 1);
				else
					printf("ISO image number %d successfully converted to CD-ROM format!\n\n", i + 1);
			}
			disc_count++;
			fclose(iso_table);
		}
	}

	printf("Successfully reconstructed %d ISO images!\n", disc_count);
	fclose(iso_map);
	return 0;
}

int main(int argc, char **argv)
{
	if ((argc <= 1) || (argc > 5))
	{
		printf("*****************************************************\n");
		printf("psxtract - Convert your PSOne Classics to ISO format.\n");
		printf("         - Written by Hykem (C).\n");
		printf("*****************************************************\n\n");
		printf("Usage: psxtract [-c] <EBOOT.PBP> <DOCUMENT.DAT> <KEYS.BIN>\n");
		printf("[-c] - Convert raw image to the original PSOne CD-ROM format.\n");
		printf("<EBOOT.PBP> - Your PSOne Classic main PBP.\n");
		printf("<DOCUMENT.DAT> - Game manual file (optional).\n");
		printf("<KEYS.BIN> - Key file (optional).\n");
		return 0;
	}

	// Keep track of the each argument's offset.
	int arg_offset = 0;

	// Check if we're converting data into CD-ROM format.
	bool conv = false;
	if (!strcmp(argv[1], "-c"))
	{
		conv = true;
		arg_offset++;
	}

	FILE* input = fopen(argv[arg_offset + 1], "rb");

	// Start KIRK.
	kirk_init();

	// Set an empty PGD key.
	unsigned char pgd_key[0x10] = {};

	// If a DOCUMENT.DAT was supplied, try to decrypt it.
	if ((argc - arg_offset) >= 3)
	{
		FILE* document = fopen(argv[arg_offset + 2], "rb");
		if (document != NULL) {
			decrypt_document(document);
			fclose(document);
		}
	}

	// Use a supplied key when available.
	// NOTE: KEYS.BIN is not really needed since we can generate a key from the PGD 0x70 MAC hash.
	if ((argc - arg_offset) >= 4)
	{
		FILE* keys = fopen(argv[arg_offset + 3], "rb");
		fread(pgd_key, sizeof(pgd_key), 1, keys);
		fclose(keys);

		int i;
		printf("Using PGD key: ");
		for(i = 0; i < 0x10; i++)
			printf("%02X", pgd_key[i]);
		printf("\n\n");
	}

	printf("Unpacking PBP %s...\n", argv[arg_offset + 1]);

	// Setup a new directory to output the unpacked contents.
	_mkdir("PBP");
	_chdir("PBP");

	// Unpack the EBOOT.PBP file.
	if (unpack_pbp(input))
	{
		printf("ERROR: Failed to unpack %s!", argv[arg_offset + 1]);
		_chdir("..");
		_rmdir("PBP");
		return -1;
	}
	else
		printf("Successfully unpacked %s!\n\n", argv[arg_offset + 1]);

	// Change the directory back.
	_chdir("..");

	// Make a directory for CD-ROM images if required.
	if (conv)
		_mkdir("CDROM");

	// Make a new directory for the ISO data.
	_mkdir("ISO");
	_chdir("ISO");

	// Locate DATA.PSAR.
	FILE* psar = fopen("../PBP/DATA.PSAR", "rb");
	if (psar == NULL)
	{
		printf("ERROR: No DATA.PSAR found!\n");
		return -1;
	}

	// Get DATA.PSAR size.
	fseek(psar, 0, SEEK_END);
	int psar_size = ftell(psar);
	fseek(psar, 0, SEEK_SET);

	// Check PSISOIMG0000 or PSTITLEIMG0000 magic.
	// NOTE: If the file represents a single disc, then PSISOIMG0000 is used.
	// However, for multidisc ISOs, the PSTITLEIMG0000 additional header
	// is used to hold data relative to the different discs.
	unsigned char magic[0x10];
	bool isMultidisc;
	fread(magic, sizeof(magic), 1, psar);

	if (memcmp(magic, iso_magic, 0xC) != 0)
	{
		if (memcmp(magic, multi_iso_magic, 0x10) != 0)
		{
			printf("ERROR: Not a valid ISO image!\n");
			return -1;
		}
		else
		{
			printf("Multidisc ISO detected!\n\n");
			isMultidisc = true;
		}
	}
	else
	{
		printf("Single disc ISO detected!\n\n");
		isMultidisc = false;
	}

	// Extract the STARTDAT sector.
	// NOTE: STARTDAT data is normally a PNG file with an intro screen of the game.
	int startdat_offset = extract_startdat(psar, isMultidisc);

	// Decrypt the disc(s).
	if (isMultidisc)
		decrypt_multi_disc(psar, psar_size, startdat_offset, pgd_key, conv);
	else
		decrypt_single_disc(psar, psar_size, startdat_offset, pgd_key, conv);

	// Change the directory back.
	_chdir("..");

	fclose(psar);
	fclose(input);

	return 0;
}