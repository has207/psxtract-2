#include "cue_resources.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>

struct CueResourceEntry {
    const char* game_id;
    int resource_id;
};

// Generated lookup table - included from separate file
#include "cue_lookup_table.autogen"

static int find_resource_id(const char* game_id) {
    if (!game_id) return 0;
    
    for (int i = 0; cue_lookup[i].game_id != NULL; i++) {
        if (strcmp(cue_lookup[i].game_id, game_id) == 0) {
            return cue_lookup[i].resource_id;
        }
    }
    
    return 0;  // Not found
}

char* load_cue_resource(const char* game_id) {
    int resource_id = find_resource_id(game_id);
    if (resource_id == 0) {
        return NULL;  // Game ID not found
    }

    // Load resource from executable
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resource_id), RT_RCDATA);
    if (!hRes) return NULL;

    DWORD size = SizeofResource(NULL, hRes);
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return NULL;

    void* pData = LockResource(hData);
    if (!pData) return NULL;

    // Copy to allocated buffer with null terminator
    char* result = (char*)malloc(size + 1);
    if (result) {
        memcpy(result, pData, size);
        result[size] = '\0';
    }

    return result;
}

void free_cue_resource(char* data) {
    if (data) {
        free(data);
    }
}

bool extract_cue_md5(const char* game_id, char* md5_output) {
    if (!game_id || !md5_output) {
        return false;
    }
    
    // Load CUE file from embedded resources
    char* cue_data = load_cue_resource(game_id);
    if (cue_data == NULL) {
        return false;
    }
    
    // Parse lines to find REM MD5 entry
    char* current_pos = cue_data;
    char* line_end;
    char line[512];
    
    while ((line_end = strchr(current_pos, '\n')) != NULL) {
        size_t line_len = line_end - current_pos;
        if (line_len < sizeof(line)) {
            strncpy(line, current_pos, line_len);
            line[line_len] = '\0';
            
            // Skip whitespace
            char* trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            
            // Look for REM MD5 line
            if (strncmp(trimmed, "REM MD5 ", 8) == 0) {
                char* md5_start = trimmed + 8; // Skip "REM MD5 "
                
                // Extract 32-character MD5 hash
                if (strlen(md5_start) >= 32) {
                    strncpy(md5_output, md5_start, 32);
                    md5_output[32] = '\0';
                    
                    // Validate it's a valid hex string
                    for (int i = 0; i < 32; i++) {
                        char c = md5_output[i];
                        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                            free_cue_resource(cue_data);
                            return false;
                        }
                    }
                    
                    free_cue_resource(cue_data);
                    return true;
                }
            }
        }
        
        // Move to next line
        current_pos = line_end + 1;
    }
    
    free_cue_resource(cue_data);
    return false;
}