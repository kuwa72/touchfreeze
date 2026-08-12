// MonitorDlg.cpp - Real-time Diagnostic & Input Monitor Dialog for TouchFreeze
#include "stdafx.h"
#include "MonitorDlg.h"
#include "TouchGesture.h"
#include "..\HookDll\HookDll.h"
#include "resource.h"
#include <stdio.h>
#include <tchar.h>

static WNDPROC g_pOldMonitorPreviewProc = NULL;
static int     g_LastSyncedLogCount = -1;

static LRESULT CALLBACK MonitorPreviewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fill background
        HBRUSH hbgBrush = CreateSolidBrush(RGB(235, 238, 245));
        FillRect(hdc, &rc, hbgBrush);
        DeleteObject(hbgBrush);

        // Draw touchpad outer frame
        HPEN hFramePen = CreatePen(PS_SOLID, 2, RGB(160, 160, 180));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hFramePen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

        RoundRect(hdc, rc.left + 4, rc.top + 4, rc.right - 4, rc.bottom - 4, 10, 10);

        int padLeft   = rc.left + 6;
        int padTop    = rc.top + 6;
        int padWidth  = (rc.right - 4) - padLeft;
        int padHeight = (rc.bottom - 4) - padTop;

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

        if (hasZone)
        {
            HBRUSH hZoneBrush = CreateSolidBrush(RGB(200, 225, 255));
            FillRect(hdc, &rcZone, hZoneBrush);
            DeleteObject(hZoneBrush);

            HPEN hZoneBorder = CreatePen(PS_DOT, 1, RGB(90, 140, 220));
            SelectObject(hdc, hZoneBorder);
            Rectangle(hdc, rcZone.left, rcZone.top, rcZone.right, rcZone.bottom);
            DeleteObject(hZoneBorder);
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

        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hFramePen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    return CallWindowProc(g_pOldMonitorPreviewProc, hwnd, uMsg, wParam, lParam);
}

static void RefreshMonitorUI(HWND hWnd)
{
    TouchDiagInfo diag;
    TouchGesture_GetDiagInfo(&diag);

    TCHAR szInfo[512];
    _stprintf_s(szInfo, sizeof(szInfo)/sizeof(TCHAR),
        _T("State Machine: %s  |  Blocked: %s  |  Right Drag: %s\r\n")
        _T("Last Key: VK 0x%02X (%u), Key Elapsed: %lu ms (Timeout: %lu ms, Cooldown: %lu ms)\r\n")
        _T("Touch Pos Ratio: X=%.1f%%, Y=%.1f%% (Active: %s, Touch Elapsed: %lu ms)\r\n")
        _T("Touch Feature: DeltaDist=%.3f, Contacts=%d, Confidence=%s, 1-Finger Allowed=%s\r\n")
        _T("Raw Input: Raw X=%lu, Y=%lu (Min/Max X:[%lu..%lu], Y:[%lu..%lu]) | hDev: 0x%p"),
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
        diag.bAllowSingleFingerMove ? _T("YES") : _T("NO"),
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
            HWND hPreview = GetDlgItem(hWnd, IDC_MONITOR_PREVIEW);
            if (hPreview)
            {
                g_pOldMonitorPreviewProc = (WNDPROC)SetWindowLongPtr(hPreview, GWLP_WNDPROC, (LONG_PTR)MonitorPreviewProc);
            }
            g_LastSyncedLogCount = -1;
            SetTimer(hWnd, 102, 50, NULL); // 50ms refresh rate
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
        case IDOK:
        case IDCANCEL:
            KillTimer(hWnd, 102);
            EndDialog(hWnd, LOWORD(wParam));
            break;
        }
        break;

    case WM_CLOSE:
        KillTimer(hWnd, 102);
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
        isMonitorVisible = FALSE;
    }
}


