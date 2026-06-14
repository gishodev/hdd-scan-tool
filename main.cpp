#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cstdio>
#include "scanner.h"
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// UI Constants
#define TIMER_UI_ID 1
#define TIMER_UI_INTERVAL 33 // ~30 FPS

#define IDC_COMBO_DRIVE 1001
#define IDC_COMBO_BLOCK 1002
#define IDC_BTN_START   1003
#define IDC_BTN_PAUSE   1004
#define IDC_BTN_STOP    1005
#define IDC_LOG_EDIT    1006
#define IDC_COMBO_LANG  1007

// Globals for Controls
HWND g_hwndMain = NULL;
HWND g_hwndDriveCombo = NULL;
HWND g_hwndBlockCombo = NULL;
HWND g_hwndBtnStart = NULL;
HWND g_hwndBtnPause = NULL;
HWND g_hwndBtnStop = NULL;
HWND g_hwndLogEdit = NULL;
HWND g_hwndLangCombo = NULL;

std::vector<DriveInfo> g_Drives;
int g_HoveredCell = -1;
HBRUSH g_hCardBrush = NULL;
HBRUSH g_hBgBrush = NULL;

// Window dimensions
const int WINDOW_WIDTH = 980;
const int WINDOW_HEIGHT = 740;

// Grid coordinates (absolute screen client)
const int GRID_START_X = 348;
const int GRID_START_Y = 158;
const int CELL_SIZE = 6;
const int CELL_GAP = 1;

