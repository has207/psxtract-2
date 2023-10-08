// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "psxtract.h"

char* exec(const char* cmd) {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES saAttr;
    char* output = NULL;
    DWORD outputSize = 0;

    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe to capture the output
    if (!CreatePipe(&hRead, &hWrite, &saAttr, 0))
        return NULL;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb = sizeof(STARTUPINFO);
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Create the process
    BOOL bSuccess = CreateProcess(NULL, (LPSTR)cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!bSuccess) {
        CloseHandle(hWrite);
        CloseHandle(hRead);
        return NULL;
    }

    CloseHandle(hWrite); // Close write end of pipe

    // Read the output from the pipe
    DWORD dwRead;
    CHAR chBuf[4096];
    while (ReadFile(hRead, chBuf, 4096, &dwRead, NULL) && dwRead > 0) {
        output = (char*)realloc(output, outputSize + dwRead + 1);
        memcpy(output + outputSize, chBuf, dwRead);
        outputSize += dwRead;
        output[outputSize] = '\0';
    }

    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return output;
}

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
	long document_size = ftell(document);
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

int decrypt_iso_header(FILE *psar, int header_offset, unsigned char *pgd_key, int disc_num)
{
	if (psar == NULL)
	{
		printf("ERROR: Can't open input file for ISO header!\n");
		return -1;
	}

	// Seek to the ISO header.
	fseek(psar, header_offset, SEEK_SET);

	// Read the ISO header.
	unsigned char *iso_header = new unsigned char[ISO_HEADER_SIZE];
	fread(iso_header, ISO_HEADER_SIZE, 1, psar);

	printf("Decrypting ISO header...\n");

	// Decrypt the PGD and get the block table.
	int pgd_size = decrypt_pgd(iso_header, ISO_HEADER_SIZE, 2, pgd_key);

	if (pgd_size > 0)
		printf("ISO header successfully decrypted! Saving as ISO_HEADER_%d.BIN...\n\n", disc_num);
	else
	{
		printf("ERROR: ISO header decryption failed!\n\n");
		return -1;
	}

	// Choose the output ISO header file name based on the disc number.
	char iso_header_filename[0x14];
	if (disc_num > 0)
		sprintf(iso_header_filename, "ISO_HEADER_%d.BIN", disc_num);
	else
		sprintf(iso_header_filename, "ISO_HEADER.BIN");

	// Store the decrypted ISO header.
	FILE* dec_iso_header = fopen(iso_header_filename, "wb");
	fwrite(iso_header + 0x90, pgd_size, 1, dec_iso_header);

	fclose(dec_iso_header);
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

int build_data_track(FILE *psar, FILE *iso_table, int disc_offset, int disc_num)
{
	if ((psar == NULL) || (iso_table == NULL))
	{
		printf("ERROR: Can't open input files for ISO!\n");
		return -1;
	}

	// Setup buffers.
	unsigned char iso_block_comp[ISO_BLOCK_SIZE];   // Compressed block.
	unsigned char iso_block_decomp[ISO_BLOCK_SIZE]; // Decompressed block.
	memset(iso_block_comp, 0, ISO_BLOCK_SIZE);
	memset(iso_block_decomp, 0, ISO_BLOCK_SIZE);

	// Locate the block table.
	int table_offset = 0x3C00;  // Fixed offset.
	fseek(iso_table, table_offset, SEEK_SET);

	// Choose the output file name based on the disc number.
	char iso_filename[0x10];
	if (disc_num > 0)
		sprintf(iso_filename, "DATA_%d.BIN", disc_num);  // multi-disc
	else
		sprintf(iso_filename, "DATA_TRACK.BIN");  // single disc

	// Open a new file to write overdump
	FILE* overdump = fopen("OVERDUMP.BIN", "wb");
	// Open a new file to write the ISO image.
	FILE* iso = fopen(iso_filename, "wb");
	if (iso == NULL)
	{
		printf("ERROR: Can't open output file for ISO!\n");
		return -1;
	}

	int iso_offset = ISO_BASE_OFFSET + disc_offset;  // Start of compressed ISO data.
	printf("ISO offset %x\n", iso_offset);
	int read_size = 0;
	int block_count = 0;
	ISO_ENTRY entry[sizeof(ISO_ENTRY)];
	memset(entry, 0, sizeof(ISO_ENTRY));

	// Read the first entry.
	fread(entry, sizeof(ISO_ENTRY), 1, iso_table);

	// Keep reading entries until we reach the end of the table.
	while (entry->size > 0)
	{
		read_size += entry->size;
		if (block_count % 100 == 0) printf(".");
		// Locate the block offset in the DATA.PSAR.
		fseek(psar, iso_offset + entry->offset, SEEK_SET);
		fread(iso_block_comp, entry->size, 1, psar);

		// Decompress if necessary.
		if (entry->size < ISO_BLOCK_SIZE)   // Compressed.
			decompress(iso_block_decomp, iso_block_comp, ISO_BLOCK_SIZE);
		else								// Not compressed.
			memcpy(iso_block_decomp, iso_block_comp, ISO_BLOCK_SIZE);

		// trash and overdump generating
		if (entry->marker == 0)
		{
			int trash_start = 0, trash_size = 0;
			unsigned int sector;			
			do
			{
				// search for first non 00 FF FF FF
				sector = iso_block_decomp[trash_start] + 256 * (iso_block_decomp[trash_start + 1] + 256 * (iso_block_decomp[trash_start + 2] + 256 * iso_block_decomp[trash_start + 3]));
				trash_start = trash_start + SECTOR_SIZE;
			} while (sector == 0xFFFFFF00);
			trash_start = trash_start - SECTOR_SIZE;
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
				fwrite(iso_block_decomp + trash_start + trash_size, ISO_BLOCK_SIZE - trash_start - trash_size, 1, overdump);
			}
			else
				fwrite(iso_block_decomp, ISO_BLOCK_SIZE, 1, overdump);
		}

		// Write it to the output file.
		fwrite(iso_block_decomp, ISO_BLOCK_SIZE, 1, iso);
		block_count++;
					
		// Clear buffers.
		memset(iso_block_comp, 0, ISO_BLOCK_SIZE);
		memset(iso_block_decomp, 0, ISO_BLOCK_SIZE);

		// Go to next entry.
		table_offset += sizeof(ISO_ENTRY);
		fseek(iso_table, table_offset, SEEK_SET);
		fread(entry, sizeof(ISO_ENTRY), 1, iso_table);
	}
	printf("\n");
	printf("Raw data track written to %s\n", iso_filename);
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
	header->fact_param1 = track_sectors * SECTOR_SIZE / 4;
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
	// According to documentation http://endlessparadigm.com/forum/showthread.php?tid=14
	// and for the vast majority of games tested Audio track type should be 0x01,
	// however at least one case exists (Castlevania: SOTN) where the track types
	// are increased by 0x20 (so data track is 0x61 rather than 0x41 and audio is
	// 0x21 rather than 0x01).
	if (cue_entry->type == 0x01 || cue_entry->type == 0x21
		|| cue_entry->type == 0x41 || cue_entry->type == 0x61)
	{
		// convert 0xXY into decimal XY
		mm1 = 10 * (mm - mm % 16) / 16 + mm % 16;
		ss1 = (10 * (ss - ss % 16) / 16 + ss % 16) - gap;
		ff1 = 10 * (ff - ff % 16) / 16 + ff % 16;
		printf("Offset %dm:%ds:%df at %04x\n", mm1, ss1, ff1, cue_offset);
		return (mm1 * 60 * 75) + (ss1 * 75) + ff1;
	}
	// once we hit invalid track type, indicate this is last track
	// at which point we'll need to get entire disc size to calculate its length
	return -1;
}

