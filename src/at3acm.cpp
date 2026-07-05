#include "at3acm.h"
#include "gui.h"
#include <stdio.h>
#include <string.h>


#define WAVE_FORMAT_SONY_SCX 0x0270

// Driver id of the codec we registered for this process via acmDriverAdd (see
// registerBundledAtrac3Codec). Such a driver reports an EMPTY szShortName, so it
// can't be located again by the name scan below - we must remember its id here.
static HACMDRIVERID g_bundledAt3Driver = nullptr;

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
    // Prefer the codec we registered ourselves this session; fall back to a
    // system-installed ATRAC3 driver (which does report a szShortName).
    if (g_bundledAt3Driver) {
        *lpHadid = g_bundledAt3Driver;
        return;
    }
    *lpHadid = nullptr;
    acmDriverEnum(findAt3Driver_cb, (DWORD_PTR)lpHadid, 0);
}

bool isAtrac3CodecAvailable() {
    HACMDRIVERID at3hadid = nullptr;
    findAt3Driver(&at3hadid);
    return (at3hadid != nullptr);
}

// Resource id of the embedded atrac3.acm (see atrac3_resources.rc).
#define IDR_ATRAC3_ACM 12001

bool registerBundledAtrac3Codec() {
    // If the codec is already present (e.g. installed system-wide), do nothing.
    if (isAtrac3CodecAvailable())
        return true;

    // Locate the ATRAC3 codec DLL embedded in our own executable.
    HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(IDR_ATRAC3_ACM), RT_RCDATA);
    if (!hRes)
        return false;
    HGLOBAL hGlobal = LoadResource(nullptr, hRes);
    if (!hGlobal)
        return false;
    DWORD dwSize = SizeofResource(nullptr, hRes);
    LPVOID pData = LockResource(hGlobal);
    if (!pData || dwSize == 0)
        return false;

    // acmDriverAdd(ACM_DRIVERADDF_FUNCTION) needs a real, loadable module, so the
    // codec has to exist on disk. Use a stable name in %TEMP% so repeated runs
    // (the GUI launches a fresh process per extraction) reuse one file instead of
    // littering the temp folder.
    char tempPath[MAX_PATH];
    char acmPath[MAX_PATH];
    if (GetTempPath(sizeof(tempPath), tempPath) == 0)
        return false;
    _snprintf(acmPath, sizeof(acmPath), "%spsxtract_atrac3.acm", tempPath);

    // Best-effort write. If another psxtract process already has this file loaded
    // it will be locked, but the on-disk copy is identical, so we just load it.
    HANDLE hFile = CreateFile(acmPath, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, pData, dwSize, &written, nullptr);
        CloseHandle(hFile);
    }

    // Load the codec and hand its DriverProc entry point to msacm. This registers
    // the driver for this process only - no admin rights, System32 copy, or
    // registry writes, which is exactly what the old INF-based installer needed
    // (and frequently failed to do on 64-bit Windows).
    HMODULE hAcm = LoadLibrary(acmPath);
    if (!hAcm)
        return false;
    FARPROC driverProc = GetProcAddress(hAcm, "DriverProc");
    if (!driverProc) {
        FreeLibrary(hAcm);
        return false;
    }

    HACMDRIVERID hadid = nullptr;
    MMRESULT mr = acmDriverAdd(&hadid, hAcm, (LPARAM)driverProc, 0,
                               ACM_DRIVERADDF_FUNCTION);
    if (mr != MMSYSERR_NOERROR || !hadid) {
        FreeLibrary(hAcm);
        return false;
    }

    // A FUNCTION-added driver has no szShortName, so remember its id for
    // findAt3Driver(). The module stays mapped for the life of the process; the
    // OS releases it (and unlocks the temp file) at exit.
    g_bundledAt3Driver = hadid;
    return isAtrac3CodecAvailable();
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