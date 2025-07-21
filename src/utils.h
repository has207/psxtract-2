// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include <string.h>
#include <stdio.h>

typedef unsigned long long u64;

unsigned char* strip_utf8(unsigned char *src, int size);
bool isEmpty(unsigned char* buf, int buf_size);
int se32(int i);
u64 se64(u64 i);
int get_exe_directory(char* buffer, int buffer_size);
void save_original_working_directory();
int build_output_path(const char* filename, char* output_path, int output_path_size);
int utf8_file_exists(const char* filename);
FILE* utf8_fopen(const char* filename, const char* mode);

// Automatically replace all fopen calls with Unicode-aware version
#define fopen utf8_fopen