// Internal prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawUIComponents(Graphics& graphics, int w, int h);
void DrawCard(Graphics& graphics, float x, float y, float w, float h, const wchar_t* title);
void FormatTime(ULONGLONG totalSeconds, wchar_t* buffer, size_t bufferSize);
void UpdateUILanguage();


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Enable Visual Styles (Common Controls V6)
    INITCOMMONCONTROLSEX icex = { 0 };
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Create Background brushes
    g_hBgBrush = CreateSolidBrush(RGB(24, 24, 28));
    g_hCardBrush = CreateSolidBrush(RGB(33, 33, 38));

    // Register Window Class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBgBrush;
    wc.lpszClassName = L"HddSurfaceScannerClass";
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR);
        return 0;
    }

    // Calculate window size for client area 960x700
    RECT wr = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    g_hwndMain = CreateWindowExW(
        0,
        L"HddSurfaceScannerClass",
        L"HDD Surface Scanner & Diagnostics (x86 Native)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hwndMain) {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    // Main Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // GDI+ Shutdown
    GdiplusShutdown(gdiplusToken);
    
    // Clean up solid brushes
    DeleteObject(g_hBgBrush);
    DeleteObject(g_hCardBrush);

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            // Retrieve system drives
            g_Drives = EnumerateDrives();
            
            // 1. Target Drive Dropdown
            g_hwndDriveCombo = CreateWindowW(
                L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                30, 45, 350, 200,
                hWnd, (HMENU)IDC_COMBO_DRIVE, GetModuleHandle(NULL), NULL
            );
            
            // 2. Block Size Dropdown
            g_hwndBlockCombo = CreateWindowW(
                L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                400, 45, 150, 200,
                hWnd, (HMENU)IDC_COMBO_BLOCK, GetModuleHandle(NULL), NULL
            );

            // Set Control Fonts
            HFONT hControlFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessage(g_hwndDriveCombo, WM_SETFONT, (WPARAM)hControlFont, TRUE);
            SendMessage(g_hwndBlockCombo, WM_SETFONT, (WPARAM)hControlFont, TRUE);

            // 3. Control Buttons (Owner Drawn)
            g_hwndBtnStart = CreateWindowW(
                L"BUTTON", L"Start Scan",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                570, 35, 110, 30,
                hWnd, (HMENU)IDC_BTN_START, GetModuleHandle(NULL), NULL
            );
            
            g_hwndBtnPause = CreateWindowW(
                L"BUTTON", L"Pause",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_DISABLED,
                690, 35, 110, 30,
                hWnd, (HMENU)IDC_BTN_PAUSE, GetModuleHandle(NULL), NULL
            );

            g_hwndBtnStop = CreateWindowW(
                L"BUTTON", L"Stop",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_DISABLED,
                810, 35, 110, 30,
                hWnd, (HMENU)IDC_BTN_STOP, GetModuleHandle(NULL), NULL
            );

            // 4. Log Edit Control (Multi-line, Dark Styled)
            g_hwndLogEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                325, 515, 605, 155,
                hWnd, (HMENU)IDC_LOG_EDIT, GetModuleHandle(NULL), NULL
            );

            HFONT hLogFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SendMessage(g_hwndLogEdit, WM_SETFONT, (WPARAM)hLogFont, TRUE);

            // 5. Language Dropdown in bottom-right card header
            g_hwndLangCombo = CreateWindowW(
                L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                810, 482, 120, 150,
                hWnd, (HMENU)IDC_COMBO_LANG, GetModuleHandle(NULL), NULL
            );
            SendMessage(g_hwndLangCombo, WM_SETFONT, (WPARAM)hControlFont, TRUE);
            SendMessageW(g_hwndLangCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
            SendMessageW(g_hwndLangCombo, CB_ADDSTRING, 0, (LPARAM)L"日本語");
            SendMessageW(g_hwndLangCombo, CB_ADDSTRING, 0, (LPARAM)L"简体中文");
            SendMessageW(g_hwndLangCombo, CB_SETCURSEL, 0, 0); // Default to English

            // Initialize all text items in UI
            UpdateUILanguage();

            // Set up 30 FPS redraw and UI updater timer
            SetTimer(hWnd, TIMER_UI_ID, TIMER_UI_INTERVAL, NULL);
            
            AddLogEntry(GetStr(IDS_LOG_INIT_SUCCESS).c_str());
            if (g_Drives.empty()) {
                AddLogEntry(GetStr(IDS_LOG_NO_DRIVES).c_str());
            } else {
                AddLogEntry(GetStr(IDS_LOG_DETECTED_DRIVES).c_str(), g_Drives.size());
            }
            break;
        }


        case WM_TIMER: {
            if (wParam == TIMER_UI_ID) {
                // Fetch background state
                ScanStats stats;
                GetSharedStats(&stats);

                // Update controls enabled/disabled properties only on state changes
                static bool s_LastRunning = false;
                static bool s_LastPaused = false;
                static bool s_FirstRun = true;

                if (s_FirstRun || stats.isRunning != s_LastRunning || stats.isPaused != s_LastPaused) {
                    EnableWindow(g_hwndBtnStart, !stats.isRunning);
                    EnableWindow(g_hwndBtnPause, stats.isRunning);
                    EnableWindow(g_hwndBtnStop, stats.isRunning);
                    EnableWindow(g_hwndDriveCombo, !stats.isRunning);
                    EnableWindow(g_hwndBlockCombo, !stats.isRunning);

                    // Toggle Pause/Resume button labels
                    wchar_t currentLabel[32];
                    GetWindowTextW(g_hwndBtnPause, currentLabel, 32);
                    if (stats.isPaused && wcscmp(currentLabel, GetStr(IDS_BTN_RESUME).c_str()) != 0) {
                        SetWindowTextW(g_hwndBtnPause, GetStr(IDS_BTN_RESUME).c_str());
                    } else if (!stats.isPaused && wcscmp(currentLabel, GetStr(IDS_BTN_PAUSE).c_str()) != 0) {
                        SetWindowTextW(g_hwndBtnPause, GetStr(IDS_BTN_PAUSE).c_str());
                    }

                    s_LastRunning = stats.isRunning;
                    s_LastPaused = stats.isPaused;
                    s_FirstRun = false;

                    // Force full window redraw on transition to clean child control rendering artifacts
                    InvalidateRect(hWnd, NULL, FALSE);
                }

                // Check logs
                static size_t lastLogCount = 0;
                std::vector<std::wstring> logs = GetLogEntries();
                if (logs.size() != lastLogCount) {
                    std::wstring allLogsText = L"";
                    for (const auto& log : logs) {
                        allLogsText += log + L"\r\n";
                    }
                    SetWindowTextW(g_hwndLogEdit, allLogsText.c_str());
                    SendMessage(g_hwndLogEdit, EM_LINESCROLL, 0, 0xFFFF); // Scroll to bottom
                    lastLogCount = logs.size();
                }

                // Force panel redraw only when scan is active
                if (stats.isRunning) {
                    RECT rcStats = { 15, 100, 295, 460 };
                    RECT rcGrid = { 310, 100, 945, 460 };
                    RECT rcGraph = { 15, 475, 295, 685 };
                    InvalidateRect(hWnd, &rcStats, FALSE);
                    InvalidateRect(hWnd, &rcGrid, FALSE);
                    InvalidateRect(hWnd, &rcGraph, FALSE);
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Bounds check grid area
            int gridW = GRID_COLS * (CELL_SIZE + CELL_GAP) - CELL_GAP;
            int gridH = GRID_ROWS * (CELL_SIZE + CELL_GAP) - CELL_GAP;

            int newHoveredCell = -1;
            if (x >= GRID_START_X && x < GRID_START_X + gridW &&
                y >= GRID_START_Y && y < GRID_START_Y + gridH) {
                
                int cellX = (x - GRID_START_X) % (CELL_SIZE + CELL_GAP);
                int cellY = (y - GRID_START_Y) % (CELL_SIZE + CELL_GAP);

                // Only record hover if not on cell gaps
                if (cellX < CELL_SIZE && cellY < CELL_SIZE) {
                    int col = (x - GRID_START_X) / (CELL_SIZE + CELL_GAP);
                    int row = (y - GRID_START_Y) / (CELL_SIZE + CELL_GAP);
                    newHoveredCell = row * GRID_COLS + col;
                }
            }

            if (newHoveredCell != g_HoveredCell) {
                g_HoveredCell = newHoveredCell;
                RECT rcGrid = { 310, 100, 945, 460 };
                InvalidateRect(hWnd, &rcGrid, FALSE);
            }
            break;
        }


        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_COMBO_LANG && wmEvent == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(g_hwndLangCombo, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR) {
                    g_CurrentLanguage = (Language)sel;
                    UpdateUILanguage();
                }
            }
            else if (wmId == IDC_BTN_START && wmEvent == BN_CLICKED) {
                if (g_Drives.empty()) {
                    MessageBoxW(hWnd, L"No physical drives available to scan.", L"Error", MB_ICONWARNING);
                    break;
                }

                int driveSel = (int)SendMessageW(g_hwndDriveCombo, CB_GETCURSEL, 0, 0);
                int blockSel = (int)SendMessageW(g_hwndBlockCombo, CB_GETCURSEL, 0, 0);

                if (driveSel != CB_ERR && blockSel != CB_ERR) {
                    int driveIndex = g_Drives[driveSel].index;
                    
                    // Determine blocks size bytes
                    DWORD blockSizeBytes = 65536; // Default
                    switch (blockSel) {
                        case 0: blockSizeBytes = 512; break;
                        case 1: blockSizeBytes = 4096; break;
                        case 2: blockSizeBytes = 65536; break;
                        case 3: blockSizeBytes = 262144; break;
                        case 4: blockSizeBytes = 1048576; break;
                    }

                    StartScan(driveIndex, blockSizeBytes);
                }
            }
            else if (wmId == IDC_BTN_PAUSE && wmEvent == BN_CLICKED) {
                ScanStats stats;
                GetSharedStats(&stats);
                if (stats.isRunning) {
                    if (stats.isPaused) {
                        ResumeScan();
                    } else {
                        PauseScan();
                    }
                }
            }
            else if (wmId == IDC_BTN_STOP && wmEvent == BN_CLICKED) {
                StopScan();
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;
            
            // Style the read-only edit control dark
            if (hwndStatic == g_hwndLogEdit) {
                SetTextColor(hdcStatic, RGB(225, 225, 230));
                SetBkColor(hdcStatic, RGB(33, 33, 38));
                return (INT_PTR)g_hCardBrush;
            }
            
            // General dark styling for any other static/combo label fields
            SetTextColor(hdcStatic, RGB(156, 163, 175));
            SetBkColor(hdcStatic, RGB(24, 24, 28));
            return (INT_PTR)g_hBgBrush;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlType == ODT_BUTTON) {
                Graphics graphics(pDIS->hDC);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);

                RectF rectF((REAL)pDIS->rcItem.left, (REAL)pDIS->rcItem.top, 
                    (REAL)(pDIS->rcItem.right - pDIS->rcItem.left), 
                    (REAL)(pDIS->rcItem.bottom - pDIS->rcItem.top));

                // Paint the background of the button control with the parent card's color
                // to prevent white corner artifacts from showing outside the rounded borders.
                SolidBrush cardBgBrush(Color(33, 33, 38));
                graphics.FillRectangle(&cardBgBrush, rectF);

                Color btnColor;
                Color textColor(255, 255, 255);
                bool isPressed = (pDIS->itemState & ODS_SELECTED) != 0;
                bool isDisabled = (pDIS->itemState & ODS_DISABLED) != 0;

                if (isDisabled) {
                    btnColor = Color(40, 40, 45);
                    textColor = Color(90, 90, 95);
                } else {
                    if (pDIS->CtlID == IDC_BTN_START) {
                        btnColor = isPressed ? Color(22, 101, 52) : Color(34, 197, 94);
                    } else if (pDIS->CtlID == IDC_BTN_PAUSE) {
                        btnColor = isPressed ? Color(30, 58, 138) : Color(59, 130, 246);
                    } else if (pDIS->CtlID == IDC_BTN_STOP) {
                        btnColor = isPressed ? Color(153, 27, 27) : Color(239, 68, 68);
                    } else {
                        btnColor = isPressed ? Color(50, 50, 55) : Color(70, 70, 75);
                    }
                }

                // Draw rounded path
                GraphicsPath path;
                float r = 5.0f;
                path.AddArc(rectF.X, rectF.Y, r * 2, r * 2, 180, 90);
                path.AddArc(rectF.X + rectF.Width - r * 2, rectF.Y, r * 2, r * 2, 270, 90);
                path.AddArc(rectF.X + rectF.Width - r * 2, rectF.Y + rectF.Height - r * 2, r * 2, r * 2, 0, 90);
                path.AddArc(rectF.X, rectF.Y + rectF.Height - r * 2, r * 2, r * 2, 90, 90);
                path.CloseFigure();

                SolidBrush brush(btnColor);
                graphics.FillPath(&brush, &path);

                // Thin border
                Pen borderPen(Color(100, 48, 48, 56), 1.0f);
                graphics.DrawPath(&borderPen, &path);

                // Draw text
                wchar_t text[128];
                GetWindowTextW(pDIS->hwndItem, text, 128);

                Font font(L"Segoe UI", 9, FontStyleBold);
                StringFormat format;
                format.SetAlignment(StringAlignmentCenter);
                format.SetLineAlignment(StringAlignmentCenter);

                SolidBrush textBrush(textColor);
                graphics.DrawString(text, -1, &font, rectF, &format, &textBrush);

                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            // Double Buffering
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
            HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

            Graphics graphics(memDC);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            // Draw Background
            SolidBrush bgBrush(Color(24, 24, 28));
            graphics.FillRectangle(&bgBrush, 0, 0, width, height);

            // Draw layout components
            DrawUIComponents(graphics, width, height);

            // Copy memory buffer back onto window DC
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            StopScan();
            KillTimer(hWnd, TIMER_UI_ID);
            PostQuitMessage(0);
            break;
        }
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