int get_track_size_from_cue(FILE *iso_table, int cue_offset)
{
	int cur_track_offset = extract_frames_from_cue(iso_table, cue_offset, 2);
	if (cur_track_offset < 0) {
		printf("ERROR: unable to get current track offset, aborting...\n");
		return -1;
	}
	int next_track_offset = extract_frames_from_cue(iso_table, cue_offset + sizeof(CUE_ENTRY), 2);
	if (next_track_offset < 0)
	{
		// get disc size to calculate last track, no gap after last track
		next_track_offset = extract_frames_from_cue(iso_table, CUE_LEADOUT_OFFSET, 0);
		if (next_track_offset < 0)
		{
			"ERROR: last track size calculation failed, aborting...\n";
			return -1;
		}
	}
	return next_track_offset - cur_track_offset;
}

int data_track_sectors(FILE *iso_table)
{
	int track = 1;
	int cue_offset = 0x41E;  // track 01 offset
	int track_size = get_track_size_from_cue(iso_table, cue_offset) - GAP_FRAMES;  // subtract 2 seconds
	if (track_size < 0) {
		printf("Unable to get data track size\n");
		return -1;
	}
	return track_size;
}

void audio_file_name(char* filename, int disc_num, int track_num, char* extension)
{
	sprintf(filename, "D%02d_TRACK%02d.%s", disc_num, track_num, extension);
}

