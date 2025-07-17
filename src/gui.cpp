#include "gui.h"
#include "utils.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// GUI constants
#define ID_FILE_BUTTON      1001
#define ID_EXTRACT_BUTTON   1002
#define ID_LOG_EDIT         1003
#define ID_FILE_EDIT        1004
#define ID_CLEANUP_CHECK    1005
#define ID_OUTPUT_BUTTON    1006
#define ID_OUTPUT_EDIT      1007
#define ID_CANCEL_BUTTON    1008

// Custom message for thread-safe logging
#define WM_APPEND_LOG       (WM_USER + 1)
#define WM_EXTRACTION_DONE  (WM_USER + 2)
#define WM_UPDATE_PROGRESS  (WM_USER + 3)

// Global variables
static HWND g_hMainWnd = NULL;
static HWND g_hLogEdit = NULL;
static HWND g_hFileEdit = NULL;
static HWND g_hExtractButton = NULL;
static HWND g_hCancelButton = NULL;
static HWND g_hCleanupCheck = NULL;
static HWND g_hOutputEdit = NULL;
static char g_selectedFiles[32768] = "";  // Buffer for multiple file paths
static int g_fileCount = 0;
static char g_outputFolder[MAX_PATH];

// Thread management
static HANDLE g_hExtractionThread = NULL;

// Progress dialog
static HWND g_hProgressDialog = NULL;
static HWND g_hProgressText = NULL;
static HWND g_hProgressFile = NULL;
static HWND g_hProgressCancel = NULL;

// Forward declarations
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
UINT_PTR CALLBACK OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);
void onFileSelect();
void onOutputSelect();
void onExtract();
void onCancel();
void appendToLog(const char* text);
LRESULT CALLBACK ProgressDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void showProgressDialog();
void hideProgressDialog();
void updateProgress(int current, int total, const char* filename);

// Global variable to track if we're in GUI mode
static bool g_guiMode = false;

void setGUIMode(bool enabled) {
    g_guiMode = enabled;
}

bool isGUIMode() {
    return g_guiMode;
}

// Hook procedure to center the file dialog
UINT_PTR CALLBACK OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
    if (uiMsg == WM_INITDIALOG) {
        // Get the dialog's parent window (the file dialog)
        HWND hParent = GetParent(hdlg);
        if (!hParent) hParent = hdlg;
        
        // Get main window position and size
        RECT mainRect;
        GetWindowRect(g_hMainWnd, &mainRect);
        int mainX = mainRect.left;
        int mainY = mainRect.top;
        int mainWidth = mainRect.right - mainRect.left;
        int mainHeight = mainRect.bottom - mainRect.top;
        
        // Get dialog size
        RECT dialogRect;
        GetWindowRect(hParent, &dialogRect);
        int dialogWidth = dialogRect.right - dialogRect.left;
        int dialogHeight = dialogRect.bottom - dialogRect.top;
        
        // Calculate centered position
        int newX = mainX + (mainWidth - dialogWidth) / 2;
        int newY = mainY + (mainHeight - dialogHeight) / 2;
        
        // Move the dialog to center
        SetWindowPos(hParent, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    return 0;
}

// Custom printf replacement that logs to GUI
int gui_printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (g_guiMode && g_hLogEdit) {
        appendToLog(buffer);
    } else {
        // Fall back to console output  
        fprintf(stdout, "%s", buffer);
        fflush(stdout);
    }
    
    return result;
}

// Forward declaration of the main extraction function
extern int psxtract_main(const char* pbp_file, const char* document_file, const char* keys_file, bool cleanup, const char* output_dir);

