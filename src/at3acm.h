#pragma once

#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <msacm.h>

// Function to find ATRAC3 driver
void findAt3Driver(LPHACMDRIVERID lpHadid);

// Function to convert ATRAC3 to WAV using ACM
int convertAt3ToWav(const char* input, const char* output);