int build_audio_at3(FILE *psar, FILE *iso_table, int base_audio_offset, unsigned char *pgd_key, int disc_num, const PREGAP_OVERRIDE* pregap_override)
{	
	if ((psar == NULL) || (iso_table == NULL))
	{
		printf("ERROR: Can't open input files for extracting audio tracks!\n");
		return -1;
	}
	char track_filename[0x10];
	int track_num = 1;
	
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
		printf("There are no CDDA audio tracks, continuing...\n");
		return 0;
	}
	while (audio_entry->offset)
	{
		track_num++;
		int track_size = get_track_size_from_cue(iso_table, cue_offset);

        if (pregap_override != NULL)
        {
            const TIMESTAMP* curr_t = &pregap_override->timestamps[track_num - 2];
            int curr_pregap = (curr_t->mm * 60 + curr_t->ss) * 75 + curr_t->ff;
            if (track_num - 1 < pregap_override->num_tracks)
            {
                printf("checking pregap override\n");
                // check if the pregap of the next track is less than 2
                const TIMESTAMP* next_t = &pregap_override->timestamps[track_num - 1];
                int next_pregap = (next_t->mm * 60 + next_t->ss) * 75 + next_t->ff;
                if (next_pregap < GAP_FRAMES && curr_pregap >= GAP_FRAMES)
                {
                    printf("Track needs additional padding due to short pregap on following track\n");
                    track_size += (GAP_FRAMES - next_pregap);
                }
            }
            else  // last track
            {
                if (curr_pregap < GAP_FRAMES)
                {
                    printf("Truncating last track by %d frames\n", (GAP_FRAMES - curr_pregap));
                    track_size -= (GAP_FRAMES - curr_pregap);
                }
            }
        }
		if (track_size < 0)
		{
			printf("ERROR: retrieving offset for track %d, aborting...\n", track_num);
			return -1;
		}
		
		// Locate the block offset in the DATA.PSAR.
		printf("seeking to %x + %x (%x)\n", base_audio_offset, audio_entry->offset, base_audio_offset + audio_entry->offset);
		fseek(psar, base_audio_offset + audio_entry->offset, SEEK_SET);

		// Read the data.
		unsigned char *track_data = new unsigned char[audio_entry->size + NBYTES];
		fread(track_data, audio_entry->size + NBYTES, 1, psar);
		
		
		// Store the decrypted track data.
		// Open a new file to write the track image with AT3 header
		audio_file_name(track_filename, disc_num, track_num, "AT3");
		FILE* track = fopen(track_filename, "wb");
		if (track == NULL)
		{
			printf("ERROR: Can't open output file for audio track %d!\n", track_num);
			return -1;
		}

		printf("Extracting audio track %d (%d sectors, %d bytes)\n", track_num, track_size, audio_entry->size + NBYTES);
		
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

int convert_at3_to_wav(int disc_num, int num_tracks)
{
	if (num_tracks > 0)
		printf("\nAttempting to convert from ATRAC3 to WAV, this may take awhile...\n\n");
	for (int i = 2; i <= num_tracks + 1; i++)
	{
		char at3_filename[0x10];
		audio_file_name(at3_filename, disc_num, i, "AT3");
		struct stat st;
		if (stat(at3_filename, &st) != 0 || st.st_size == 0)
		{
			printf("%s doesn't exist or empty, aborting...\n", at3_filename);
			return -1;
		}

		char wav_filename[0x10];
		audio_file_name(wav_filename, disc_num, i, "WAV");

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
			printf("ERROR: Failed to find at3tool.exe, aborting...\n");
			return -1;
		}
		sprintf(command, "%s\\msvcr71.dll", wdir);
		if (stat(command, &st) != 0)
		{
			printf("ERROR: Failed to find msvcr71.dll needed by at3tool.exe, aborting...\n");
			return -1;
		}
		sprintf(command, "cmd /c %s\\at3tool.exe -d \"%s\" \"%s\"", wdir, at3_filename, wav_filename);
		printf("%s\n", command);
		char *result = exec(command);
        if (result) {
            printf("%s\n", result);
            free(result);
        }
		if (stat(wav_filename, &st) != 0 || st.st_size <= 44)
		{
			printf("%s failed to convert, ignoring audio tracks...\n", wav_filename);
			return -1;
		}
		if (stat(wav_filename, &st) == 0)
		{
			FILE* at3_file = fopen(at3_filename, "rb");
			if (at3_file == NULL)
			{
				printf("ERROR: Can't open %s for verification, aborting...\n", at3_filename);
				return -1;
			}
			AT3_HEADER at3_header[sizeof(AT3_HEADER)];
			fread(at3_header, sizeof(AT3_HEADER), 1, at3_file);
			printf("%s created with size %d (expected %d)...\n", wav_filename, st.st_size, 44 + at3_header->fact_param1 * 4);
			fclose(at3_file);
		}
		else
			printf("Unable to open %s for verification...\n", wav_filename);
		printf("\n");

	}
	return num_tracks;
}