void DrawCard(Graphics& graphics, float x, float y, float w, float h, const wchar_t* title) {
    RectF rect(x, y, w, h);
    float r = 6.0f;

    GraphicsPath path;
    path.AddArc(rect.X, rect.Y, r * 2, r * 2, 180, 90);
    path.AddArc(rect.X + rect.Width - r * 2, rect.Y, r * 2, r * 2, 270, 90);
    path.AddArc(rect.X + rect.Width - r * 2, rect.Y + rect.Height - r * 2, r * 2, r * 2, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - r * 2, r * 2, r * 2, 90, 90);
    path.CloseFigure();

    SolidBrush fillBrush(Color(33, 33, 38));
    graphics.FillPath(&fillBrush, &path);

    Pen borderPen(Color(48, 48, 56), 1.0f);
    graphics.DrawPath(&borderPen, &path);

    if (title && wcslen(title) > 0) {
        Font titleFont(L"Segoe UI", 9, FontStyleBold);
        SolidBrush titleBrush(Color(243, 244, 246));
        graphics.DrawString(title, -1, &titleFont, PointF(x + 15.0f, y + 10.0f), &titleBrush);

        Pen dividerPen(Color(48, 48, 56), 1.0f);
        graphics.DrawLine(&dividerPen, x + 15.0f, y + 32.0f, x + w - 15.0f, y + 32.0f);
    }
}

