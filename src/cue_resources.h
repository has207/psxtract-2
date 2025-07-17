#pragma once

// Structure to hold CUE candidate information
typedef struct {
    char game_id[32];
    char title[256];
} CueCandidate;

// Function to load CUE file from embedded resources
extern char* load_cue_resource(const char* game_id);
extern void free_cue_resource(char* data);

// Function to extract MD5 from prebaked CUE file
extern bool extract_cue_md5(const char* game_id, char* md5_output);

// Function to find all CUE candidates for a base serial
extern int find_cue_candidates(const char* base_serial, CueCandidate* candidates, int max_candidates);

// Function to extract title from CUE file
extern bool extract_cue_title(const char* cue_data, char* title_output);

// Enhanced load function with candidate selection
extern char* load_cue_resource_with_selection(const char* game_id);

// Function to select CUE variant and update the disc serial in-place
extern bool select_cue_variant_and_update_serial(char* disc_serial);