// EBOOT stores audio tracks with the assumption that a 2 second pregap exists on each track.
// This assumption results in information loss wrt original pregap values present on physical
// discs. While a vast majority of audio tracks on most games do in fact have 2 second pregaps,
// there are notable exceptions, for the most part with larger pregaps present on one or more
// tracks on the disc. And there are also games where the pregaps on disc are only
// 1 second long as opposed to 2 or more and these are the most troublesome.
//
// In all cases, the EBOOT seems to store only *up to* 2 seconds pregap silence at the end of the track
// regardless of how long the pregap should be and we have no way to figure this out from the data
// at runtime.
//
// Luckily, there are very few games that have this issue and they have all been enumerated
// and custom pregap values are hardcoded for them so that we can generate accurate images. In
// most cases having shorter, i.e. only 2 seconds, of pregap instead of the slightly longer ones
// on the disc should not matter, but there are cases where the pregap is intentionally quite long
// and there are also two specific eboots that have 1 second pregaps, so special handling is
// required to produce working results in all cases.
//
// In any case, we need to strip the WAVE header and generate the required gaps
// by moving pregap silence to the front, then further prepad the track
// to ensure it has expected length gap, as defined by the CUE values relative to
// actual data size of the previous track. Such prepadding only really works with track 2, i.e.
// the very first audio track, since we know the exact length of the data track and the start of
// first audio track can always be kept consistent with the disc.
//
// On further audio tracks we assume 2 second gaps unless the game is known to have different
// pregap timings. Finally we pad the last track with zeroes until the disc reaches its expected
// length.
int convert_wav_to_bin(int data_gap, int disc_num, int num_tracks, const PREGAP_OVERRIDE* pregap_override)
{
	printf("\nAttempting to convert WAV audio to BIN...\n\n");
	for (int i = 2; i <= num_tracks + 1; i++)
	{
		char wav_filename[0x10];
		audio_file_name(wav_filename, disc_num, i, "WAV");
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

		fseek(wav_file, 0, SEEK_END);
		long wav_size = ftell(wav_file);
		fseek(wav_file, wav_size - GAP_SIZE, SEEK_SET);

		char bin_filename[0x10];
		audio_file_name(bin_filename, disc_num, i, "BIN");
		FILE* bin_file = fopen(bin_filename, "wb");
		if (bin_file == NULL)
		{
			printf("ERROR: Can't open %s, aborting...\n", bin_filename);
			fclose(wav_file);
			return -1;
		}

		// grab the expected size from the AT3 header
		long expected_size = -1;
		char at3_filename[0x10];
		audio_file_name(at3_filename, disc_num, i, "AT3");
		FILE* at3_file = fopen(at3_filename, "rb");
		if (at3_file != NULL)
		{
			AT3_HEADER at3_header[sizeof(AT3_HEADER)];
			fread(at3_header, sizeof(AT3_HEADER), 1, at3_file);
			expected_size = at3_header->fact_param1 * 4;
		}
		else
			printf("WARNING: Can't open %s, skipping padding step...\n", at3_filename);
        int gap_frames = GAP_FRAMES;
        if (pregap_override != NULL && i > 2 && i - 2 < pregap_override->num_tracks)
        {
            // check if the pregap for this track gets an override
            const TIMESTAMP* t = &pregap_override->timestamps[i - 2];
            printf("Overriding pregap with %02d:%02d:%02d\n", t->mm, t->ss, t->ff);
            gap_frames = (t->mm * 60 + t->ss) * 75 + t->ff;
        }
		int pregap_size = (((i == 2) ? data_gap : gap_frames) - 1) * SECTOR_SIZE;

		char zero = '\0';
		printf("Adding gap %d bytes...\n", pregap_size);
		fseek(bin_file, pregap_size - 1, SEEK_SET);
		fwrite(&zero, 1, 1, bin_file);

		fseek(wav_file, 44, SEEK_SET);  // skip the WAVE header
		int data_size = wav_size - pregap_size - 44;

        if (pregap_override != NULL && i - 1 < pregap_override->num_tracks)
        {
            // check if the pregap of the next track is less than 2
            const TIMESTAMP* t = &pregap_override->timestamps[i - 1];
            int next_pregap = (t->mm * 60 + t->ss) * 75 + t->ff;
            if (next_pregap < gap_frames)
            {
                printf("Extending data_size due to short next_gap (%d < %d)\n", next_pregap, gap_frames);
                data_size += (gap_frames - next_pregap) * SECTOR_SIZE;
            }
        }
        if (data_size + pregap_size > expected_size)
            data_size = expected_size - pregap_size;

		unsigned char* audio_data = (unsigned char*)malloc(data_size);
		if (audio_data == NULL) {
			printf("Unable to allocated data size %d, aborting...\n", data_size);
			if (at3_file != NULL) fclose(at3_file);
			fclose(bin_file);
			fclose(wav_file);
			return -1;
		}
		fread(audio_data, data_size, 1, wav_file);
		fwrite(audio_data, data_size, 1, bin_file);
		free(audio_data);

		fseek(bin_file, 0, SEEK_END);
		long file_size = ftell(bin_file);
		if (file_size < expected_size)
		{
			printf("Padding track %d with additional %d bytes\n", i, expected_size - file_size);
			long new_pos = expected_size - 1;
			fseek(bin_file, new_pos, SEEK_SET);
			int written = fwrite(&zero, 1, 1, bin_file);
		}

		fclose(at3_file);
		fclose(wav_file);
		fclose(bin_file);
	}
	return num_tracks;
}

