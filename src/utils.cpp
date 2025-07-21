// Copyright (C) 2014       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "utils.h"
#include <windows.h>
#include <shlwapi.h>
#include <direct.h>
#include <stdio.h>

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
	wchar_t wbuffer[_MAX_PATH];
	if (GetModuleFileNameW(NULL, wbuffer, _MAX_PATH) == 0)
	{
		return -1;
	}
	
	// Find the last backslash in the path, and null-terminate there to remove the executable name
	wchar_t* last_backslash = wcsrchr(wbuffer, L'\\');
	if (last_backslash) {
		*last_backslash = L'\0';
	}
	
	// Convert from wide char to multibyte using UTF-8 encoding
	int result = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, buffer_size, NULL, NULL);
	if (result == 0) {
		return -1;
	}
	
	return 0;
}

// Global variable to store the original working directory
static char g_original_working_dir[_MAX_PATH] = {0};

// Function to save the original working directory (called once at startup)
void save_original_working_directory()
{
	wchar_t wbuffer[_MAX_PATH];
	if (GetCurrentDirectoryW(_MAX_PATH, wbuffer) == 0) {
		g_original_working_dir[0] = 0; // Clear on failure
		return;
	}
	
	// Convert from wide char to multibyte using UTF-8 encoding
	int result = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, g_original_working_dir, _MAX_PATH, NULL, NULL);
	if (result == 0) {
		g_original_working_dir[0] = 0; // Clear on failure
		return;
	}
}

// Helper function to build a full path in the original working directory
int build_output_path(const char* filename, char* output_path, int output_path_size)
{
	// Use saved original directory, fallback to current if not set
	const char* target_dir = (g_original_working_dir[0] != 0) ? g_original_working_dir : ".";
	
	// Use PathCombine to properly build the path
	char relative_path[_MAX_PATH];
	if (PathCombineA(relative_path, target_dir, filename) == NULL) {
		return -1; // PathCombine failed
	}
	
	// Convert to full path
	if (_fullpath(output_path, relative_path, output_path_size) == NULL) {
		// If _fullpath fails, try GetFullPathName
		if (GetFullPathName(relative_path, output_path_size, output_path, NULL) == 0) {
			// If both fail, just copy the relative path
			if (strlen(relative_path) >= output_path_size) {
				return -1; // Path too long
			}
			strcpy(output_path, relative_path);
		}
	}
	
	return 0;
}

int utf8_file_exists(const char* filename)
{
	int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
	wchar_t* wfilename = (wchar_t*)malloc(wlen * sizeof(wchar_t));
	if (!wfilename) {
		return -1;
	}
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, wlen);
	
	DWORD attrs = GetFileAttributesW(wfilename);
	free(wfilename);
	
	return (attrs != INVALID_FILE_ATTRIBUTES) ? 0 : -1;
}

FILE* utf8_fopen(const char* filename, const char* mode)
{
	int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
	wchar_t* wfilename = (wchar_t*)malloc(wlen * sizeof(wchar_t));
	if (!wfilename) {
		return NULL;
	}
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, wlen);
	
	int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
	wchar_t* wmode = (wchar_t*)malloc(wmode_len * sizeof(wchar_t));
	if (!wmode) {
		free(wfilename);
		return NULL;
	}
	MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, wmode_len);
	
	FILE* file = _wfopen(wfilename, wmode);
	
	free(wfilename);
	free(wmode);
	
	return file;
}