// Thread function for extraction
DWORD WINAPI extractionThread(LPVOID lpParam) {
    char logMsg[512];
    
    // Show progress dialog for batch operations (more than 1 file)
    bool showProgressDlg = (g_fileCount > 1);
    if (showProgressDlg) {
        PostMessage(g_hMainWnd, WM_UPDATE_PROGRESS, 0, (LPARAM)_strdup("showDialog"));
        Sleep(100); // Brief delay to let dialog appear
    } else {
        sprintf(logMsg, "Starting extraction process...\n");
        appendToLog(logMsg);
    }
    
    // Check if cleanup is requested
    bool cleanup = (BST_CHECKED == SendMessage(g_hCleanupCheck, BM_GETCHECK, 0, 0));
    if (!showProgressDlg && cleanup) {
        appendToLog("Cleanup mode enabled\n");
    }
    
    int overallResult = 0;
    int successCount = 0;
    int failureCount = 0;
    int skippedCount = 0;
    
    // Parse and process each file
    char* currentPos = g_selectedFiles;
    
    if (g_fileCount == 1) {
        // Single file - g_selectedFiles contains the full path
        if (!showProgressDlg) {
            sprintf(logMsg, "\n=== Processing file 1/1: %s ===\n", strrchr(currentPos, '\\') ? strrchr(currentPos, '\\') + 1 : currentPos);
            appendToLog(logMsg);
        }
        
        // Run extraction in separate process to avoid memory accumulation
        char cmdLine[2048];
        char cleanupFlag[8] = "";
        if (cleanup) {
            strcpy(cleanupFlag, "-c ");
        }
        
        // Get current executable path
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(NULL, exePath, sizeof(exePath)) == 0) {
            strcpy(exePath, "psxtract.exe"); // fallback
        }
        
        sprintf(cmdLine, "\"%s\" %s--gui \"%s\"", exePath, cleanupFlag, currentPos);
        
        // Change to output directory for this extraction
        char originalDir[MAX_PATH];
        _getcwd(originalDir, sizeof(originalDir));
        _chdir(g_outputFolder);
        
        // Create pipes for stdout/stderr capture
        HANDLE hStdOutRead, hStdOutWrite;
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;
        
        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
            failureCount++;
            overallResult = -1;
            sprintf(logMsg, "Failed to create output pipe.\n");
            appendToLog(logMsg);
            _chdir(originalDir);
            return 0;
        }
        
        // Ensure the read handle is not inherited
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdOutWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;
        
        if (CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            // Close write end of pipe in parent process
            CloseHandle(hStdOutWrite);
            
            // Read output in real-time
            char buffer[1024];
            DWORD bytesRead;
            DWORD exitCode = STILL_ACTIVE;
            
            while (exitCode == STILL_ACTIVE) {
                // Check if process is still running
                GetExitCodeProcess(pi.hProcess, &exitCode);
                
                // Read any available output
                while (PeekNamedPipe(hStdOutRead, NULL, 0, NULL, &bytesRead, NULL) && bytesRead > 0) {
                    if (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        appendToLog(buffer);
                    }
                }
                
                // Small delay to prevent excessive CPU usage
                Sleep(50);
            }
            
            // Read any remaining output
            while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                appendToLog(buffer);
            }
            
            CloseHandle(hStdOutRead);
            
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            if (exitCode == 0) {
                successCount++;
                sprintf(logMsg, "File completed successfully.\n");
                appendToLog(logMsg);
            } else if (exitCode == (DWORD)-2 || exitCode == 4294967294U) { // -2 as unsigned (user cancellation)
                skippedCount++;
                sprintf(logMsg, "File extraction cancelled by user.\n");
                appendToLog(logMsg);
            } else {
                failureCount++;
                overallResult = exitCode;
                sprintf(logMsg, "File failed with exit code %d.\n", (int)exitCode);
                appendToLog(logMsg);
            }
        } else {
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdOutRead);
            failureCount++;
            overallResult = -1;
            sprintf(logMsg, "Failed to start extraction process.\n");
            appendToLog(logMsg);
        }
        
        // Restore original directory
        _chdir(originalDir);
    } else {
        // Multiple files - first part is directory, then individual filenames
        char directory[MAX_PATH];
        strcpy(directory, currentPos);
        
        if (!showProgressDlg) {
            sprintf(logMsg, "Base directory: %s\n", directory);
            appendToLog(logMsg);
        }
        
        // Move past directory name
        currentPos += strlen(currentPos) + 1;
        
        int fileIndex = 1;
        while (*currentPos != '\0') {
            // Build full path
            char fullPath[MAX_PATH];
            sprintf(fullPath, "%s\\%s", directory, currentPos);
            
            // Update progress dialog or log
            if (showProgressDlg) {
                sprintf(logMsg, "BATCH_UPDATE_PROGRESS:%d:%d:%s", fileIndex, g_fileCount, currentPos);
                PostMessage(g_hMainWnd, WM_UPDATE_PROGRESS, 0, (LPARAM)_strdup(logMsg));
                Sleep(50); // Brief delay to let UI update
            } else {
                sprintf(logMsg, "\n=== Processing file %d/%d: %s ===\n", fileIndex, g_fileCount, currentPos);
                appendToLog(logMsg);
            }
            
            // Run extraction in separate process to avoid memory accumulation
            char cmdLine[2048];
            char cleanupFlag[8] = "";
            if (cleanup) {
                strcpy(cleanupFlag, "-c ");
            }
            
            // Get current executable path
            char exePath[MAX_PATH];
            if (GetModuleFileNameA(NULL, exePath, sizeof(exePath)) == 0) {
                strcpy(exePath, "psxtract.exe"); // fallback
            }
            
            sprintf(cmdLine, "\"%s\" %s--gui \"%s\"", exePath, cleanupFlag, fullPath);
            
            // Change to output directory for this extraction
            char originalDir[MAX_PATH];
            _getcwd(originalDir, sizeof(originalDir));
            _chdir(g_outputFolder);
            
            // Create pipes for stdout/stderr capture
            HANDLE hStdOutRead, hStdOutWrite;
            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
            saAttr.bInheritHandle = TRUE;
            saAttr.lpSecurityDescriptor = NULL;
            
            if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
                failureCount++;
                overallResult = -1;
                sprintf(logMsg, "Failed to create output pipe.\n");
                appendToLog(logMsg);
                continue;
            }
            
            // Ensure the read handle is not inherited
            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
            
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            ZeroMemory(&pi, sizeof(pi));
            si.cb = sizeof(si);
            si.hStdOutput = hStdOutWrite;
            si.hStdError = hStdOutWrite;
            si.dwFlags |= STARTF_USESTDHANDLES;
            
            if (CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
                // Close write end of pipe in parent process
                CloseHandle(hStdOutWrite);
                
                // Read output in real-time
                char buffer[1024];
                DWORD bytesRead;
                DWORD exitCode = STILL_ACTIVE;
                
                while (exitCode == STILL_ACTIVE) {
                    // Check if process is still running
                    GetExitCodeProcess(pi.hProcess, &exitCode);
                    
                    // Read any available output
                    while (PeekNamedPipe(hStdOutRead, NULL, 0, NULL, &bytesRead, NULL) && bytesRead > 0) {
                        if (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                            buffer[bytesRead] = '\0';
                            appendToLog(buffer);
                        }
                    }
                    
                    // Small delay to prevent excessive CPU usage
                    Sleep(50);
                }
                
                // Read any remaining output
                while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    appendToLog(buffer);
                }
                
                CloseHandle(hStdOutRead);
                
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                
                if (exitCode == 0) {
                    successCount++;
                    sprintf(logMsg, "File completed successfully.\n");
                    appendToLog(logMsg);
                } else {
                    failureCount++;
                    overallResult = exitCode;
                    sprintf(logMsg, "File failed with exit code %d.\n", (int)exitCode);
                    appendToLog(logMsg);
                }
            } else {
                CloseHandle(hStdOutWrite);
                CloseHandle(hStdOutRead);
                failureCount++;
                overallResult = -1;
                sprintf(logMsg, "Failed to start extraction process.\n");
                appendToLog(logMsg);
            }
            
            // Restore original directory
            _chdir(originalDir);
            
            // Move to next filename
            currentPos += strlen(currentPos) + 1;
            fileIndex++;
        }
    }
    
    // Log batch completion summary
    if (showProgressDlg) {
        // Update progress dialog with completion summary
        sprintf(logMsg, "Complete! %d successful, %d skipped, %d failed", successCount, skippedCount, failureCount);
        if (g_hProgressText) {
            SetWindowText(g_hProgressText, logMsg);
        }
        if (g_hProgressFile) {
            SetWindowText(g_hProgressFile, "Processing finished.");
        }
        Sleep(2000); // Show summary for 2 seconds
        
        // Hide dialog
        PostMessage(g_hMainWnd, WM_UPDATE_PROGRESS, 0, (LPARAM)_strdup("hideDialog"));
    }
    
    sprintf(logMsg, "\n=== Batch Processing Complete ===\n");
    appendToLog(logMsg);
    sprintf(logMsg, "Total files: %d\n", g_fileCount);
    appendToLog(logMsg);
    sprintf(logMsg, "Successful: %d\n", successCount);
    appendToLog(logMsg);
    sprintf(logMsg, "Skipped: %d\n", skippedCount);
    appendToLog(logMsg);
    sprintf(logMsg, "Failed: %d\n", failureCount);
    appendToLog(logMsg);
    
    // Post completion message to UI thread
    PostMessage(g_hMainWnd, WM_EXTRACTION_DONE, (WPARAM)overallResult, 0);
    
    return 0;
}

