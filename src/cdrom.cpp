// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

// Created in 2014 by Daniel Huguenin. Please ask for permission before 
// re-using this program or parts of it and mention my name in your project.
// You may redistribute this program in unaltered form as you deem fit.

#include "cdrom.h"
#include "gui.h"


struct fixImageStatus fixImage(char* inputfilepath, char* outputfilepath, int num_sectors, enum EDCMode form2EDCMode, bool verbose)
{
    //Initialize return value struct
    struct fixImageStatus status;
    status.errorcode                  = 0;
    status.mode0sectors               = 0;
    status.mode1sectors               = 0;
    status.mode2form1sectors          = 0;
    status.mode2form2sectors          = 0;
    status.form2bootsectorswithedc    = 0;
    status.form2bootsectorswithoutedc = 0;
    status.warnings                   = NULL;
    status.warningscount              = 0;

    //Sync pattern
    unsigned char sync[SYNC_SIZE] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

    //Open the input file
    FILE* inputfile;
    inputfile = fopen(inputfilepath, "rb");
    if(inputfile == NULL)
    {
        status.errorcode = ERROR_INPUT_IO_ERROR;
        return status;
    }

    //Open the output file
    FILE* outputfile;
    outputfile = fopen(outputfilepath, "wb");
    if(outputfile == NULL)
    {
        //Close the input file
        fclose(inputfile);

        status.errorcode = ERROR_OUTPUT_IO_ERROR;
        return status;
    }

    //Determine file size
    fseek(inputfile, 0, SEEK_END);
    int filesize = ftell(inputfile);
    if (filesize < num_sectors * SECTOR_SIZE)
    {
        fclose(inputfile);
        status.errorcode = ERROR_IMAGE_INCOMPLETE;
        return status;
    }
    fseek(inputfile, 0, SEEK_SET);

    if(form2EDCMode == INFER)
    {
        //If the EDC mode is to be inferred, do so by looking at the bootloader

        //Allocate memory to hold bootloader
        unsigned char* bootloader = (unsigned char*) malloc(BOOTLOADER_SIZE);

        //Read bootloader
        int bytesread = fread(bootloader, 1, BOOTLOADER_SIZE, inputfile);
        if(bytesread != BOOTLOADER_SIZE)
        {
            //Free memory
            free(bootloader);

            status.errorcode = ERROR_IMAGE_INCOMPLETE;
            fclose(inputfile);
            fclose(outputfile);
            return status;
        }

        //Inspect the EDC of the four form 2 sectors in the bootloader
        unsigned int form2sectorsinbootloader[] = {12, 13, 14, 15};
        for(int i = 0; i < sizeof(form2sectorsinbootloader) / sizeof(form2sectorsinbootloader[0]); ++i)
        {
            //Navigate to the current sector
            unsigned char* sector = bootloader + form2sectorsinbootloader[i] * SECTOR_SIZE;

            //Extract EDC
            unsigned int EDC = (sector[CDROMXA_FORM2_EDC_OFFSET + 0] << 0)
                             | (sector[CDROMXA_FORM2_EDC_OFFSET + 1] << 8)
                             | (sector[CDROMXA_FORM2_EDC_OFFSET + 2] << 16)
                             | (sector[CDROMXA_FORM2_EDC_OFFSET + 3] << 24);

            //Check if EDC is set and increment corresponding counters
            if(EDC == 0x00000000)
            {
                ++status.form2bootsectorswithoutedc;
            }
            else
            {
                ++status.form2bootsectorswithedc;
            }
        }

        //Change the EDC mode appropriately
        if(status.form2bootsectorswithoutedc >= status.form2bootsectorswithedc)
        {
            form2EDCMode = ZERO;
        }
        else
        {
            form2EDCMode = COMPUTE;
        }

        //Free memory
        free(bootloader);

        //Reset input file position (the bootloader must be fixed as well)
        fseek(inputfile, 0, SEEK_SET);
    }

