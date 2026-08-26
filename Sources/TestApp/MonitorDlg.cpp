// MonitorDlg.cpp - Real-time Diagnostic & Input Monitor Dialog for TouchFreeze
#include "stdafx.h"
#include "MonitorDlg.h"
#include "TouchGesture.h"
#include "..\HookDll\HookDll.h"
#include "resource.h"
#include <stdio.h>
#include <tchar.h>

#include "KeyHookTest.h"
#include <windowsx.h>

static WNDPROC g_pOldMonitorPreviewProc = NULL;
static int     g_LastSyncedLogCount = -1;
static BOOL    g_bMonitorDraggingArea = FALSE;
static POINT   g_ptMonitorDragStart = { 0, 0 };
static POINT   g_ptMonitorDragCurrent = { 0, 0 };

static LRESULT CALLBACK MonitorPreviewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int padLeft   = rc.left + 6;
    int padTop    = rc.top + 6;
    int padRight  = rc.right - 6;
    int padBottom = rc.bottom - 6;
    int padWidth  = padRight - padLeft;
    int padHeight = padBottom - padTop;

    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        {
            g_bMonitorDraggingArea = TRUE;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            x = max(padLeft, min(padRight, x));
            y = max(padTop, min(padBottom, y));
            g_ptMonitorDragStart.x = x;
            g_ptMonitorDragStart.y = y;
            g_ptMonitorDragCurrent = g_ptMonitorDragStart;
            SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (g_bMonitorDraggingArea)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            x = max(padLeft, min(padRight, x));
            y = max(padTop, min(padBottom, y));
            g_ptMonitorDragCurrent.x = x;
            g_ptMonitorDragCurrent.y = y;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_bMonitorDraggingArea)
        {
            g_bMonitorDraggingArea = FALSE;
            ReleaseCapture();

            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            x = max(padLeft, min(padRight, x));
            y = max(padTop, min(padBottom, y));
            g_ptMonitorDragCurrent.x = x;
            g_ptMonitorDragCurrent.y = y;

            int x1 = min(g_ptMonitorDragStart.x, g_ptMonitorDragCurrent.x);
            int x2 = max(g_ptMonitorDragStart.x, g_ptMonitorDragCurrent.x);
            int y1 = min(g_ptMonitorDragStart.y, g_ptMonitorDragCurrent.y);
            int y2 = max(g_ptMonitorDragStart.y, g_ptMonitorDragCurrent.y);

            if ((x2 - x1) >= 8 && (y2 - y1) >= 8 && padWidth > 0 && padHeight > 0)
            {
                double minX = (double)(x1 - padLeft) / (double)padWidth;
                double maxX = (double)(x2 - padLeft) / (double)padWidth;
                double minY = (double)(y1 - padTop) / (double)padHeight;
                double maxY = (double)(y2 - padTop) / (double)padHeight;

                minX = max(0.0, min(1.0, minX));
                maxX = max(0.0, min(1.0, maxX));
                minY = max(0.0, min(1.0, minY));
                maxY = max(0.0, min(1.0, maxY));

                TouchGesture_SetCustomZone(minX, maxX, minY, maxY);
                TouchGesture_SetZoneMode(PAD_ZONE_CUSTOM);
                g_CurrentRightDragZone = PAD_ZONE_CUSTOM;
                SaveCustomZoneParams(minX, maxX, minY, maxY);
                SaveRightDragZone(PAD_ZONE_CUSTOM);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_RBUTTONUP:
        TouchGesture_SetZoneMode(PAD_ZONE_DISABLED);
        g_CurrentRightDragZone = PAD_ZONE_DISABLED;
        SaveRightDragZone(PAD_ZONE_DISABLED);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Fill background
            HBRUSH hbgBrush = CreateSolidBrush(RGB(235, 238, 245));
            FillRect(hdc, &rc, hbgBrush);
            DeleteObject(hbgBrush);

            // Draw touchpad outer frame
            HPEN hFramePen = CreatePen(PS_SOLID, 2, RGB(160, 160, 180));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hFramePen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

            RoundRect(hdc, rc.left + 4, rc.top + 4, rc.right - 4, rc.bottom - 4, 10, 10);

            int zoneMode = TouchGesture_GetZoneMode();
            RECT rcZone = { 0 };
            BOOL hasZone = TRUE;

            switch (zoneMode)
            {
            case PAD_ZONE_RIGHT_THIRD:
                rcZone.left   = padLeft + (int)(padWidth * 0.70);
                rcZone.top    = padTop;
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            case PAD_ZONE_RIGHT_HALF:
                rcZone.left   = padLeft + (int)(padWidth * 0.50);
                rcZone.top    = padTop;
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            case PAD_ZONE_TOP_RIGHT:
                rcZone.left   = padLeft + (int)(padWidth * 0.70);
                rcZone.top    = padTop;
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + (int)(padHeight * 0.50);
                break;
            case PAD_ZONE_BOTTOM_RIGHT:
                rcZone.left   = padLeft + (int)(padWidth * 0.70);
                rcZone.top    = padTop + (int)(padHeight * 0.50);
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            case PAD_ZONE_RIGHT_QUARTER:
                rcZone.left   = padLeft + (int)(padWidth * 0.75);
                rcZone.top    = padTop;
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            case PAD_ZONE_RIGHT_FIFTH:
                rcZone.left   = padLeft + (int)(padWidth * 0.80);
                rcZone.top    = padTop;
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            case PAD_ZONE_BOTTOM_RIGHT_CORNER:
                rcZone.left   = padLeft + (int)(padWidth * 0.80);
                rcZone.top    = padTop + (int)(padHeight * 0.65);
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            case PAD_ZONE_CUSTOM:
                {
                    double minX, maxX, minY, maxY;
                    TouchGesture_GetCustomZone(&minX, &maxX, &minY, &maxY);
                    rcZone.left   = padLeft + (int)(padWidth * minX);
                    rcZone.top    = padTop  + (int)(padHeight * minY);
                    rcZone.right  = padLeft + (int)(padWidth * maxX);
                    rcZone.bottom = padTop  + (int)(padHeight * maxY);
                }
                break;
            case PAD_ZONE_ANYWHERE:
                rcZone.left   = padLeft;
                rcZone.top    = padTop;
                rcZone.right  = padLeft + padWidth;
                rcZone.bottom = padTop + padHeight;
                break;
            default:
                hasZone = FALSE;
                break;
            }

            if (hasZone && !g_bMonitorDraggingArea)
            {
                HBRUSH hZoneBrush = CreateSolidBrush(RGB(200, 225, 255));
                FillRect(hdc, &rcZone, hZoneBrush);
                DeleteObject(hZoneBrush);

                HPEN hZoneBorder = CreatePen(PS_DOT, 1, RGB(90, 140, 220));
                SelectObject(hdc, hZoneBorder);
                Rectangle(hdc, rcZone.left, rcZone.top, rcZone.right, rcZone.bottom);
                DeleteObject(hZoneBorder);
            }

            if (g_bMonitorDraggingArea)
            {
                int x1 = min(g_ptMonitorDragStart.x, g_ptMonitorDragCurrent.x);
                int x2 = max(g_ptMonitorDragStart.x, g_ptMonitorDragCurrent.x);
                int y1 = min(g_ptMonitorDragStart.y, g_ptMonitorDragCurrent.y);
                int y2 = max(g_ptMonitorDragStart.y, g_ptMonitorDragCurrent.y);

                x1 = max(padLeft, min(padRight, x1));
                x2 = max(padLeft, min(padRight, x2));
                y1 = max(padTop, min(padBottom, y1));
                y2 = max(padTop, min(padBottom, y2));

                RECT rcDrag;
                rcDrag.left   = x1;
                rcDrag.right  = x2;
                rcDrag.top    = y1;
                rcDrag.bottom = y2;

                HBRUSH hDragBrush = CreateSolidBrush(RGB(180, 240, 200));
                FillRect(hdc, &rcDrag, hDragBrush);
                DeleteObject(hDragBrush);

                HPEN hDragPen = CreatePen(PS_SOLID, 2, RGB(40, 180, 80));
                SelectObject(hdc, hDragPen);
                Rectangle(hdc, rcDrag.left, rcDrag.top, rcDrag.right, rcDrag.bottom);
                DeleteObject(hDragPen);
            }

            // Draw active touch coordinates
            TouchDiagInfo diag;
            TouchGesture_GetDiagInfo(&diag);

            if (diag.bTouchActive && diag.fRatioX >= 0.0 && diag.fRatioY >= 0.0)
            {
                int cx = padLeft + (int)(padWidth * diag.fRatioX);
                int cy = padTop  + (int)(padHeight * diag.fRatioY);

                COLORREF touchColor = diag.bDragging ? RGB(220, 30, 30) : RGB(40, 160, 40);
                HBRUSH hTouchBrush = CreateSolidBrush(touchColor);
                HPEN hTouchPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));

                SelectObject(hdc, hTouchBrush);
                SelectObject(hdc, hTouchPen);
                Ellipse(hdc, cx - 7, cy - 7, cx + 7, cy + 7);

                DeleteObject(hTouchBrush);
                DeleteObject(hTouchPen);
            }

            SetBkMode(hdc, TRANSPARENT);
            if (diag.bBlocked)
            {
                SetTextColor(hdc, RGB(220, 80, 0));
                TextOut(hdc, padLeft + 6, padTop + 6, _T("[BLOCKED]"), 9);
            }
            else if (diag.bDragging)
            {
                SetTextColor(hdc, RGB(200, 0, 0));
                TextOut(hdc, padLeft + 6, padTop + 6, _T("[RIGHT DRAGGING]"), 16);
            }
            else
            {
                SetTextColor(hdc, RGB(80, 120, 160));
                TextOut(hdc, padLeft + 6, padTop + 6, _T("Drag mouse here to set Custom Area"), 34);
            }

            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hFramePen);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return CallWindowProc(g_pOldMonitorPreviewProc, hwnd, uMsg, wParam, lParam);
}