void onFileSelect() {
    OPENFILENAME ofn;
    
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(g_selectedFiles, sizeof(g_selectedFiles));
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFile = g_selectedFiles;
    ofn.nMaxFile = sizeof(g_selectedFiles);
    ofn.lpstrFilter = "PBP Files\0*.PBP\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ALLOWMULTISELECT;
    ofn.lpfnHook = OFNHookProc;
    
    if (GetOpenFileName(&ofn)) {
        // Parse multiple file selection
        g_fileCount = 0;
        char displayText[1024] = "";
        
        // Check if multiple files were selected
        char* fileName = g_selectedFiles + strlen(g_selectedFiles) + 1;
        if (*fileName == '\0') {
            // Single file selected
            g_fileCount = 1;
            sprintf(displayText, "1 file selected: %s", strrchr(g_selectedFiles, '\\') ? strrchr(g_selectedFiles, '\\') + 1 : g_selectedFiles);
        } else {
            // Multiple files selected
            char directory[MAX_PATH];
            strcpy(directory, g_selectedFiles);
            
            while (*fileName != '\0') {
                g_fileCount++;
                fileName += strlen(fileName) + 1;
            }
            
            sprintf(displayText, "%d files selected in: %s", g_fileCount, directory);
        }
        
        SetWindowText(g_hFileEdit, displayText);
        EnableWindow(g_hExtractButton, TRUE);
        
        // Update Extract button text to show file count
        if (g_fileCount > 1) {
            char buttonText[32];
            sprintf(buttonText, "Extract (%d)", g_fileCount);
            SetWindowText(g_hExtractButton, buttonText);
        } else {
            SetWindowText(g_hExtractButton, "Extract");
        }
    }
}

