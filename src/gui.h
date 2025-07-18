#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

// GUI function declarations
int showGUI();
void logToGUI(const char* message);
void enableExtractButton(bool enabled);
void showAtrac3CodecWarning();
bool extractAndRunAtrac3Installer();

// Printf redirection
extern void setGUIMode(bool enabled);
extern bool isGUIMode();

// Function for important messages that should go to GUI log
extern int gui_log_printf(const char* format, ...);

// Printf replacement function
extern int gui_printf_impl(const char* format, ...);

// Redefine printf to use our implementation
#define printf gui_printf_impl

// GUI-aware prompt function
extern bool gui_prompt(const char* message, const char* title);

// GUI-aware selection function for multiple options
extern int gui_select_option(const char* title, const char* message, const char* options[], int option_count);

// Internal function for creating selection dialog
extern int gui_create_selection_dialog(const char* title, const char* message, const char* options[], int option_count);