static void RefreshMonitorUI(HWND hWnd)
{
    TouchDiagInfo diag;
    TouchGesture_GetDiagInfo(&diag);

    LPCTSTR szPtpStatus = (diag.hDevice != NULL && diag.maxX > diag.minX) ? 
        _T("PTP Active (Precision Touchpad Detected)") : 
        _T("Legacy / Fallback Mode (Keyboard Hook Only)");

    TCHAR szInfo[600];
    _stprintf_s(szInfo, sizeof(szInfo)/sizeof(TCHAR),
        _T("Touchpad Device: %s\r\n")
        _T("State Machine: %s  |  Blocked: %s  |  Right Drag: %s\r\n")
        _T("Last Key: VK 0x%02X (%u), Key Elapsed: %lu ms (Timeout: %lu ms, Cooldown: %lu ms)\r\n")
        _T("Touch Pos Ratio: X=%.1f%%, Y=%.1f%% (Active: %s, Touch Elapsed: %lu ms)\r\n")
        _T("Touch Feature: DeltaDist=%.3f, Contacts=%d, Confidence=%s, Freeze Cursor=%s\r\n")
        _T("Raw Input: Raw X=%lu, Y=%lu (Min/Max X:[%lu..%lu], Y:[%lu..%lu]) | hDev: 0x%p"),
        szPtpStatus,
        TouchGesture_GetStateName(diag.state),
        diag.bBlocked ? _T("YES") : _T("NO"),
        diag.bDragging ? _T("ACTIVE") : _T("OFF"),
        diag.nLastVkCode, diag.nLastVkCode,
        diag.dwElapsedKeyMs < 999999 ? diag.dwElapsedKeyMs : 0,
        diag.dwTypingTimeoutMs, diag.dwCooldownMs,
        diag.fRatioX >= 0 ? diag.fRatioX * 100.0 : 0.0,
        diag.fRatioY >= 0 ? diag.fRatioY * 100.0 : 0.0,
        diag.bTouchActive ? _T("YES") : _T("NO"),
        diag.dwElapsedTouchMs < 999999 ? diag.dwElapsedTouchMs : 0,
        diag.fDeltaDist, diag.nContactCount,
        diag.bConfidence ? _T("TRUE") : _T("FALSE"),
        diag.bFreezeCursor ? _T("YES") : _T("NO"),
        diag.rawX, diag.rawY, diag.minX, diag.maxX, diag.minY, diag.maxY,
        diag.hDevice);

    SetDlgItemText(hWnd, IDC_TXT_DIAG_INFO, szInfo);


    // Refresh Visual Preview
    HWND hPreview = GetDlgItem(hWnd, IDC_MONITOR_PREVIEW);
    if (hPreview)
    {
        InvalidateRect(hPreview, NULL, FALSE);
    }

    // Refresh Log ListBox
    HWND hList = GetDlgItem(hWnd, IDC_LOG_LIST);
    if (hList)
    {
        int currentCount = TouchGesture_GetLogCount();
        if (currentCount != g_LastSyncedLogCount)
        {
            SendMessage(hList, LB_RESETCONTENT, 0, 0);
            TCHAR szItem[256];
            for (int i = 0; i < currentCount; i++)
            {
                if (TouchGesture_GetLogItem(i, szItem, 256))
                {
                    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)szItem);
                }
            }
            SendMessage(hList, LB_SETCURSEL, currentCount - 1, 0);
            g_LastSyncedLogCount = currentCount;
        }
    }
}

