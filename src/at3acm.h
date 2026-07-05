#pragma once

#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <msacm.h>

// Function to find ATRAC3 driver
void findAt3Driver(LPHACMDRIVERID lpHadid);

// Function to check if ATRAC3 driver is available
bool isAtrac3CodecAvailable();

// Register the ATRAC3 ACM codec bundled in this executable for the current
// process only (no admin rights, registry, or external installer needed).
// Returns true if the codec is available afterwards.
bool registerBundledAtrac3Codec();

// Function to convert ATRAC3 to WAV using ACM with pre-found driver
int convertAt3ToWav(const char* input, const char* output, HACMDRIVERID at3hadid);