    //Iterate over all sectors in the input file, fix them, and write
    unsigned char  minutes = 0x00;
    unsigned char  seconds = 0x02;
    unsigned char  blocks  = 0x00;
    unsigned char* sector  = (unsigned char*) malloc(SECTOR_SIZE);
    if(sector == NULL)
    {
        status.errorcode = ERROR_OUT_OF_MEMORY;
        fclose(inputfile);
        fclose(outputfile);
        return status;
    }
    printf("Processing %d sectors\n", num_sectors);
    status.totalsectors = 0;
    bool reached_zero_padding = false;
    for(int i = 0; i < num_sectors * SECTOR_SIZE; i += SECTOR_SIZE)
    {
        if(!reached_zero_padding)
        {
            //Read next sector
            int bytesread = fread(sector, 1, SECTOR_SIZE, inputfile);
            if(bytesread != SECTOR_SIZE)
            {
                //Free memory
                free(sector);

                //Close the input and output files
                fclose(inputfile);
                fclose(outputfile);

                status.errorcode = ERROR_IMAGE_INCOMPLETE;
                return status;
            }
        }
        else
        {
            //We've reached zero padding, generate all-zero sectors
            memset(sector, 0, SECTOR_SIZE);
        }

        //Find mode (or set to 0 if we're generating zero sectors)
        unsigned char mode;
        if(reached_zero_padding)
        {
            mode = MODE_0;
        }
        else
        {
            mode = sector[HEADER_OFFSET + 3];
        }

        //Process sector based on mode
        if(mode == MODE_0)
        {
            
            if(!reached_zero_padding)
            {
                //Check that the sector is really all-zero
                for(int j = HEADER_OFFSET + HEADER_SIZE; j < SECTOR_SIZE; ++j)
                {
                    if(sector[j] != 0x00)
                    {
                        //Free memory
                        free(sector);

                        //Close the input and output files
                        fclose(inputfile);
                        fclose(outputfile);

                        status.errorcode = ERROR_MODE0_IS_NOT_0;
                        return status;
                    }
                }
            }

            if(!reached_zero_padding)
            {
                //We have probably reached the beginning of the zero-padding - verify if that's the case

                //Make a backup of the current read position in the input file
                int inputfile_position_backup = ftell(inputfile);

            //Read the remainder of the input file and check that it is all-zero
            bool remainder_is_zero = true;
            unsigned char* temp = (unsigned char*) malloc(SECTOR_SIZE);
            if(temp == NULL)
            {
                //Free memory
                free(sector);

                //Close the input and output files
                fclose(inputfile);
                fclose(outputfile);

                status.errorcode = ERROR_OUT_OF_MEMORY;
                return status;
            }

            for(int j = i + SECTOR_SIZE; j < filesize && remainder_is_zero; j += SECTOR_SIZE)
            {
                //Read next sector
                int bytesread = fread(temp, 1, SECTOR_SIZE, inputfile);
                if(bytesread != SECTOR_SIZE)
                {
                    //Free memory
                    free(sector);
                    free(temp);

                    //Close the input and output files
                    fclose(inputfile);
                    fclose(outputfile);

                    status.errorcode = ERROR_IMAGE_INCOMPLETE;
                    return status;
                }

                //Check that the sector is all-zero
                for(int k = 0; k < SECTOR_SIZE; ++k)
                {
                    if(temp[k] != 0x00)
                    {
                        remainder_is_zero = false;
                        break;
                    }
                }
            }
            free(temp);

            if(remainder_is_zero)
            {
                //We had indeed reached the beginning of the zero-padding. 
                //Continue processing to ensure we output the expected number of sectors.
                //Set flag to generate zero sectors for remainder
                reached_zero_padding = true;
                
                //Mode 0 sector should be completely zeroed out
                memset(sector, 0, SECTOR_SIZE);

                //Update sector mode count
                ++status.mode0sectors;
            }
            else
            {
                //The mode 0 sector did NOT belong to the zero-padding - it should be added to the output file

                //Notify the user of this unexpected condition
                printf("Encountered a mode 0 sector at 0x%08X that is followed by more data. This is not expected to happen, but fixing will proceed.\n", i);

                //Restore input file position
                fseek(inputfile, inputfile_position_backup, SEEK_SET);

                //Write sync field
                memcpy(sector, sync, sizeof(sync));

                //Write header
                sector[HEADER_OFFSET + 0] = minutes;
                sector[HEADER_OFFSET + 1] = seconds;
                sector[HEADER_OFFSET + 2] = blocks;
                sector[HEADER_OFFSET + 3] = MODE_2;

                //Update sector mode count
                ++status.mode0sectors;
            }
            }
            else
            {
                //We're already in zero-padding mode, just count the mode 0 sector
                ++status.mode0sectors;
            }
            ++status.totalsectors;
        }
        else if(mode == MODE_1)
        {
            //Free memory
            free(sector);

            //Close the input and output files
            fclose(inputfile);
            fclose(outputfile);

            status.errorcode = ERROR_UNSUPPORTED_MODE;
            return status;
        }
        else if(mode == MODE_2)
        {
            //Write sync field
            memcpy(sector, sync, sizeof(sync));

            //Read subheader
            unsigned char filenumber        = sector[CDROMXA_SUBHEADER_OFFSET + 0];
            unsigned char channelnumber     = sector[CDROMXA_SUBHEADER_OFFSET + 1];
            unsigned char submode           = sector[CDROMXA_SUBHEADER_OFFSET + 2];
            unsigned char datatype          = sector[CDROMXA_SUBHEADER_OFFSET + 3];
            unsigned char filenumbercopy    = sector[CDROMXA_SUBHEADER_OFFSET + 4];
            unsigned char channelnumbercopy = sector[CDROMXA_SUBHEADER_OFFSET + 5];
            unsigned char submodecopy       = sector[CDROMXA_SUBHEADER_OFFSET + 6];
            unsigned char datatypecopy      = sector[CDROMXA_SUBHEADER_OFFSET + 7];

            //Check that the two copies of the subheader data are equivalent
            if(filenumber != filenumbercopy)
            {
                addWarning(status, "Corrupt CD-ROM XA subheader will be copied to the output file. File number mismatch at %02X:%02X:%02X: 0x%02X vs 0x%02X", 255, minutes, seconds, blocks, filenumber, filenumbercopy);
            }
            if(channelnumber != channelnumbercopy)
            {
                addWarning(status, "Corrupt CD-ROM XA subheader will be copied to the output file. Channel number mismatch at %02X:%02X:%02X: 0x%02X vs 0x%02X", 255, minutes, seconds, blocks, channelnumber, channelnumbercopy);
            }
            if(submode != submodecopy)
            {
                addWarning(status, "Corrupt CD-ROM XA subheader will be copied to the output file. Submode mismatch at %02X:%02X:%02X: 0x%02X vs 0x%02X", 255, minutes, seconds, blocks, submode, submodecopy);
            }
            if(datatype != datatypecopy)
            {
                addWarning(status, "Corrupt CD-ROM XA subheader will be copied to the output file. CD-ROM XA subheader corrupt. Data type mismatch at %02X:%02X:%02X: 0x%02X vs 0x%02X", 255, minutes, seconds, blocks, datatype, datatypecopy);
            }

            //Determine CD ROM XA Mode 2 form
            bool isForm2 = (submode & 0x20) == 0x20;

            //Compute and write EDC
            if(isForm2)
            {
                //Write header
                sector[HEADER_OFFSET + 0] = minutes;
                sector[HEADER_OFFSET + 1] = seconds;
                sector[HEADER_OFFSET + 2] = blocks;
                sector[HEADER_OFFSET + 3] = mode;

                //Handle form 2 EDC
                unsigned int EDC; //For some strange reason, a declaration in case COMPUTE would require a ; in front.
                switch(form2EDCMode)
                {
                    case KEEP:
                        //Leave the original in tact. Nothing to do here.
                        break;

                    case COMPUTE:
                        //Compute form 2 EDC
                        EDC = 0x00000000;
                        for(int i = CDROMXA_SUBHEADER_OFFSET; i < CDROMXA_FORM2_EDC_OFFSET; ++i)
                        {
                            EDC = EDC ^ sector[i];
                            EDC = (EDC >> 8) ^ EDCTable[EDC & 0x000000FF];
                        }

                        //Write EDC
                        sector[CDROMXA_FORM2_EDC_OFFSET + 0] = (EDC & 0x000000FF) >> 0;
                        sector[CDROMXA_FORM2_EDC_OFFSET + 1] = (EDC & 0x0000FF00) >> 8;
                        sector[CDROMXA_FORM2_EDC_OFFSET + 2] = (EDC & 0x00FF0000) >> 16;
                        sector[CDROMXA_FORM2_EDC_OFFSET + 3] = (EDC & 0xFF000000) >> 24;
                        break;

                    case ZERO:
                        //Write zeroed EDC
                        sector[CDROMXA_FORM2_EDC_OFFSET + 0] = 0;
                        sector[CDROMXA_FORM2_EDC_OFFSET + 1] = 0;
                        sector[CDROMXA_FORM2_EDC_OFFSET + 2] = 0;
                        sector[CDROMXA_FORM2_EDC_OFFSET + 3] = 0;
                        break;
                }

                //Update sector mode count
                ++status.mode2form2sectors;
            }
            else
            {
                //Compute form 1 EDC
                unsigned int EDC = 0x00000000;
                for(int i = CDROMXA_SUBHEADER_OFFSET; i < CDROMXA_FORM1_EDC_OFFSET; ++i)
                {
                    EDC = EDC ^ sector[i];
                    EDC = (EDC >> 8) ^ EDCTable[EDC & 0x000000FF];
                }

                //Write EDC
                sector[CDROMXA_FORM1_EDC_OFFSET + 0] = (EDC & 0x000000FF) >> 0;
                sector[CDROMXA_FORM1_EDC_OFFSET + 1] = (EDC & 0x0000FF00) >> 8;
                sector[CDROMXA_FORM1_EDC_OFFSET + 2] = (EDC & 0x00FF0000) >> 16;
                sector[CDROMXA_FORM1_EDC_OFFSET + 3] = (EDC & 0xFF000000) >> 24;

                //Write error-correction data

                //Temporarily clear header
                sector[HEADER_OFFSET + 0] = 0x00;
                sector[HEADER_OFFSET + 1] = 0x00;
                sector[HEADER_OFFSET + 2] = 0x00;
                sector[HEADER_OFFSET + 3] = 0x00;

                //Calculate P parity
                {
                    unsigned char* src = sector + HEADER_OFFSET;
                    unsigned char* dst = sector + CDROMXA_FORM1_PARITY_P_OFFSET;
                    for(int i = 0; i < 43; ++i)
                    {
                        unsigned short x = 0x0000;
                        unsigned short y = 0x0000;
                        for(int j = 19; j < 43; ++j)
                        {
                            x ^= RSPCTable[j][src[0]]; //LSB
                            y ^= RSPCTable[j][src[1]]; //MSB
                            src += 2 * 43;
                        }
                        dst[         0] = x >> 8;
                        dst[2 * 43 + 0] = x & 0xFF;
                        dst[         1] = y >> 8;
                        dst[2 * 43 + 1] = y & 0xFF;
                        dst += 2;
                        src -= (43 - 19) * 2 * 43; //Restore src to the state before the inner loop
                        src += 2;
                    }
                }

                //Calculate Q parity
                {
                    unsigned char* src = sector + HEADER_OFFSET;
                    unsigned char* dst = sector + CDROMXA_FORM1_PARITY_Q_OFFSET;
                    unsigned char* src_end = sector + CDROMXA_FORM1_PARITY_Q_OFFSET;
                    for(int i = 0; i < 26; ++i)
                    {
                        unsigned char* src_backup = src;
                        unsigned short x = 0x0000;
                        unsigned short y = 0x0000;
                        for(int j = 0; j < 43; ++j)
                        {
                            x ^= RSPCTable[j][src[0]]; //LSB
                            y ^= RSPCTable[j][src[1]]; //MSB
                            src += 2 * 44;
                            if(src >= src_end)
                            {
                                src = src - (HEADER_SIZE + CDROMXA_SUBHEADER_SIZE + CDROMXA_FORM1_USER_DATA_SIZE + EDC_SIZE + CDROMXA_FORM1_PARITY_P_SIZE);
                            }
                        }

                        dst[         0] = x >> 8;
                        dst[2 * 26 + 0] = x & 0xFF;
                        dst[         1] = y >> 8;
                        dst[2 * 26 + 1] = y & 0xFF;
                        dst += 2;
                        src = src_backup;
                        src += 2 * 43;
                    }
                }

                //Restore header
                sector[HEADER_OFFSET + 0] = minutes;
                sector[HEADER_OFFSET + 1] = seconds;
                sector[HEADER_OFFSET + 2] = blocks;
                sector[HEADER_OFFSET + 3] = mode;

                //Update sector mode count
                ++status.mode2form1sectors;
            }
            ++status.totalsectors;
        }
        else
        {
            //Free memory
            free(sector);

            //Close the input and output files
            fclose(inputfile);
            fclose(outputfile);

            status.errorcode = ERROR_UNEXPECTED_MODE;
            return status;
        }

        //Write fixed sector to output file
        int byteswritten = fwrite(sector, 1, SECTOR_SIZE, outputfile);
        if(byteswritten != SECTOR_SIZE)
        {
            //Free memory
            free(sector);

            //Close the input and output files
            fclose(inputfile);
            fclose(outputfile);

            status.errorcode = ERROR_OUTPUT_IO_ERROR;
            return status;
        }

        //Update position
        ++blocks;
        if((blocks & 0x0F) == 0x0A)
        {
            //Increment from BCD x9 to (x+1)0
            blocks += 0x06;
        }
        else
        {
            if(blocks == 0x75) //Can be in the else branch because blocks & 0x0F == 0x0A => blocks != 0x75
            {
                blocks = 0x00;
                ++seconds;
                if((seconds & 0x0F) == 0x0A)
                {
                    //Increment from BCD x9 to (x+1)0
                    seconds += 0x06;
                    if(seconds == 0x60) //Can be in the else branch because seconds & 0x0F == 0x0A => seconds != 0x60
                    {
                        seconds = 0x00;
                        ++minutes;
                        if((minutes & 0x0F) == 0x0A)
                        {
                            //Increment from BCD x9 to (x+1)0
                            minutes += 0x06;
                        }
                    }
                }
            }
        }
    }

