// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include <string.h>

typedef unsigned long long u64;

unsigned char* strip_utf8(unsigned char *src, int size);
bool isEmpty(unsigned char* buf, int buf_size);
int se32(int i);
u64 se64(u64 i);
int get_exe_directory(char* buffer, int buffer_size);