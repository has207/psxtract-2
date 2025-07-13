// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "md5_data.h"
#include "gui.h"
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

// Define printf to use GUI-aware version
#define printf gui_printf

// MD5 hash calculation function
bool calculate_md5(const char* filename, char* md5_string)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HANDLE hFile = NULL;
    BYTE rgbFile[1024];
    DWORD cbRead = 0;
    BYTE rgbHash[16];
    DWORD cbHash = 16;
    CHAR rgbDigits[] = "0123456789abcdef";
    
    hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        CloseHandle(hFile);
        return false;
    }
    
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
    {
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return false;
    }
    
    while (ReadFile(hFile, rgbFile, 1024, &cbRead, NULL) && cbRead > 0)
    {
        if (!CryptHashData(hHash, rgbFile, cbRead, 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            CloseHandle(hFile);
            return false;
        }
    }
    
    if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
    {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return false;
    }
    
    // Convert hash to string
    for (DWORD i = 0; i < cbHash; i++)
    {
        md5_string[i * 2] = rgbDigits[rgbHash[i] >> 4];
        md5_string[i * 2 + 1] = rgbDigits[rgbHash[i] & 0xf];
    }
    md5_string[cbHash * 2] = '\0';
    
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);
    
    return true;
}

// Load MD5 entries from embedded map
int load_md5_entries(MD5_ENTRY** entries)
{
    const auto& md5_map = get_md5_map();
    int count = md5_map.size();
    
    // Allocate memory for entries
    *entries = (MD5_ENTRY*)malloc(count * sizeof(MD5_ENTRY));
    if (*entries == NULL) {
        printf("ERROR: Failed to allocate memory for MD5 entries\n");
        return -1;
    }
    
    // Convert map to array
    int i = 0;
    for (const auto& pair : md5_map) {
        strncpy((*entries)[i].serial, pair.first.c_str(), sizeof((*entries)[i].serial) - 1);
        (*entries)[i].serial[sizeof((*entries)[i].serial) - 1] = '\0';
        
        // Clear filename field (not used in embedded data)
        (*entries)[i].filename[0] = '\0';
        
        strncpy((*entries)[i].md5_hash, pair.second.c_str(), sizeof((*entries)[i].md5_hash) - 1);
        (*entries)[i].md5_hash[sizeof((*entries)[i].md5_hash) - 1] = '\0';
        
        i++;
    }
    
    return count;
}

// Find MD5 entry for a given disc serial
MD5_ENTRY* find_md5_entry(MD5_ENTRY* entries, int count, const char* serial)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].serial, serial) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

// Verify data track MD5
bool verify_data_track_md5(const char* data_track_file, const char* disc_serial, MD5_ENTRY* md5_entries, int md5_count)
{
    if (md5_entries == NULL || md5_count <= 0) {
        printf("No MD5 entries available for verification\n");
        return false;
    }
    
    // Try to find the expected MD5 for this disc serial
    MD5_ENTRY* expected_entry = find_md5_entry(md5_entries, md5_count, disc_serial);
    
    // If not found, try converting format (dash <-> underscore) and search again
    if (expected_entry == NULL) {
        char converted_serial[0x10];
        strcpy(converted_serial, disc_serial);
        for (int i = 0; converted_serial[i]; i++) {
            if (converted_serial[i] == '_') {
                converted_serial[i] = '-';
            } else if (converted_serial[i] == '-') {
                converted_serial[i] = '_';
            }
        }
        expected_entry = find_md5_entry(md5_entries, md5_count, converted_serial);
    }
    
    if (expected_entry == NULL) {
        printf("No MD5 reference found for disc serial: %s\n", disc_serial);
        return false;
    }
    
    // Calculate actual MD5 of the data track
    char actual_md5[33];
    if (!calculate_md5(data_track_file, actual_md5)) {
        printf("Failed to calculate MD5 for data track: %s\n", data_track_file);
        return false;
    }
    
    // Compare MD5 hashes
    bool match = (strcmp(actual_md5, expected_entry->md5_hash) == 0);
    
    printf("MD5 verification for %s:\n", disc_serial);
    printf("  Expected: %s\n", expected_entry->md5_hash);
    printf("  Actual:   %s\n", actual_md5);
    printf("  Result:   %s\n", match ? "PASS" : "FAIL");
    
    return match;
}
