#pragma once

// MD5 verification using prebaked CUE files only
bool calculate_md5(const char* filename, char* md5_string);
bool verify_data_track_md5_cue(const char* data_track_file, const char* disc_serial);