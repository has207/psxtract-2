#pragma once

#include <windows.h>

// GUI function declarations
int showGUI();
void logToGUI(const char* message);
void enableExtractButton(bool enabled);

// Printf redirection
extern void setGUIMode(bool enabled);
extern bool isGUIMode();
extern int gui_printf(const char* format, ...);

// GUI-aware prompt function
extern bool gui_prompt(const char* message, const char* title);