int copy_track_to_iso(FILE *bin_file, char *track_filename, int track_num)
{
	FILE* track_file = fopen(track_filename, "rb");
	if (track_file == NULL)
	{
		printf("ERROR: %s cannot be opened\n", track_filename);
		return -1;
	}
	printf("\tadding %s\n", track_filename);
	fseek(track_file, 0, SEEK_END);
	long track_size = ftell(track_file);
	unsigned char* track_data = (unsigned char*)malloc(track_size);
	if (track_data == NULL)
	{
		printf("ERROR: could not allocate memory to read data, aborting...\n");
		fclose(track_file);
		return -1;
	}
	fseek(track_file, 0, SEEK_SET);
	fread(track_data, track_size, 1, track_file);
	fwrite(track_data, track_size, 1, bin_file);
	fclose(track_file);
	free(track_data);
	return track_size;
}

int fix_iso(FILE *iso_table, char* data_track_file_name, char* data_fixed_file_path)
{
	// Patch ECC/EDC and build a new proper CD-ROM image for this ISO.
	printf("Patching ECC/EDC data...\n");
	int num_sectors_expected = data_track_sectors(iso_table);
	if (num_sectors_expected < 0)
	{
		return -1;
	}
	int actual_data_sectors = make_cdrom(data_track_file_name, data_fixed_file_path, num_sectors_expected, true);
	int gap = num_sectors_expected - actual_data_sectors + GAP_FRAMES;
	printf("Gap after data track: %d sectors\n", gap);
	return gap;
}