void onOutputSelect() {
    BROWSEINFO bi;
    char szDir[MAX_PATH];
    ITEMIDLIST* pidl;
    
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = g_hMainWnd;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = szDir;
    bi.lpszTitle = "Select Output Folder:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = NULL;
    bi.lParam = 0;
    
    pidl = SHBrowseForFolder(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDList(pidl, szDir)) {
            strcpy(g_outputFolder, szDir);
            SetWindowText(g_hOutputEdit, g_outputFolder);
        }
        CoTaskMemFree(pidl);
    }
}

void onExtract() {
    if (g_fileCount == 0) {
        MessageBox(g_hMainWnd, "Please select one or more PBP files first.", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Clear the log
    SetWindowText(g_hLogEdit, "");
    
    // Update button states
    EnableWindow(g_hExtractButton, FALSE);
    EnableWindow(g_hCancelButton, TRUE);
    
    // Start extraction in a separate thread
    g_hExtractionThread = CreateThread(NULL, 0, extractionThread, NULL, 0, NULL);
}

void onCancel() {
    if (g_hExtractionThread) {
        // Immediately terminate the extraction thread
        TerminateThread(g_hExtractionThread, 1);
        
        appendToLog("Extraction cancelled.\n");
        
        // Manually trigger completion handling
        PostMessage(g_hMainWnd, WM_EXTRACTION_DONE, (WPARAM)-2, 0);
    }
}

void appendToLog(const char* text) {
    if (!g_hLogEdit || !g_hMainWnd) return;
    
    // For thread safety, allocate memory for the text and post a message
    // The UI thread will handle the actual log update
    char* logText = (char*)malloc(strlen(text) + 1);
    if (logText) {
        strcpy(logText, text);
        PostMessage(g_hMainWnd, WM_APPEND_LOG, 0, (LPARAM)logText);
    }
}

void appendToLogDirect(const char* text) {
    if (!g_hLogEdit) return;
    
    int length = GetWindowTextLength(g_hLogEdit);
    SendMessage(g_hLogEdit, EM_SETSEL, length, length);
    SendMessage(g_hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
    
    // Auto-scroll to bottom
    SendMessage(g_hLogEdit, EM_SCROLL, SB_BOTTOM, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        {
            // Initialize output folder with current directory
            if (_getcwd(g_outputFolder, sizeof(g_outputFolder)) == NULL) {
                strcpy(g_outputFolder, ".");
            }
            
            // Create controls
            CreateWindow("STATIC", "PBP File:", WS_VISIBLE | WS_CHILD,
                        10, 20, 80, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
            
            g_hFileEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
                        100, 20, 350, 25, hWnd, (HMENU)ID_FILE_EDIT, GetModuleHandle(NULL), NULL);
            
            CreateWindow("BUTTON", "Browse...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        460, 20, 80, 25, hWnd, (HMENU)ID_FILE_BUTTON, GetModuleHandle(NULL), NULL);
            
            CreateWindow("STATIC", "Output Folder:", WS_VISIBLE | WS_CHILD,
                        10, 55, 100, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
            
            g_hOutputEdit = CreateWindow("EDIT", g_outputFolder, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
                        120, 55, 330, 25, hWnd, (HMENU)ID_OUTPUT_EDIT, GetModuleHandle(NULL), NULL);
            
            CreateWindow("BUTTON", "Browse...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        460, 55, 80, 25, hWnd, (HMENU)ID_OUTPUT_BUTTON, GetModuleHandle(NULL), NULL);
            
            g_hCleanupCheck = CreateWindow("BUTTON", "Clean up TEMP files after extraction", 
                        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                        10, 90, 300, 20, hWnd, (HMENU)ID_CLEANUP_CHECK, GetModuleHandle(NULL), NULL);
            
            // Default cleanup to checked in GUI mode
            SendMessage(g_hCleanupCheck, BM_SETCHECK, BST_CHECKED, 0);
            
            g_hExtractButton = CreateWindow("BUTTON", "Extract", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        10, 120, 100, 30, hWnd, (HMENU)ID_EXTRACT_BUTTON, GetModuleHandle(NULL), NULL);
            
            g_hCancelButton = CreateWindow("BUTTON", "Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        120, 120, 100, 30, hWnd, (HMENU)ID_CANCEL_BUTTON, GetModuleHandle(NULL), NULL);
            
            EnableWindow(g_hExtractButton, FALSE);
            EnableWindow(g_hCancelButton, FALSE);
            
            CreateWindow("STATIC", "Log:", WS_VISIBLE | WS_CHILD,
                        10, 160, 40, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
            
            g_hLogEdit = CreateWindow("EDIT", "", 
                        WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                        10, 185, 560, 300, hWnd, (HMENU)ID_LOG_EDIT, GetModuleHandle(NULL), NULL);
            
            // Set default font
            HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            
            // Set font for all child windows
            SendMessage(g_hFileEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hOutputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hExtractButton, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hCancelButton, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hCleanupCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(g_hLogEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        break;
        
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_FILE_BUTTON:
            onFileSelect();
            break;
        case ID_OUTPUT_BUTTON:
            onOutputSelect();
            break;
        case ID_EXTRACT_BUTTON:
            onExtract();
            break;
        case ID_CANCEL_BUTTON:
            onCancel();
            break;
        }
        break;
        
    case WM_SIZE:
        {
            RECT rect;
            GetClientRect(hWnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            // Resize log window to fit
            if (g_hLogEdit) {
                SetWindowPos(g_hLogEdit, NULL, 10, 210, width - 20, height - 220,
                           SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        
    case WM_APPEND_LOG:
        {
            char* logText = (char*)lParam;
            if (logText) {
                appendToLogDirect(logText);
                free(logText);
            }
        }
        break;
        
    case WM_UPDATE_PROGRESS:
        {
            char* progressMsg = (char*)lParam;
            if (progressMsg) {
                if (strcmp(progressMsg, "showDialog") == 0) {
                    showProgressDialog();
                } else if (strncmp(progressMsg, "BATCH_UPDATE_PROGRESS:", 22) == 0) {
                    // Parse: BATCH_UPDATE_PROGRESS:current:total:filename
                    int current, total;
                    char filename[512];
                    
                    // Parse current and total first
                    if (sscanf(progressMsg + 22, "%d:%d:", &current, &total) == 2) {
                        // Find the filename part after the second colon
                        char* msg = progressMsg + 22;
                        char* firstColon = strchr(msg, ':');
                        char* secondColon = firstColon ? strchr(firstColon + 1, ':') : NULL;
                        if (secondColon && *(secondColon + 1)) {
                            strcpy(filename, secondColon + 1);
                            updateProgress(current, total, filename);
                        }
                    }
                } else if (strcmp(progressMsg, "hideDialog") == 0) {
                    hideProgressDialog();
                }
                free(progressMsg);
            }
        }
        break;
        
    case WM_EXTRACTION_DONE:
        {
            int result = (int)wParam;
            
            // Hide progress dialog if shown
            hideProgressDialog();
            
            if (result == 0) {
                if (g_fileCount == 1) {
                    appendToLogDirect("Extraction completed successfully!\n");
                } else {
                    appendToLogDirect("Batch extraction completed successfully!\n");
                }
            } else if (result == -2) {
                // Cancellation - message already logged
            } else {
                if (g_fileCount == 1) {
                    appendToLogDirect("Extraction failed with errors.\n");
                } else {
                    appendToLogDirect("Batch extraction completed with some failures.\n");
                }
            }
            
            // Clean up thread handle
            if (g_hExtractionThread) {
                CloseHandle(g_hExtractionThread);
                g_hExtractionThread = NULL;
            }
            
            // Reset button states and text
            EnableWindow(g_hExtractButton, TRUE);
            EnableWindow(g_hCancelButton, FALSE);
            
            // Reset Extract button text
            if (g_fileCount > 1) {
                char buttonText[32];
                sprintf(buttonText, "Extract (%d)", g_fileCount);
                SetWindowText(g_hExtractButton, buttonText);
            } else {
                SetWindowText(g_hExtractButton, "Extract");
            }
        }
        break;
        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int showGUI() {
    WNDCLASSEX wcex;
    const char* className = "PSXExtractorGUI";
    
    // Register window class
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = className;
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wcex)) {
        return 1;
    }
    
    // Create main window centered on screen
    int windowWidth = 600;
    int windowHeight = 560;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    g_hMainWnd = CreateWindow(className, "PSX Extractor v3",
                            WS_OVERLAPPEDWINDOW,
                            x, y, windowWidth, windowHeight,
                            NULL, NULL, GetModuleHandle(NULL), NULL);
    
    if (!g_hMainWnd) {
        return 1;
    }
    
    ShowWindow(g_hMainWnd, SW_SHOW);
    UpdateWindow(g_hMainWnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

void logToGUI(const char* message) {
    if (g_hLogEdit) {
        appendToLog(message);
    }
}

void enableExtractButton(bool enabled) {
    if (g_hExtractButton) {
        EnableWindow(g_hExtractButton, enabled ? TRUE : FALSE);
    }
}

// GUI-aware prompt function
bool gui_prompt(const char* message, const char* title) {
    if (g_guiMode) {
        // Use MessageBox in GUI mode (use NULL as parent for subprocess)
        HWND parent = g_hMainWnd ? g_hMainWnd : NULL;
        int result = MessageBox(parent, message, title, MB_YESNO | MB_ICONQUESTION);
        return (result == IDYES);
    } else {
        // Fall back to console prompt
        fprintf(stdout, "%s (y/N): ", message);
        fflush(stdout);
        
        char input[10];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            char response = input[0];
            return (response == 'y' || response == 'Y');
        }
        return false; // Default to No if input fails
    }
}

// Progress Dialog Implementation
LRESULT CALLBACK ProgressDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDCANCEL) {
            // Cancel button pressed - terminate extraction thread
            if (g_hExtractionThread) {
                TerminateThread(g_hExtractionThread, 1);
                appendToLog("Batch extraction cancelled by user.\n");
                PostMessage(g_hMainWnd, WM_EXTRACTION_DONE, (WPARAM)-2, 0);
            }
            return 0;
        }
        break;
        
    case WM_CLOSE:
        // Prevent closing dialog with X button during extraction
        return 0;
        
    case WM_DESTROY:
        return 0;
        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void showProgressDialog() {
    if (g_hProgressDialog) return; // Already shown
    
    // Register progress dialog window class if not already registered
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = ProgressDialogProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = GetModuleHandle(NULL);
        wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = "ProgressDialog";
        wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClassEx(&wcex);
        classRegistered = true;
    }
    
    // Create dialog window manually
    g_hProgressDialog = CreateWindow("ProgressDialog", "Batch Extraction Progress", 
                                   WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 400, 180,
                                   g_hMainWnd, NULL, GetModuleHandle(NULL), NULL);
    
    if (g_hProgressDialog) {
        // Create controls manually
        g_hProgressText = CreateWindow("STATIC", "Preparing...", WS_VISIBLE | WS_CHILD,
                                     20, 20, 360, 20, g_hProgressDialog, NULL, GetModuleHandle(NULL), NULL);
        
        g_hProgressFile = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY,
                                     20, 50, 360, 40, g_hProgressDialog, NULL, GetModuleHandle(NULL), NULL);
        
        g_hProgressCancel = CreateWindow("BUTTON", "Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                       160, 110, 80, 30, g_hProgressDialog, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);
        
        // Center the dialog on the main window
        RECT parentRect, dialogRect;
        GetWindowRect(g_hMainWnd, &parentRect);
        GetWindowRect(g_hProgressDialog, &dialogRect);
        
        int x = parentRect.left + (parentRect.right - parentRect.left - (dialogRect.right - dialogRect.left)) / 2;
        int y = parentRect.top + (parentRect.bottom - parentRect.top - (dialogRect.bottom - dialogRect.top)) / 2;
        
        SetWindowPos(g_hProgressDialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        ShowWindow(g_hProgressDialog, SW_SHOW);
        EnableWindow(g_hMainWnd, FALSE); // Make it modal
    }
}

void hideProgressDialog() {
    if (g_hProgressDialog) {
        EnableWindow(g_hMainWnd, TRUE); // Re-enable main window
        DestroyWindow(g_hProgressDialog);
        g_hProgressDialog = NULL;
        g_hProgressText = NULL;
        g_hProgressFile = NULL;
        g_hProgressCancel = NULL;
    }
}

void updateProgress(int current, int total, const char* filename) {
    if (!g_hProgressDialog) return;
    
    char progressText[256];
    sprintf(progressText, "Processing file %d of %d", current, total);
    if (g_hProgressText) {
        SetWindowText(g_hProgressText, progressText);
    }
    
    if (filename && g_hProgressFile) {
        char fileText[512];
        const char* justFilename = strrchr(filename, '\\');
        justFilename = justFilename ? justFilename + 1 : filename;
        sprintf(fileText, "Extracting %s", justFilename);
        SetWindowText(g_hProgressFile, fileText);
    }
}

// Selection dialog data
struct SelectionDialogData {
    const char** options;
    int option_count;
    int selected_index;
    const char* message;
};

// Global variables for selection dialog
static int g_selectionResult = -1;
static HWND g_selectionDialog = NULL;

// Selection dialog procedure
LRESULT CALLBACK SelectionDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            SelectionDialogData* pData = (SelectionDialogData*)cs->lpCreateParams;
            if (!pData) return -1;
            
            // Store data pointer
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pData);
            
            // Create message text with more space and word wrap
            CreateWindow("STATIC", pData->message,
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                20, 20, 360, 60,
                hWnd, NULL, GetModuleHandle(NULL), NULL);
            
            // Create buttons for each option (moved down to make room)
            int buttonY = 90;
            for (int i = 0; i < pData->option_count; i++) {
                CreateWindow("BUTTON", pData->options[i],
                    WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                    20, buttonY, 360, 30,
                    hWnd, (HMENU)(1000 + i), GetModuleHandle(NULL), NULL);
                buttonY += 35;
            }
            
            return 0;
        }
        
    case WM_COMMAND:
        {
            int buttonId = LOWORD(wParam);
            if (buttonId >= 1000 && buttonId < 1020) {
                // Option button clicked
                g_selectionResult = buttonId - 1000;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
        
    case WM_CLOSE:
        g_selectionResult = 0; // Default to first option
        DestroyWindow(hWnd);
        return 0;
        
    case WM_DESTROY:
        g_selectionDialog = NULL;
        return 0;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Create selection dialog with individual buttons
int gui_create_selection_dialog(const char* title, const char* message, const char* options[], int option_count) {
    SelectionDialogData data;
    data.options = options;
    data.option_count = option_count;
    data.selected_index = 0;
    data.message = message;
    
    g_selectionResult = 0; // Default to first option
    
    // Register window class if needed
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = SelectionDialogProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "CueSelectionDialog";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        
        if (RegisterClass(&wc)) {
            classRegistered = true;
        }
    }
    
    // Calculate dialog height based on option count (extra space for message)
    int dialogHeight = 140 + (option_count * 35);
    
    // Create the dialog
    HWND parent = g_hMainWnd;
    
    // Get parent window rect for centering
    RECT parentRect;
    int x, y;
    if (parent && GetWindowRect(parent, &parentRect)) {
        x = parentRect.left + (parentRect.right - parentRect.left - 420) / 2;
        y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
    } else {
        // Center on screen if parent rect is invalid
        x = (GetSystemMetrics(SM_CXSCREEN) - 420) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - dialogHeight) / 2;
    }
    
    g_selectionDialog = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "CueSelectionDialog",
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, 420, dialogHeight,
        parent, NULL, GetModuleHandle(NULL), &data);
        
    if (g_selectionDialog) {
        // Force the window to be visible and on top
        ShowWindow(g_selectionDialog, SW_SHOW);
        UpdateWindow(g_selectionDialog);
        SetWindowPos(g_selectionDialog, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        BringWindowToTop(g_selectionDialog);
        
        // Make it modal
        EnableWindow(parent, FALSE);
        SetForegroundWindow(g_selectionDialog);
        SetActiveWindow(g_selectionDialog);
        
        // Message loop
        MSG msg;
        while (IsWindow(g_selectionDialog)) {
            if (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                break;
            }
        }
        
        // Re-enable parent
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }
    
    return g_selectionResult;
}

// GUI-aware selection function
int gui_select_option(const char* title, const char* message, const char* options[], int option_count) {
    if (!isGUIMode()) {
        // Console mode - use existing console selection logic
        printf("\n%s\n", message);
        
        for (int i = 0; i < option_count; i++) {
            printf("  %d) %s\n", i + 1, options[i]);
        }
        
        printf("\nEnter your choice (1-%d): ", option_count);
        fflush(stdout);
        
        int choice = 0;
        char input[16];
        
        if (fgets(input, sizeof(input), stdin)) {
            choice = atoi(input);
        }
        
        // Validate choice
        if (choice < 1 || choice > option_count) {
            printf("Invalid choice. Using first option: %s\n", options[0]);
            return 0; // Return 0-based index
        }
        
        printf("Selected: %s\n\n", options[choice - 1]);
        return choice - 1; // Return 0-based index
    } else {
        // GUI mode - create a proper dialog with individual buttons
        return gui_create_selection_dialog(title, message, options, option_count);
    }
}
