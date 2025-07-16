#pragma once

// Function to load CUE file from embedded resources
extern char* load_cue_resource(const char* game_id);
extern void free_cue_resource(char* data);

// Function to extract MD5 from prebaked CUE file
extern bool extract_cue_md5(const char* game_id, char* md5_output);