HWND g_hMonitorDlg = NULL;

static INT_PTR CALLBACK MonitorDlgProc(
   HWND hWnd,
   UINT uMsg,
   WPARAM wParam,
   LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        {
            g_hMonitorDlg = hWnd;
            HWND hPreview = GetDlgItem(hWnd, IDC_MONITOR_PREVIEW);
            if (hPreview)
            {
                g_pOldMonitorPreviewProc = (WNDPROC)SetWindowLongPtr(hPreview, GWLP_WNDPROC, (LONG_PTR)MonitorPreviewProc);
            }
            g_LastSyncedLogCount = -1;
            SetTimer(hWnd, 102, 80, NULL); // 80ms refresh rate
            RefreshMonitorUI(hWnd);

            // Bring dialog to front and focus to prevent clicks passing through to background windows
            SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(hWnd);
            SetFocus(hWnd);
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == 102)
        {
            RefreshMonitorUI(hWnd);
        }
        break;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDC_BTN_CLEAR_LOG:
            TouchGesture_ClearLog();
            g_LastSyncedLogCount = -1;
            RefreshMonitorUI(hWnd);
            break;
        case IDC_BTN_DISABLE_ZONE:
            TouchGesture_SetZoneMode(PAD_ZONE_DISABLED);
            g_CurrentRightDragZone = PAD_ZONE_DISABLED;
            SaveRightDragZone(PAD_ZONE_DISABLED);
            RefreshMonitorUI(hWnd);
            break;
        case IDOK:
        case IDCANCEL:
            KillTimer(hWnd, 102);
            g_hMonitorDlg = NULL;
            EndDialog(hWnd, LOWORD(wParam));
            break;
        }
        break;

    case WM_CLOSE:
        KillTimer(hWnd, 102);
        g_hMonitorDlg = NULL;
        EndDialog(hWnd, IDCANCEL);
        break;
    }
    return FALSE;
}

void ShowMonitorDlg(HINSTANCE hInst, HWND hwndParent)
{
    static BOOL isMonitorVisible = FALSE;
    if (!isMonitorVisible)
    {
        isMonitorVisible = TRUE;
        // Pass NULL instead of HWND_MESSAGE to allow OS proper Z-order and focus handling
        INT_PTR nRet = DialogBox(hInst, MAKEINTRESOURCE(IDD_MONITOR), NULL, MonitorDlgProc);
        if (nRet == -1)
        {
            DWORD dwErr = GetLastError();
            TCHAR szErr[256];
            _stprintf_s(szErr, _T("Failed to create Monitor dialog. Error Code: %lu"), dwErr);
            MessageBox(NULL, szErr, _T("TouchFreeze Diagnostic Error"), MB_OK | MB_ICONERROR);
        }
        g_hMonitorDlg = NULL;
        isMonitorVisible = FALSE;
    }
}



