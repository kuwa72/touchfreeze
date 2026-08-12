// Copyright (C) 2007-2013 Ivan Zhakov.
#include "stdafx.h"
#include "AboutDlg.h"
#include "KeyHookTest.h"
#include "TouchGesture.h"
#include "..\HookDll\HookDll.h"
#include "resource.h"

static WNDPROC g_pOldPreviewProc = NULL;

static LRESULT CALLBACK TouchpadPreviewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fill background
        HBRUSH hbgBrush = CreateSolidBrush(RGB(240, 240, 245));
        FillRect(hdc, &rc, hbgBrush);
        DeleteObject(hbgBrush);

        // Draw touchpad outer frame
        HPEN hFramePen = CreatePen(PS_SOLID, 2, RGB(180, 180, 190));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hFramePen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

        RoundRect(hdc, rc.left + 4, rc.top + 4, rc.right - 4, rc.bottom - 4, 12, 12);

        int padLeft   = rc.left + 6;
        int padTop    = rc.top + 6;
        int padWidth  = (rc.right - 4) - padLeft;
        int padHeight = (rc.bottom - 4) - padTop;

        int zoneMode = TouchGesture_GetZoneMode();

        // Highlight selected zone
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

            HPEN hZoneBorder = CreatePen(PS_DOT, 1, RGB(100, 150, 220));
            SelectObject(hdc, hZoneBorder);
            Rectangle(hdc, rcZone.left, rcZone.top, rcZone.right, rcZone.bottom);
            DeleteObject(hZoneBorder);
        }

        // Draw Touch position if active
        double rx = -1.0, ry = -1.0;
        BOOL bActiveTouch = TouchGesture_GetLastRatio(&rx, &ry);

        if (bActiveTouch && rx >= 0.0 && ry >= 0.0)
        {
            int cx = padLeft + (int)(padWidth * rx);
            int cy = padTop  + (int)(padHeight * ry);

            HBRUSH hTouchBrush = CreateSolidBrush(RGB(255, 50, 50));
            HPEN hTouchPen = CreatePen(PS_SOLID, 2, RGB(200, 0, 0));

            SelectObject(hdc, hTouchBrush);
            SelectObject(hdc, hTouchPen);

            Ellipse(hdc, cx - 8, cy - 8, cx + 8, cy + 8);

            DeleteObject(hTouchBrush);
            DeleteObject(hTouchPen);
        }

        // Status Overlay Text
        SetBkMode(hdc, TRANSPARENT);
        if (TFHookIsBlocked())
        {
            SetTextColor(hdc, RGB(220, 100, 0));
            TextOut(hdc, padLeft + 10, padTop + 10, _T("[KEYBOARD INPUT - BLOCKED]"), 26);
        }
        else if (TouchGesture_IsDragging())
        {
            SetTextColor(hdc, RGB(200, 0, 0));
            TextOut(hdc, padLeft + 10, padTop + 10, _T("[RIGHT DRAGGING ACTIVE]"), 23);
        }
        else
        {
            SetTextColor(hdc, RGB(100, 100, 100));
            TextOut(hdc, padLeft + 10, padTop + 10, _T("Touchpad Ready"), 14);
        }

        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hFramePen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    return CallWindowProc(g_pOldPreviewProc, hwnd, uMsg, wParam, lParam);
}

static BOOL CALLBACK AboutDlgProc(
   HWND hWnd,
   UINT uMsg,
   WPARAM wParam,
   LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        {
            HWND hPreview = GetDlgItem(hWnd, IDC_TOUCHPAD_PREVIEW);
            if (hPreview)
            {
                g_pOldPreviewProc = (WNDPROC)SetWindowLongPtr(hPreview, GWLP_WNDPROC, (LONG_PTR)TouchpadPreviewProc);
            }
            SetTimer(hWnd, 101, 50, NULL); // 50ms refresh timer for live test
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == 101)
        {
            HWND hPreview = GetDlgItem(hWnd, IDC_TOUCHPAD_PREVIEW);
            if (hPreview)
            {
                InvalidateRect(hPreview, NULL, FALSE);
            }

            double rx = 0, ry = 0;
            BOOL bTouch = TouchGesture_GetLastRatio(&rx, &ry);
            BOOL bDrag = TouchGesture_IsDragging();
            BOOL bBlocked = TFHookIsBlocked();

            TCHAR szBuf[128];
            if (bBlocked)
            {
                _stprintf_s(szBuf, _T("Status: Typing (Touch Blocked) | Zone: %s"), 
                    bTouch ? _T("Touched") : _T("Idle"));
            }
            else if (bDrag)
            {
                _stprintf_s(szBuf, _T("Status: *** RIGHT DRAG IN PROGRESS *** (X:%.0f%% Y:%.0f%%)"), 
                    rx * 100.0, ry * 100.0);
            }
            else if (bTouch)
            {
                _stprintf_s(szBuf, _T("Status: Touching Touchpad (X:%.0f%% Y:%.0f%%)"), 
                    rx * 100.0, ry * 100.0);
            }
            else
            {
                _stprintf_s(szBuf, _T("Status: Waiting for touchpad touch..."));
            }
            SetDlgItemText(hWnd, IDC_STATUS_TEXT, szBuf);
        }
        break;

    case WM_CLOSE:
        KillTimer(hWnd, 101);
        EndDialog(hWnd, IDCANCEL);
        break;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
            KillTimer(hWnd, 101);
            EndDialog(hWnd, IDOK);
            break;
        }
        break;
    }
    return FALSE;
}

void ShowAboutDlg(HINSTANCE hInst, HWND hwnd)
{
    static BOOL isDialogVisible = FALSE;
    
    if ( isDialogVisible == FALSE )
    {
      isDialogVisible = TRUE;
      DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
      isDialogVisible = FALSE;
    }
}
