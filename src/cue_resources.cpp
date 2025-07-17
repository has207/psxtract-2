#include "cue_resources.h"
#include "gui.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

bool extract_cue_title(const char* cue_data, char* title_output) {
    if (!cue_data || !title_output) {
        return false;
    }
    
    // Parse lines to find FILE line with title
    const char* current_pos = cue_data;
    const char* line_end;
    char line[512];
    
    while ((line_end = strchr(current_pos, '\n')) != NULL) {
        size_t line_len = line_end - current_pos;
        if (line_len < sizeof(line)) {
            strncpy(line, current_pos, line_len);
            line[line_len] = '\0';
            
            // Skip whitespace
            char* trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            
            // Look for FILE line: FILE "title.bin" BINARY
            if (strncmp(trimmed, "FILE ", 5) == 0) {
                char* quote_start = strchr(trimmed + 5, '"');
                if (quote_start) {
                    quote_start++; // Skip opening quote
                    char* quote_end = strchr(quote_start, '"');
                    if (quote_end) {
                        size_t title_len = quote_end - quote_start;
                        if (title_len > 0 && title_len < 255) {
                            strncpy(title_output, quote_start, title_len);
                            title_output[title_len] = '\0';
                            
                            // Remove .bin extension if present
                            char* bin_ext = strstr(title_output, ".bin");
                            if (bin_ext && bin_ext[4] == '\0') {
                                *bin_ext = '\0';
                            }
                            
                            return true;
                        }
                    }
                }
            }
        }
        
        // Move to next line
        current_pos = line_end + 1;
    }
    
    return false;
}

int find_cue_candidates(const char* base_serial, CueCandidate* candidates, int max_candidates) {
    if (!base_serial || !candidates || max_candidates <= 0) {
        return 0;
    }
    
    int found_count = 0;
    
    // First, check for exact match
    if (find_resource_id(base_serial) != 0) {
        strcpy(candidates[found_count].game_id, base_serial);
        
        // Load CUE data to extract title
        char* cue_data = load_cue_resource(base_serial);
        if (cue_data) {
            if (!extract_cue_title(cue_data, candidates[found_count].title)) {
                strcpy(candidates[found_count].title, "Unknown Title");
            }
            free_cue_resource(cue_data);
        } else {
            strcpy(candidates[found_count].title, "Unknown Title");
        }
        
        found_count++;
    }
    
    // Then look for candidates with -N suffix
    for (int i = 1; i <= 10 && found_count < max_candidates; i++) {
        char candidate_id[64];
        snprintf(candidate_id, sizeof(candidate_id), "%s-%d", base_serial, i);
        
        if (find_resource_id(candidate_id) != 0) {
            strcpy(candidates[found_count].game_id, candidate_id);
            
            // Load CUE data to extract title
            char* cue_data = load_cue_resource(candidate_id);
            if (cue_data) {
                if (!extract_cue_title(cue_data, candidates[found_count].title)) {
                    strcpy(candidates[found_count].title, "Unknown Title");
                }
                free_cue_resource(cue_data);
            } else {
                strcpy(candidates[found_count].title, "Unknown Title");
            }
            
            found_count++;
        }
    }
    
    return found_count;
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

bool select_cue_variant_and_update_serial(char* disc_serial) {
    if (!disc_serial) {
        return false;
    }
    
    // First try exact match
    if (find_resource_id(disc_serial) != 0) {
        // Exact match found, no need to update
        return true;
    }
    
    // If no exact match, look for candidates
    CueCandidate candidates[12];
    int candidate_count = find_cue_candidates(disc_serial, candidates, 12);
    
    if (candidate_count == 0) {
        return false; // No candidates found
    }
    
    if (candidate_count == 1) {
        // Only one candidate, update serial to use it
        strcpy(disc_serial, candidates[0].game_id);
        return true;
    }
    
    // Multiple candidates found - present user with choices
    char message[256];
    snprintf(message, sizeof(message), "Multiple CUE file candidates found for disc serial: %s\nPlease select which version to use:", disc_serial);
    
    // Prepare options array for GUI (only show titles, not serials)
    const char* options[12]; // Max candidates we support
    for (int i = 0; i < candidate_count; i++) {
        // Create a static buffer for each option (this is a limitation, but works for our use case)
        static char option_buffers[12][512];
        snprintf(option_buffers[i], sizeof(option_buffers[i]), "%s", candidates[i].title);
        options[i] = option_buffers[i];
    }
    
    // Use GUI-aware selection
    int choice = gui_select_option("CUE File Selection", message, options, candidate_count);
    
    printf("Selected: %s - %s\n\n", candidates[choice].game_id, candidates[choice].title);
    
    // Update the disc serial to the selected variant
    strcpy(disc_serial, candidates[choice].game_id);
    
    return true;
}

char* load_cue_resource_with_selection(const char* game_id) {
    // This function is now simplified - just load the CUE resource
    // The serial selection should have happened earlier via select_cue_variant_and_update_serial
    return load_cue_resource(game_id);
}