void DrawStatsRow(Graphics& graphics, Font& font, float xStart, float xEnd, float y, const wchar_t* label, const wchar_t* value, Color valColor) {
    SolidBrush labelBrush(Color(156, 163, 175));
    SolidBrush valBrush(valColor);
    
    graphics.DrawString(label, -1, &font, PointF(xStart, y), &labelBrush);

    StringFormat rightAlign;
    rightAlign.SetAlignment(StringAlignmentFar);
    
    RectF valRect(xStart + 100.0f, y, xEnd - xStart - 100.0f, 20.0f);
    graphics.DrawString(value, -1, &font, valRect, &rightAlign, &valBrush);
}

void DrawUIComponents(Graphics& graphics, int w, int h) {
    // 1. Top configuration card
    DrawCard(graphics, 15.0f, 15.0f, 930.0f, 70.0f, L"");
    
    Font labelFont(L"Segoe UI", 9, FontStyleBold);
    SolidBrush textBrush(Color(156, 163, 175));
    graphics.DrawString(GetStr(IDS_TARGET_DRIVE).c_str(), -1, &labelFont, PointF(30.0f, 24.0f), &textBrush);
    graphics.DrawString(GetStr(IDS_BLOCK_SIZE).c_str(), -1, &labelFont, PointF(400.0f, 24.0f), &textBrush);

    // Fetch scanner state
    ScanStats stats;
    GetSharedStats(&stats);

    // 2. Stats card
    DrawCard(graphics, 15.0f, 100.0f, 280.0f, 360.0f, GetStr(IDS_DIAGNOSTICS_TITLE).c_str());

    Font statsFont(L"Segoe UI", 9);
    float statsXStart = 30.0f;
    float statsXEnd = 280.0f;

    // Formatting string buffers
    std::wstring statusVal = GetStr(IDS_VAL_IDLE);
    Color statusColor(156, 163, 175);
    if (stats.isRunning) {
        if (stats.isPaused) {
            statusVal = GetStr(IDS_VAL_PAUSED);
            statusColor = Color(234, 179, 8); // Yellow
        } else {
            statusVal = GetStr(IDS_VAL_SCANNING);
            statusColor = Color(59, 130, 246); // Blue
        }
    } else {
        if (wcscmp(stats.currentStatusText, L"Scan Completed") == 0) {
            statusVal = GetStr(IDS_VAL_COMPLETED);
            statusColor = Color(34, 197, 94); // Green
        } else if (wcsstr(stats.currentStatusText, L"Failed") != NULL) {
            statusVal = GetStr(IDS_VAL_ERROR);
            statusColor = Color(239, 68, 68); // Red
        }
    }

    wchar_t sizeVal[64] = L"--";
    if (stats.totalBytes > 0) {
        swprintf_s(sizeVal, L"%.2f GB", (double)stats.totalBytes / (1024.0 * 1024.0 * 1024.0));
    }

    wchar_t progressVal[64] = L"0.00%";
    if (stats.totalBytes > 0) {
        swprintf_s(progressVal, L"%.2f%%", (double)stats.bytesScanned * 100.0 / stats.totalBytes);
    }

    wchar_t blocksVal[128] = L"0 / 0";
    if (stats.totalBlocks > 0) {
        swprintf_s(blocksVal, L"%I64u / %I64u", stats.blocksScanned, stats.totalBlocks);
    }

    wchar_t speedVal[128] = L"0.0 / 0.0 MB/s";
    if (stats.isRunning || stats.bytesScanned > 0) {
        swprintf_s(speedVal, L"%.1f / %.1f MB/s", stats.currentSpeedMBs, stats.averageSpeedMBs);
    }

    ULONGLONG healthySectors = 0;
    if (stats.blocksScanned > 0) {
        ULONGLONG nonHealthy = stats.slowSectors + stats.verySlowSectors + stats.badSectors;
        if (stats.blocksScanned >= nonHealthy) {
            healthySectors = stats.blocksScanned - nonHealthy;
        }
    }

    wchar_t healthyVal[64];
    swprintf_s(healthyVal, L"%I64u", healthySectors);

    wchar_t slowVal[64];
    swprintf_s(slowVal, L"%I64u", stats.slowSectors);

    wchar_t verySlowVal[64];
    swprintf_s(verySlowVal, L"%I64u", stats.verySlowSectors);

    wchar_t badVal[64];
    swprintf_s(badVal, L"%I64u", stats.badSectors);

    wchar_t elapsedVal[64];
    FormatTime(stats.elapsedMs / 1000, elapsedVal, 64);

    wchar_t remainingVal[64] = L"--:--:--";
    if (stats.isRunning && !stats.isPaused && stats.averageSpeedMBs > 0.0) {
        ULONGLONG bytesRemaining = stats.totalBytes - stats.bytesScanned;
        double speedBytes = stats.averageSpeedMBs * 1024.0 * 1024.0;
        double remSecs = (double)bytesRemaining / speedBytes;
        FormatTime((ULONGLONG)remSecs, remainingVal, 64);
    }

    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 145.0f, GetStr(IDS_STAT_STATUS).c_str(), statusVal.c_str(), statusColor);
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 170.0f, GetStr(IDS_STAT_CAPACITY).c_str(), sizeVal, Color(243, 244, 246));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 195.0f, GetStr(IDS_STAT_PROGRESS).c_str(), progressVal, Color(243, 244, 246));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 220.0f, GetStr(IDS_STAT_BLOCKS).c_str(), blocksVal, Color(156, 163, 175));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 245.0f, GetStr(IDS_STAT_SPEED).c_str(), speedVal, Color(139, 92, 246));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 270.0f, GetStr(IDS_STAT_HEALTHY).c_str(), healthyVal, Color(34, 197, 94));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 295.0f, GetStr(IDS_STAT_SLOW).c_str(), slowVal, Color(234, 179, 8));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 320.0f, GetStr(IDS_STAT_VERY_SLOW).c_str(), verySlowVal, Color(249, 115, 22));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 345.0f, GetStr(IDS_STAT_BAD_SECTORS).c_str(), badVal, stats.badSectors > 0 ? Color(239, 68, 68) : Color(156, 163, 175));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 370.0f, GetStr(IDS_STAT_ELAPSED).c_str(), elapsedVal, Color(243, 244, 246));
    DrawStatsRow(graphics, statsFont, statsXStart, statsXEnd, 395.0f, GetStr(IDS_STAT_REMAINING).c_str(), remainingVal, Color(243, 244, 246));

    // 3. Grid map card
    DrawCard(graphics, 310.0f, 100.0f, 635.0f, 360.0f, GetStr(IDS_SURFACE_GRID_TITLE).c_str());


    // Draw grid cells
    for (int row = 0; row < GRID_ROWS; ++row) {
        for (int col = 0; col < GRID_COLS; ++col) {
            int idx = row * GRID_COLS + col;
            
            BlockStatus cellStatus = g_Grid[idx];
            Color cellColor;

            switch (cellStatus) {
                case STATUS_UNSCANNED:
                    cellColor = Color(45, 45, 52); // Charcoal dark
                    break;
                case STATUS_SCANNING:
                    cellColor = Color(59, 130, 246); // Active Blue
                    break;
                case STATUS_HEALTHY:
                    cellColor = Color(34, 197, 94); // Green
                    break;
                case STATUS_SLOW:
                    cellColor = Color(234, 179, 8); // Yellow
                    break;
                case STATUS_VERY_SLOW:
                    cellColor = Color(249, 115, 22); // Orange
                    break;
                case STATUS_BAD:
                    cellColor = Color(239, 68, 68); // Red
                    break;
                default:
                    cellColor = Color(45, 45, 52);
                    break;
            }

            // Highlighting the hovered cell
            if (idx == g_HoveredCell) {
                cellColor = Color(255, 255, 255); // White highlight
            }

            float cx = (float)(GRID_START_X + col * (CELL_SIZE + CELL_GAP));
            float cy = (float)(GRID_START_Y + row * (CELL_SIZE + CELL_GAP));

            SolidBrush cellBrush(cellColor);
            graphics.FillRectangle(&cellBrush, cx, cy, (REAL)CELL_SIZE, (REAL)CELL_SIZE);
        }
    }

    // Hover tooltip/details text rendering at the bottom margin of Card 3
    if (g_HoveredCell != -1 && stats.totalBlocks > 0) {
        ULONGLONG blocksPerCell = stats.totalBlocks / GRID_SIZE;
        if (blocksPerCell == 0) blocksPerCell = 1;
        
        ULONGLONG startBlock = g_HoveredCell * blocksPerCell;
        ULONGLONG endBlock = (g_HoveredCell + 1) * blocksPerCell - 1;
        if (endBlock >= stats.totalBlocks) endBlock = stats.totalBlocks - 1;
        
        DWORD sectorsPerBlock = stats.blockSizeBytes / 512;
        ULONGLONG startLBA = startBlock * sectorsPerBlock;
        ULONGLONG endLBA = (endBlock + 1) * sectorsPerBlock - 1;

        BlockStatus cellStatus = g_Grid[g_HoveredCell];
        std::wstring statusStr = GetStr(IDS_GRID_UNSCANNED);
        Color statusColor(156, 163, 175);

        switch (cellStatus) {
            case STATUS_SCANNING: statusStr = GetStr(IDS_GRID_SCANNING); statusColor = Color(59, 130, 246); break;
            case STATUS_HEALTHY: statusStr = GetStr(IDS_GRID_HEALTHY); statusColor = Color(34, 197, 94); break;
            case STATUS_SLOW: statusStr = GetStr(IDS_GRID_SLOW); statusColor = Color(234, 179, 8); break;
            case STATUS_VERY_SLOW: statusStr = GetStr(IDS_GRID_VERY_SLOW); statusColor = Color(249, 115, 22); break;
            case STATUS_BAD: statusStr = GetStr(IDS_GRID_BAD); statusColor = Color(239, 68, 68); break;
        }

        double latency = g_GridLatency[g_HoveredCell];

        wchar_t infoText[256];
        std::wstring lblGrid = g_CurrentLanguage == LANG_JA ? L"グリッド" : (g_CurrentLanguage == LANG_ZH ? L"网格" : L"Grid");
        std::wstring lblStatus = g_CurrentLanguage == LANG_JA ? L"ステータス" : (g_CurrentLanguage == LANG_ZH ? L"状态" : L"Status");
        std::wstring lblLatency = g_CurrentLanguage == LANG_JA ? L"最大遅延" : (g_CurrentLanguage == LANG_ZH ? L"最大延迟" : L"Max Latency");

        if (latency > 0) {
            swprintf_s(infoText, L"%s %d | LBA %I64u - %I64u | %s: %s | %s: %.1f ms", 
                lblGrid.c_str(), g_HoveredCell, startLBA, endLBA, lblStatus.c_str(), statusStr.c_str(), lblLatency.c_str(), latency);
        } else {
            swprintf_s(infoText, L"%s %d | LBA %I64u - %I64u | %s: %s", 
                lblGrid.c_str(), g_HoveredCell, startLBA, endLBA, lblStatus.c_str(), statusStr.c_str());
        }

        Font detailsFont(L"Segoe UI", 9, FontStyleBold);
        SolidBrush detailsBrush(statusColor);
        graphics.DrawString(infoText, -1, &detailsFont, PointF(325.0f, 436.0f), &detailsBrush);
    } else {
        // Default text when no cell hovered
        Font detailsFont(L"Segoe UI", 9, FontStyleItalic);
        SolidBrush detailsBrush(Color(100, 100, 110));
        graphics.DrawString(GetStr(IDS_GRID_HOVER_PROMPT).c_str(), -1, &detailsFont, PointF(325.0f, 436.0f), &detailsBrush);
    }

    // 4. Speed graph card
    DrawCard(graphics, 15.0f, 475.0f, 280.0f, 210.0f, GetStr(IDS_SPEED_BENCHMARK_TITLE).c_str());
    
    // Graph Area Bounds
    float graphX = 30.0f;
    float graphY = 520.0f;
    float graphW = 250.0f;
    float graphH = 140.0f;

    // Draw horizontal grid lines
    Pen gridPen(Color(30, 48, 48, 56), 1.0f);
    for (int i = 1; i < 4; ++i) {
        float yPos = graphY + (graphH * 0.25f * i);
        graphics.DrawLine(&gridPen, graphX, yPos, graphX + graphW, yPos);
    }

    // Draw speed chart
    std::vector<double> speedHistory;
    EnterCriticalSection(&g_ScanStatsCS);
    speedHistory = g_SpeedHistory;
    LeaveCriticalSection(&g_ScanStatsCS);

    if (speedHistory.size() > 1) {
        // Find max speed in history (scale chart dynamically)
        double maxSpeed = 100.0;
        for (double s : speedHistory) {
            if (s > maxSpeed) maxSpeed = s;
        }
        maxSpeed *= 1.2; // Add 20% margin at top

        // Graph formatting arrays
        std::vector<PointF> points;
        float xStep = graphW / 59.0f;

        for (size_t i = 0; i < speedHistory.size(); ++i) {
            float px = graphX + i * xStep;
            float py = graphY + graphH - (float)((speedHistory[i] / maxSpeed) * graphH);
            points.push_back(PointF(px, py));
        }

        // 1. Draw gradient area under the curve
        GraphicsPath fillPath;
        fillPath.AddLine(points.front().X, graphY + graphH, points.front().X, points.front().Y);
        for (size_t i = 1; i < points.size(); ++i) {
            fillPath.AddLine(points[i - 1].X, points[i - 1].Y, points[i].X, points[i].Y);
        }
        fillPath.AddLine(points.back().X, points.back().Y, points.back().X, graphY + graphH);
        fillPath.CloseFigure();

        LinearGradientBrush areaBrush(
            PointF(graphX, graphY),
            PointF(graphX, graphY + graphH),
            Color(80, 139, 92, 246), // Transparent purple top
            Color(0, 139, 92, 246)  // Full transparent bottom
        );
        graphics.FillPath(&areaBrush, &fillPath);

        // 2. Draw active speed line
        Pen linePen(Color(255, 139, 92, 246), 2.0f);
        graphics.DrawCurve(&linePen, points.data(), (int)points.size());

        // Speed Label in corner
        wchar_t curSpdText[64];
        swprintf_s(curSpdText, L"%.1f MB/s", stats.currentSpeedMBs);
        Font speedFont(L"Segoe UI", 12, FontStyleBold);
        SolidBrush speedBrush(Color(220, 139, 92, 246));
        graphics.DrawString(curSpdText, -1, &speedFont, PointF(graphX + 10.0f, graphY + 10.0f), &speedBrush);

        // Max Speed boundary label
        wchar_t maxSpdText[64];
        swprintf_s(maxSpdText, L"Max: %.0f MB/s", maxSpeed);
        Font maxFont(L"Segoe UI", 8);
        SolidBrush maxBrush(Color(120, 156, 163, 175));
        graphics.DrawString(maxSpdText, -1, &maxFont, PointF(graphX + graphW - 80.0f, graphY - 12.0f), &maxBrush);
    } else {
        // Prompt for scan start
        Font promptFont(L"Segoe UI", 9, FontStyleItalic);
        SolidBrush promptBrush(Color(100, 100, 110));
        StringFormat centerAlign;
        centerAlign.SetAlignment(StringAlignmentCenter);
        centerAlign.SetLineAlignment(StringAlignmentCenter);
        
        RectF graphRect(graphX, graphY, graphW, graphH);
        graphics.DrawString(GetStr(IDS_GRID_AWAITING).c_str(), -1, &promptFont, graphRect, &centerAlign, &promptBrush);
    }

    // 5. Event logs card background
    DrawCard(graphics, 310.0f, 475.0f, 635.0f, 210.0f, GetStr(IDS_SURFACE_LOG_TITLE).c_str());
}