int build_bin_cue(FILE *iso_table, char *data_fixed_file_path, char *cdrom_file_name, char *cue_file_name, char *iso_disc_name, int disc_num, int data_gap, const PREGAP_OVERRIDE *pregap_override
)
{
	char cdrom_file_path[256] = "../";
	strcat(cdrom_file_path, cdrom_file_name);
	char cue_file_path[256] = "../";
	strcat(cue_file_path, cue_file_name);

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

	// Copy data track
	FILE* bin_file = fopen(cdrom_file_path, "wb");
	if (bin_file == NULL)
	{
		printf("ERROR: Can't open %s!\n", cdrom_file_path);
		return -1;
	}
	int data_track_size = copy_track_to_iso(bin_file, data_fixed_file_path, 1);
	if (data_track_size < 0)
	{
		printf("Error copying data track to CDROM.BIN, aborting...\n");
		fclose(bin_file);
		return -1;
	}
	// Read track 02
	fseek(iso_table, cue_offset, SEEK_SET);
	fread(cue_entry, sizeof(CUE_ENTRY), 1, iso_table);
	int track_num = 2;
	while (cue_entry->type)
	{
		char track_filename[0x10];
		audio_file_name(track_filename, disc_num, track_num, "BIN");
		int audio_track_size = copy_track_to_iso(bin_file, track_filename, track_num);
		if (audio_track_size < 0)
		{
			// If audio track copy failed on track 2 just bail and generate valid cue for the data track.
			// This is mainly a workaround for RE2 EBOOT having audio track pointers but no audio tracks
			if (track_num == 2)
			{
				printf("Proceeding to generate CUE file without audio tracks\n");
				break;
			}
			else
			{
				printf("ERROR: failed to copy track %d to CDROM.BIN, aborting...\n", track_num);
				fclose(cue_file);
				fclose(bin_file);
				return -1;
			}
		}

		int ff1, ss1, mm1, mm0, ss0, ff0;
		i++;
		// convert 0xXY into decimal XY
		mm1 = 10 * (cue_entry->I1m - cue_entry->I1m % 16) / 16 + cue_entry->I1m % 16;
		ss1 = mm1 * 60 + 10 * (cue_entry->I1s - cue_entry->I1s % 16) / 16 + cue_entry->I1s % 16;
		ff1 = ss1 * 75 + 10 * (cue_entry->I1f - cue_entry->I1f % 16) / 16 + cue_entry->I1f % 16;
		ff1 -= GAP_FRAMES;
		memset(cue, 0, 0x100);
		sprintf(cue, "  TRACK %02d AUDIO\n", i);
		fputs(cue, cue_file);
		memset(cue, 0, 0x100);
		int pregap_frames = GAP_FRAMES;
		if (pregap_override != NULL) {
			int mm = pregap_override->timestamps[track_num - 2].mm;
			int ss = pregap_override->timestamps[track_num - 2].ss;
			int ff = pregap_override->timestamps[track_num - 2].ff;
			pregap_frames = ff + 75 * ss + 75 * 60 * mm;
			printf("Overriding pregap with %02d:%02d%02d (%d)\n", mm, ss, ff, pregap_frames);
		}
		ff0 = ff1 - (track_num == 2 ? data_gap : pregap_frames);
		ss0 = ff0 / 75;
		mm0 = ss0 / 60;
		ss0 = ss0 % 60;
		ff0 = ff0 % 75;
		ss1 = ff1 / 75;
		mm1 = ss1 / 60;
		ss1 = ss1 % 60;
		ff1 = ff1 % 75;
		sprintf(cue, "    INDEX 00 %02d:%02d:%02d\n", mm0, ss0, ff0);
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

int extract_and_convert_audio(FILE *psar, FILE *iso_table, int base_audio_offset, unsigned char *pgd_key, int disc_num, int data_gap, const PREGAP_OVERRIDE* pregap_override)
{
	printf("\nAttempting to extract audio tracks...\n\n");
	int num_tracks = build_audio_at3(psar, iso_table, base_audio_offset, pgd_key, disc_num, pregap_override);
	if (num_tracks < 0) {
		printf("ERROR: Audio track extraction failed!\n");
		return -1;
	}
	else if (num_tracks > 0)
		printf("%d audio tracks extracted to ATRAC3\n", num_tracks);

	if (convert_at3_to_wav(disc_num, num_tracks) < 0)
	{
		printf("ATRAC3 to WAV conversion failed!\n\n");
		return 0;
	}
	else if (num_tracks > 0)
		printf("%d audio tracks converted to WAV\n", num_tracks);

	if (convert_wav_to_bin(data_gap, disc_num, num_tracks, pregap_override) < 0)
	{
		printf("ERROR: WAV to BIN conversion failed!\n\n");
		return -1;
	}
	else if (num_tracks > 0)
		printf("%d audio tracks converted to BIN\n\n", num_tracks);
	return num_tracks;
}

const PREGAP_OVERRIDE* find_pregap_mapping(char* game_id)
{
    for (int i = 0; i < sizeof(pregap_overrides) / sizeof(PREGAP_OVERRIDE); i++)
    {
        if (strcmp(game_id, pregap_overrides[i].game_id) == 0)
            return &pregap_overrides[i];
    }
    return NULL;
}

int decrypt_single_disc(FILE* psar, int psar_size, int startdat_offset, unsigned char* pgd_key)
{
	// Decrypt the ISO header and get the block table.
	// NOTE: In a single disc, the ISO header is located at offset 0x400 and has a length of 0xB6600.
	if (decrypt_iso_header(psar, ISO_HEADER_OFFSET, pgd_key, 0) < 0)
		return -1;

	// Re-open in read mode (just to be safe).
	FILE* iso_table = fopen("ISO_HEADER.BIN", "rb");
	if (iso_table == NULL)
	{
		printf("ERROR: No decrypted ISO header found!\n");
		return -1;
	}

	// Save the ISO disc name and title (UTF-8).
	char iso_title[0x80];
	char iso_disc_name[0x10];
	memset(iso_title, 0, 0x80);
	memset(iso_disc_name, 0, 0x10);

	fseek(iso_table, 1, SEEK_SET);
	fread(iso_disc_name, 0x0F, 1, iso_table);
	fseek(iso_table, 0xE2C, SEEK_SET);
	fread(iso_title, 0x80, 1, iso_table);

	printf("ISO disc: %s\n", iso_disc_name);
	printf("ISO title: %s\n\n", iso_title);

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

	// Build the data track image.
	printf("Building the data track...\n");
	if (build_data_track(psar, iso_table, 0, 0) < 0)
		printf("ERROR: Failed to reconstruct the data track!\n");
	else
		printf("Data track successfully reconstructed!\n");

	printf("\n");

	char data_bin[16] = "DATA_TRACK.BIN";
	char data_bin_fixed[256];
	memset(data_bin_fixed, 0, 256);
	strcat(data_bin_fixed, data_bin);
	strcat(data_bin_fixed, ".ISO");
	int data_gap = fix_iso(iso_table, data_bin, data_bin_fixed);
	if (data_gap < 0)
	{
		printf("ERROR: unable to fix data track %s\n", data_bin);
		fclose(iso_table);
		return -1;
	}
	printf("\n");

    const PREGAP_OVERRIDE* pregap_override = find_pregap_mapping(iso_disc_name);
    if (pregap_override != NULL) {
        printf("Using custom pregaps for %s (%d tracks)\n", iso_disc_name, pregap_override->num_tracks);
    }

	// Handle audio tracks
	if (extract_and_convert_audio(psar, iso_table, ISO_BASE_OFFSET, pgd_key, 1, data_gap, pregap_override) < 0)
	{
		printf("ERROR: extract and convert audio failed, aborting...\n");
		fclose(iso_table);
		return -1;
	}

	// Convert to BIN/CUE.
	printf("Converting the final image to BIN/CUE...\n");
	if (build_bin_cue(iso_table, data_bin_fixed, "CDROM.BIN", "CDROM.CUE", iso_disc_name, 1, data_gap, pregap_override))
	{
		printf("ERROR: Failed to convert to BIN/CUE!\n");
		fclose(iso_table);
		return -1;
	}
	else
		printf("Disc successfully converted to BIN/CUE format!\n");

	fclose(iso_table);
	return 0;
}

int decrypt_multi_disc(FILE *psar, int psar_size, int startdat_offset, unsigned char *pgd_key)
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
	char iso_title[0x80];
	char iso_disc_name[0x10];
	memset(iso_title, 0, 0x80);
	memset(iso_disc_name, 0, 0x10);

	fseek(iso_map, 0x65, SEEK_SET);
	fread(iso_disc_name, 0x0F, 1, iso_map);
	fseek(iso_map, 0x10C, SEEK_SET);
	fread(iso_title, 0x80, 1, iso_map);

	printf("ISO disc: %s\n", iso_disc_name);
	printf("ISO title: %s\n\n", iso_title);

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
			if (decrypt_iso_header(psar, disc_offset[i] + ISO_HEADER_OFFSET, pgd_key, i + 1) < 0)
			{
				fclose(iso_map);
				return -1;
			}
			// Re-open in read mode (just to be safe).
			char iso_header_filename[0x14];
			sprintf(iso_header_filename, "ISO_HEADER_%d.BIN", i + 1);
			FILE* iso_table = fopen(iso_header_filename, "rb");
			if (iso_table == NULL)
			{
				printf("ERROR: No decrypted ISO header found!\n");
				return -1;
			}

			// Build the data track.
			printf("Building data track for disc %d...\n", i + 1);
			if (build_data_track(psar, iso_table, disc_offset[i], i + 1) < 0)
				printf("ERROR: Failed to reconstruct data track for disc %d!\n", i + 1);
			else
				printf("Data track successfully reconstructed for disc %d!\n", i + 1);
			printf("\n");

			char data_x_bin[0x10];
			sprintf(data_x_bin, "DATA_%d.BIN", i + 1);
			char data_x_bin_fixed[256];
			memset(data_x_bin_fixed, 0, 256);
			strcat(data_x_bin_fixed, data_x_bin);
			strcat(data_x_bin_fixed, ".ISO");
			int data_gap = fix_iso(iso_table, data_x_bin, data_x_bin_fixed);
			if (data_gap < 0)
			{
				printf("ERROR: unable to fix data track\n");
				fclose(iso_table);
				return -1;
			}
			printf("\n");

			// Attempt to extact and convert audio tracks
			if (extract_and_convert_audio(psar, iso_table, disc_offset[i] + ISO_BASE_OFFSET, pgd_key, i + 1, data_gap, NULL) < 0)
			{
				printf("ERROR: extract and convert audio failed, aborting...\n");
				fclose(iso_table);
				return -1;
			}

			// Convert to BIN/CUE
			printf("Converting disc %d to BIN/CUE...\n", i + 1);

			char cdrom_x_bin[0x10];
			sprintf(cdrom_x_bin, "CDROM_%d.BIN", i + 1);
			char cdrom_x_cue[0x10];
			sprintf(cdrom_x_cue, "CDROM_%d.CUE", i + 1);
			if (build_bin_cue(iso_table, data_x_bin_fixed, cdrom_x_bin, cdrom_x_cue, iso_disc_name, i + 1, data_gap, NULL))
				printf("ERROR: Encountered issues converting disc %d to BIN/CUE!\n\n", i + 1);
			else
				printf("Disc %d successfully converted to BIN/CUE format!\n\n", i + 1);

			disc_count++;
			fclose(iso_table);
		}
	}

	printf("Successfully reconstructed %d discs!\n", disc_count);
	fclose(iso_map);
	return 0;
}

