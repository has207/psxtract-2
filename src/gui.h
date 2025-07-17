#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

// GUI function declarations
int showGUI();
void logToGUI(const char* message);
void enableExtractButton(bool enabled);

// Printf redirection
extern void setGUIMode(bool enabled);
extern bool isGUIMode();
extern int gui_printf(const char* format, ...);
// Forward declaration
extern bool isVerboseMode();

// Inline verbose_printf function
inline int verbose_printf(const char* format, ...) {
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    fprintf(stdout, "%s", buffer);
    fflush(stdout);
    
    return result;
}

// GUI-aware prompt function
extern bool gui_prompt(const char* message, const char* title);

// GUI-aware selection function for multiple options
extern int gui_select_option(const char* title, const char* message, const char* options[], int option_count);

// Internal function for creating selection dialog
extern int gui_create_selection_dialog(const char* title, const char* message, const char* options[], int option_count);

// Redefine printf to use verbose output (after all declarations)
#ifndef GUI_CPP_INTERNAL
#define printf verbose_printf
#endif