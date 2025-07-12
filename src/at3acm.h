#pragma once

#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <msacm.h>

// Function to find ATRAC3 driver
void findAt3Driver(LPHACMDRIVERID lpHadid);

// Function to convert ATRAC3 to WAV using ACM with pre-found driver
int convertAt3ToWav(const char* input, const char* output, HACMDRIVERID at3hadid);