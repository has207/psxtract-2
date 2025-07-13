#include "at3acm.h"
#include "gui.h"
#include <stdio.h>
#include <string.h>

// Define printf to use GUI-aware version
#define printf gui_printf

#define WAVE_FORMAT_SONY_SCX 0x0270

static BOOL WINAPI findAt3Driver_cb(HACMDRIVERID hadid, DWORD_PTR dwInstance, DWORD fdwSupport) {
    ACMDRIVERDETAILS details;
    details.cbStruct = sizeof(ACMDRIVERDETAILS);
    acmDriverDetails(hadid, &details, 0);

    if (strcmp(details.szShortName, "ATRAC3") == 0) {
        *(LPHACMDRIVERID)dwInstance = hadid;
        return FALSE;
    }

    return TRUE;
}

void findAt3Driver(LPHACMDRIVERID lpHadid) {
    acmDriverEnum(findAt3Driver_cb, (DWORD_PTR)lpHadid, 0);
}

int convertAt3ToWav(const char* input, const char* output, HACMDRIVERID at3hadid) {
    FILE *sfp = fopen(input, "rb");
    if (!sfp) {
        printf("ERROR: Cannot open AT3 input file: %s\n", input);
        return 1;
    }

    unsigned char *bFmt = nullptr, *bData = nullptr;
    unsigned int sizeFmt = 0, sizeData = 0;

    char cb[12];
    fread(cb, 1, 12, sfp);
    if (memcmp(cb, "RIFF", 4) || memcmp(&cb[8], "WAVE", 4)) {
        printf("ERROR: Not a WAV file: %s\n", input);
        fclose(sfp);
        return 1;
    }

    while (!feof(sfp)) {
        char magic[4];
        DWORD32 blockSize;
        fread(magic, 1, 4, sfp);
        if (feof(sfp)) break;
        fread(&blockSize, 4, 1, sfp);
        if (feof(sfp)) break;

        if (memcmp(magic, "fmt ", 4) == 0 && !bFmt) {
            bFmt = new unsigned char[blockSize];
            sizeFmt = blockSize;
            fread(bFmt, 1, sizeFmt, sfp);
        }
        else if (memcmp(magic, "data", 4) == 0 && !bData) {
            bData = new unsigned char[blockSize];
            sizeData = blockSize;
            fread(bData, 1, sizeData, sfp);
        }
        else {
            fseek(sfp, blockSize, SEEK_CUR);
        }
    }
    fclose(sfp);

    if (!bFmt || !bData) {
        printf("ERROR: Invalid WAV file structure: %s\n", input);
        delete[] bFmt;
        delete[] bData;
        return 1;
    }

    WAVEFORMATEX *swfx = (WAVEFORMATEX*)bFmt;
    if (swfx->wFormatTag != WAVE_FORMAT_SONY_SCX) {
        printf("ERROR: Input file is not ATRAC3 format: %s\n", input);
        delete[] bFmt;
        delete[] bData;
        return 1;
    }

    // Driver is passed as parameter, no need to find it again

    WAVEFORMATEX dwfx;
    memset(&dwfx, 0, sizeof(WAVEFORMATEX));
    dwfx.wFormatTag = WAVE_FORMAT_PCM;
    dwfx.nChannels = swfx->nChannels;
    dwfx.nSamplesPerSec = swfx->nSamplesPerSec;
    dwfx.wBitsPerSample = 16;
    dwfx.nBlockAlign = dwfx.nChannels * (dwfx.wBitsPerSample / 8);
    dwfx.nAvgBytesPerSec = dwfx.nSamplesPerSec * dwfx.nBlockAlign;
    dwfx.cbSize = 0;

    HACMDRIVER at3had;
    if (acmDriverOpen(&at3had, at3hadid, 0)) {
        printf("ERROR: ATRAC3 driver could not be opened\n");
        delete[] bFmt;
        delete[] bData;
        return 1;
    }

    HACMSTREAM stream;
    if(acmStreamOpen(&stream, at3had, swfx, &dwfx, nullptr, 0, 0, ACM_STREAMOPENF_NONREALTIME)) {
        printf("ERROR: ATRAC3 stream initialization failed\n");
        acmDriverClose(at3had, 0);
        delete[] bFmt;
        delete[] bData;
        return 1;
    }

    DWORD dlen;
    if (acmStreamSize(stream, sizeData, &dlen, ACM_STREAMSIZEF_SOURCE)) {
        acmStreamClose(stream, 0);
        acmDriverClose(at3had, 0);
        printf("ERROR: Could not infer stream size\n");
        delete[] bFmt;
        delete[] bData;
        return 1;
    }

    dlen += 0x400;
    unsigned char *dbuf = new unsigned char[dlen];

    ACMSTREAMHEADER header;
    memset(&header, 0, sizeof(header));
    header.cbStruct = sizeof(header);
    header.fdwStatus = 0;
    header.pbSrc = bData;
    header.cbSrcLength = sizeData;
    header.cbSrcLengthUsed = 0;
    header.pbDst = dbuf;
    header.cbDstLength = dlen;
    header.cbDstLengthUsed = 0;

    if (acmStreamPrepareHeader(stream, &header, 0)) {
        acmStreamClose(stream, 0);
        acmDriverClose(at3had, 0);
        printf("ERROR: Could not prepare header\n");
        delete[] bFmt;
        delete[] bData;
        delete[] dbuf;
        return 1;
    }

    if (acmStreamConvert(stream, &header, ACM_STREAMCONVERTF_BLOCKALIGN)) {
        acmStreamUnprepareHeader(stream, &header, 0);
        acmStreamClose(stream, 0);
        acmDriverClose(at3had, 0);
        printf("ERROR: ATRAC3 conversion failed\n");
        delete[] bFmt;
        delete[] bData;
        delete[] dbuf;
        return 1;
    }

    acmStreamUnprepareHeader(stream, &header, 0);
    acmStreamClose(stream, 0);
    acmDriverClose(at3had, 0);

    FILE *dfp = fopen(output, "wb");
    if (!dfp) {
        printf("ERROR: Cannot open output file: %s\n", output);
        delete[] bFmt;
        delete[] bData;
        delete[] dbuf;
        return 1;
    }

    DWORD32 fileSize = 0xC + 8 + sizeof(WAVEFORMATEX) + 8 + header.cbDstLengthUsed;
    fwrite("RIFF", 1, 4, dfp);
    fwrite(&fileSize, 4, 1, dfp);
    fwrite("WAVE", 1, 4, dfp);

    fwrite("fmt ", 1, 4, dfp);
    DWORD32 fmtSize = sizeof(WAVEFORMATEX);
    fwrite(&fmtSize, 4, 1, dfp);
    fwrite(&dwfx, 1, sizeof(WAVEFORMATEX), dfp);

    fwrite("data", 1, 4, dfp);
    DWORD32 dataSize = header.cbDstLengthUsed;
    fwrite(&dataSize, 4, 1, dfp);
    fwrite(dbuf, 1, header.cbDstLengthUsed, dfp);

    fclose(dfp);

    delete[] bFmt;
    delete[] bData;
    delete[] dbuf;

    return 0;
}