void FormatTime(ULONGLONG totalSeconds, wchar_t* buffer, size_t bufferSize) {
    if (totalSeconds == (ULONGLONG)-1) {
        swprintf_s(buffer, bufferSize, L"--:--:--");
        return;
    }
    ULONGLONG hours = totalSeconds / 3600;
    ULONGLONG minutes = (totalSeconds % 3600) / 60;
    ULONGLONG seconds = totalSeconds % 60;
    swprintf_s(buffer, bufferSize, L"%02I64u:%02I64u:%02I64u", hours, minutes, seconds);
}

void UpdateUILanguage() {
    // 1. Update Buttons
    SetWindowTextW(g_hwndBtnStart, GetStr(IDS_BTN_START).c_str());
    
    ScanStats stats;
    GetSharedStats(&stats);
    if (stats.isPaused) {
        SetWindowTextW(g_hwndBtnPause, GetStr(IDS_BTN_RESUME).c_str());
    } else {
        SetWindowTextW(g_hwndBtnPause, GetStr(IDS_BTN_PAUSE).c_str());
    }
    SetWindowTextW(g_hwndBtnStop, GetStr(IDS_BTN_STOP).c_str());

    // 2. Update Block Size Combo entries
    int curBlockSel = (int)SendMessageW(g_hwndBlockCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(g_hwndBlockCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 5; ++i) {
        SendMessageW(g_hwndBlockCombo, CB_ADDSTRING, 0, (LPARAM)GetBlockSizeStr(i).c_str());
    }
    SendMessageW(g_hwndBlockCombo, CB_SETCURSEL, curBlockSel == CB_ERR ? 2 : curBlockSel, 0);

    // 3. Update Drive Combo entries
    int curDriveSel = (int)SendMessageW(g_hwndDriveCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(g_hwndDriveCombo, CB_RESETCONTENT, 0, 0);
    if (g_Drives.empty()) {
        std::wstring emptyStr = g_CurrentLanguage == LANG_JA ? L"物理ドライブが見つかりません (管理者として実行してください)" :
                               (g_CurrentLanguage == LANG_ZH ? L"未检测到物理驱动器 (请以管理员身份运行)" :
                                                              L"No Physical Drives Found (Run as Admin)");
        SendMessageW(g_hwndDriveCombo, CB_ADDSTRING, 0, (LPARAM)emptyStr.c_str());
    } else {
        for (const auto& drive : g_Drives) {
            wchar_t itemText[256];
            std::wstring drivePrefix = g_CurrentLanguage == LANG_JA ? L"ドライブ" :
                                       (g_CurrentLanguage == LANG_ZH ? L"驱动器" : L"Drive");
            swprintf_s(itemText, L"%s %d: %s (%.1f GB)", 
                drivePrefix.c_str(),
                drive.index, 
                drive.model.c_str(), 
                (double)drive.sizeBytes / (1024.0 * 1024.0 * 1024.0));
            SendMessageW(g_hwndDriveCombo, CB_ADDSTRING, 0, (LPARAM)itemText);
        }
        SendMessageW(g_hwndDriveCombo, CB_SETCURSEL, curDriveSel == CB_ERR ? 0 : curDriveSel, 0);
    }
    
    // 4. Force full window redraw to update GDI+ text and panels
    InvalidateRect(g_hwndMain, NULL, FALSE);
}

