// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "utils.h"
#include <windows.h>

bool isEmpty(unsigned char* buf, int buf_size)
{
	if (buf != NULL)
	{
		int i;
		for(i = 0; i < buf_size; i++)
		{
			if (buf[i] != 0) return false;
		}
	}
	return true;
}

int se32(int i)
{
	return ((i & 0xFF000000) >> 24) | ((i & 0xFF0000) >>  8) | ((i & 0xFF00) <<  8) | ((i & 0xFF) << 24);
}

u64 se64(u64 i)
{
	return ((i & 0x00000000000000ff) << 56) | ((i & 0x000000000000ff00) << 40) |
		((i & 0x0000000000ff0000) << 24) | ((i & 0x00000000ff000000) <<  8) |
		((i & 0x000000ff00000000) >>  8) | ((i & 0x0000ff0000000000) >> 24) |
		((i & 0x00ff000000000000) >> 40) | ((i & 0xff00000000000000) >> 56);
}

int get_exe_directory(char* buffer, int buffer_size)
{
	if (GetModuleFileName(NULL, buffer, buffer_size) == 0)
	{
		return -1;
	}
	
	// Find the last backslash in the path, and null-terminate there to remove the executable name
	char* last_backslash = strrchr(buffer, '\\');
	if (last_backslash) {
		*last_backslash = '\0';
	}
	
	return 0;
}