int main(int argc, char **argv)
{
	SetConsoleOutputCP(CP_UTF8);
	if ((argc <= 1) || (argc > 5))
	{
		printf("*****************************************************\n");
		printf("psxtract - Convert your PSOne Classics to BIN/CUE format.\n");
		printf("         - Written by Hykem (C).\n");
		printf("*****************************************************\n\n");
		printf("Usage: psxtract [-c] <EBOOT.PBP> [DOCUMENT.DAT] [KEYS.BIN]\n");
		printf("[-c] - Clean up temporary files after finishing.\n");
		printf("EBOOT.PBP - Your PSOne Classic main PBP.\n");
		printf("DOCUMENT.DAT - Game manual file (optional).\n");
		printf("KEYS.BIN - Key file (optional).\n");
		return 0;
	}

	// Keep track of the each argument's offset.
	int arg_offset = 0;

	// Check if we want to clean up temp files before exiting.
	bool cleanup = false;
	if (!strcmp(argv[1], "-c"))
	{
		cleanup = true;
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
	// Make a new directory for intermediate data.
	_mkdir("TEMP");
	_chdir("TEMP");

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

	_chdir("..");

	// Locate DATA.PSAR.
	FILE* psar = fopen("PBP/DATA.PSAR", "rb");
	if (psar == NULL)
	{
		printf("ERROR: No DATA.PSAR found!\n");
		return -1;
	}

	// Get DATA.PSAR size.
	fseek(psar, 0, SEEK_END);
	long psar_size = ftell(psar);
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
			printf("Multi-disc game detected!\n\n");
			isMultidisc = true;
		}
	}
	else
	{
		printf("Single disc game detected!\n\n");
		isMultidisc = false;
	}

	// Extract the STARTDAT sector.
	// NOTE: STARTDAT data is normally a PNG file with an intro screen of the game.
	int startdat_offset = extract_startdat(psar, isMultidisc);

	// Decrypt the disc(s).
	if (isMultidisc)
		decrypt_multi_disc(psar, psar_size, startdat_offset, pgd_key);
	else
		decrypt_single_disc(psar, psar_size, startdat_offset, pgd_key);

	// Change the directory back.
	_chdir("..");

	fclose(psar);
	fclose(input);

	if (cleanup)
	{
		printf("Cleanup requested, removing TEMP folder\n");
		printf("[If you see errors above try running without -c to leave TEMP files in place in order to debug.]\n");
		system("rmdir /S /Q TEMP");
	}
	return 0;
}