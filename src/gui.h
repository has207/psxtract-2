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

// GUI-aware selection function for multiple options
extern int gui_select_option(const char* title, const char* message, const char* options[], int option_count);

// Internal function for creating selection dialog
extern int gui_create_selection_dialog(const char* title, const char* message, const char* options[], int option_count);