    //Free memory
    free(sector);

    //Close the input and output files
    fclose(inputfile);
    fclose(outputfile);

    //Signal successful operation and return status
    return status;
}

int make_cdrom(char* inputfile, char* outputfile, int num_sectors, bool verbose)
{
    // Use the INFER method for EDC calculation (proved to be the more accurate approach).
	struct fixImageStatus status = fixImage(inputfile, outputfile, num_sectors, INFER, verbose);
    if (verbose)
    {
        printf("Number of mode 0 sectors:               %i\n", status.mode0sectors);
        printf("Number of mode 1 sectors:               Mode 1 sectors are not supported.\n");
        printf("Number of mode 2 form 1 sectors:        %i\n", status.mode2form1sectors);
        printf("Number of mode 2 form 2 sectors:        %i\n", status.mode2form2sectors);
        printf("Mode 2 form 2 boot sectors with EDC:    %i\n", status.form2bootsectorswithedc);
        printf("Mode 2 form 2 boot sectors without EDC: %i\n", status.form2bootsectorswithoutedc);
        printf("Total sectors processed:                %i\n", status.totalsectors);
    }
    switch(status.errorcode)
    {
        //Print success message
        case 0:
            printf("The image has been fixed!\n");
            break;

        //Print an error message
        case ERROR_OUT_OF_MEMORY:
            printf("Out of memory!\n");
            break;

        case ERROR_INPUT_IO_ERROR:
            printf("Could not open input file - terminating\n");
            break;

        case ERROR_OUTPUT_IO_ERROR:
            printf("Could not write to output file - terminating\n");
            break;

        case ERROR_IMAGE_INCOMPLETE:
            printf("Image ended prematurely! The image file you provided contains an incomplete sector.\n");
            break;

        case ERROR_UNEXPECTED_MODE:
            printf("The image has been fixed!\nProcess stopped early, this is likely due to non-standard pregap before audio tracks present.\n");
            break;

        case ERROR_UNSUPPORTED_MODE:
            printf("Mode 1 sector encountered. This program does not support such images.\n");
            break;

        case ERROR_MODE0_IS_NOT_0:
            printf("Encountered a mode 0 sector that contained non-null data. This image is corrupt!\n");
            break;

        default:
            printf("Encountered unknown error: %i\n", status.errorcode);
            break;
    }

    if(status.warningscount > 0)
    {
        printf("%d warnings occured during the process:\n", status.warningscount);
        for(int i = 0; i < status.warningscount; ++i)
        {
            printf(status.warnings[i]);
            if (i > MAX_WARNINGS)
            {
                printf("\n(%d warnings suppressed)\n", status.warningscount - i);
                break;
            }
            printf("\n");
        }
    }

    return status.totalsectors;
}
