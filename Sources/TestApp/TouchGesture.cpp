// TouchGesture.cpp - Physical Touchpad area recognition for TouchFreeze
#include "stdafx.h"
#include "TouchGesture.h"
#include "..\HookDll\HookDll.h"
#include <windows.h>
#include <hidusage.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <math.h>


#pragma comment(lib, "User32.lib")
#pragma comment(lib, "hid.lib")

static BOOL                 g_bEnabled = TRUE;
static TouchpadZoneMode     g_ZoneMode = PAD_ZONE_RIGHT_THIRD;
static double               g_CustomMinX = 0.80;
static double               g_CustomMaxX = 1.00;
static double               g_CustomMinY = 0.65;
static double               g_CustomMaxY = 1.00;
static TouchGestureState    g_GestureState = GESTURE_STATE_IDLE;
static HWND                 g_hWndTarget = NULL;

static BOOL                 g_bTouchActive = FALSE;
static BOOL                 g_bInvalidStroke = FALSE;
static BOOL                 g_bRightDragLatched = FALSE;
static DWORD                g_LastTouchTime = 0;
static DWORD                g_InitTime = 0;
static double               g_LastRatioX = -1.0;
static double               g_LastRatioY = -1.0;
#define TOUCH_RELEASE_TIMEOUT 150 // 150ms for responsive release

#define MAX_LOG_ENTRIES 100
static TCHAR g_LogEntries[MAX_LOG_ENTRIES][256];
static int   g_LogCount = 0;
static int   g_LogHead = 0;

static PalmRejectionState g_PalmState = PALM_STATE_IDLE;
static ULONG g_RawX = 0, g_RawY = 0;
static ULONG g_MinX = 0, g_MaxX = 0, g_MinY = 0, g_MaxY = 0;
static HANDLE g_hLastDevice = NULL;
static TCHAR  g_szLastDeviceName[256] = _T("None");
static int    g_ContactCount = 1;
static BOOL   g_bConfidence = TRUE;

static DWORD  g_TypingTimeoutMs = 500;
static DWORD  g_CooldownMs = 500;
static double g_LastDeltaX = 0.0;
static double g_LastDeltaY = 0.0;
static double g_LastDeltaDist = 0.0;

extern HWND g_hMonitorDlg;

void TouchGesture_AddLog(LPCTSTR format, ...)
{
    if (!g_hMonitorDlg || !IsWindow(g_hMonitorDlg))
        return;

    TCHAR szMsg[256];

    va_list args;
    va_start(args, format);
    _vstprintf_s(szMsg, sizeof(szMsg)/sizeof(TCHAR), format, args);
    va_end(args);

    DWORD now = GetTickCount();
    TCHAR szTimedMsg[256];
    _stprintf_s(szTimedMsg, sizeof(szTimedMsg)/sizeof(TCHAR), _T("[%08lu] %s"), now % 1000000, szMsg);

    int idx = (g_LogHead + g_LogCount) % MAX_LOG_ENTRIES;
    if (g_LogCount < MAX_LOG_ENTRIES)
    {
        g_LogCount++;
    }
    else
    {
        g_LogHead = (g_LogHead + 1) % MAX_LOG_ENTRIES;
    }
    _tcscpy_s(g_LogEntries[idx], 256, szTimedMsg);
}


static void SendRightButtonInput(DWORD dwFlags)
{
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = dwFlags;
    input.mi.dwExtraInfo = TF_GESTURE_EXTRA_INFO;
    SendInput(1, &input, sizeof(INPUT));
}

void TouchGesture_SetEnabled(BOOL bEnable)
{
    g_bEnabled = bEnable;
    if (!bEnable && g_bRightDragLatched)
    {
        SendRightButtonInput(MOUSEEVENTF_RIGHTUP);
        TFHookSetRightDragActive(FALSE);
        g_bRightDragLatched = FALSE;
        g_bTouchActive = FALSE;
        g_bInvalidStroke = FALSE;
        g_GestureState = GESTURE_STATE_IDLE;
    }
}

BOOL TouchGesture_IsEnabled()
{
    return g_bEnabled;
}

void TouchGesture_SetZoneMode(int mode)
{
    g_ZoneMode = (TouchpadZoneMode)mode;
    if (g_ZoneMode == PAD_ZONE_DISABLED && g_bRightDragLatched)
    {
        SendRightButtonInput(MOUSEEVENTF_RIGHTUP);
        TFHookSetRightDragActive(FALSE);
        g_bRightDragLatched = FALSE;
        g_bTouchActive = FALSE;
        g_bInvalidStroke = FALSE;
        g_GestureState = GESTURE_STATE_IDLE;
    }
}

int TouchGesture_GetZoneMode()
{
    return (int)g_ZoneMode;
}

void TouchGesture_SetCustomZone(double minX, double maxX, double minY, double maxY)
{
    g_CustomMinX = (minX < 0.0) ? 0.0 : ((minX > 1.0) ? 1.0 : minX);
    g_CustomMaxX = (maxX < 0.0) ? 0.0 : ((maxX > 1.0) ? 1.0 : maxX);
    g_CustomMinY = (minY < 0.0) ? 0.0 : ((minY > 1.0) ? 1.0 : minY);
    g_CustomMaxY = (maxY < 0.0) ? 0.0 : ((maxY > 1.0) ? 1.0 : maxY);
    if (g_CustomMinX > g_CustomMaxX) { double tmp = g_CustomMinX; g_CustomMinX = g_CustomMaxX; g_CustomMaxX = tmp; }
    if (g_CustomMinY > g_CustomMaxY) { double tmp = g_CustomMinY; g_CustomMinY = g_CustomMaxY; g_CustomMaxY = tmp; }
}

void TouchGesture_GetCustomZone(double *pMinX, double *pMaxX, double *pMinY, double *pMaxY)
{
    if (pMinX) *pMinX = g_CustomMinX;
    if (pMaxX) *pMaxX = g_CustomMaxX;
    if (pMinY) *pMinY = g_CustomMinY;
    if (pMaxY) *pMaxY = g_CustomMaxY;
}

BOOL TouchGesture_IsDragging()
{
    return g_bRightDragLatched;
}

BOOL TouchGesture_GetLastRatio(double *pRatioX, double *pRatioY)
{
    if (pRatioX) *pRatioX = g_LastRatioX;
    if (pRatioY) *pRatioY = g_LastRatioY;
    return (g_LastTouchTime > 0 && (GetTickCount() - g_LastTouchTime < 500));
}

static bool IsInPhysicalZone(double ratioX, double ratioY)
{
    if (g_ZoneMode == PAD_ZONE_DISABLED)
        return false;
    if (g_ZoneMode == PAD_ZONE_ANYWHERE)
        return true;

    switch (g_ZoneMode)
    {
    case PAD_ZONE_RIGHT_THIRD:
        return (ratioX >= 0.70); // Right 30% area

    case PAD_ZONE_RIGHT_HALF:
        return (ratioX >= 0.50); // Right half area

    case PAD_ZONE_TOP_RIGHT:
        return (ratioX >= 0.70) && (ratioY <= 0.50); // Top-Right quarter

    case PAD_ZONE_BOTTOM_RIGHT:
        return (ratioX >= 0.70) && (ratioY >= 0.50); // Bottom-Right quarter

    case PAD_ZONE_RIGHT_QUARTER:
        return (ratioX >= 0.75); // Right 25% area

    case PAD_ZONE_RIGHT_FIFTH:
        return (ratioX >= 0.80); // Right 20% area

    case PAD_ZONE_BOTTOM_RIGHT_CORNER:
        return (ratioX >= 0.80) && (ratioY >= 0.65); // Bottom-Right Corner (Right 20%, Bottom 35%)

    case PAD_ZONE_CUSTOM:
        return (ratioX >= g_CustomMinX && ratioX <= g_CustomMaxX &&
                ratioY >= g_CustomMinY && ratioY <= g_CustomMaxY);

    default:
        break;
    }

    return false;
}

static void CheckTouchRelease()
{
    DWORD now = GetTickCount();
    if (g_LastTouchTime > 0 && (now - g_LastTouchTime > TOUCH_RELEASE_TIMEOUT))
    {
        if (g_bRightDragLatched)
        {
            g_bRightDragLatched = FALSE;
            g_GestureState = GESTURE_STATE_IDLE;
            SendRightButtonInput(MOUSEEVENTF_RIGHTUP);
            TFHookSetRightDragActive(FALSE);
            OutputDebugString(_T("[TouchFreeze] Physical Touch Ended: Right Drag Released\n"));
            TouchGesture_AddLog(_T("Touch Ended -> Right Drag Released"));
        }
        g_bTouchActive = FALSE;
        g_bInvalidStroke = FALSE;
    }
    UpdatePalmState();
}

BOOL TouchGesture_Init(HWND hWnd)
{
    g_hWndTarget = hWnd;
    g_GestureState = GESTURE_STATE_IDLE;
    g_bRightDragLatched = FALSE;
    g_bTouchActive = FALSE;
    g_bInvalidStroke = FALSE;
    g_InitTime = GetTickCount();
    g_LastTouchTime = g_InitTime;

    RAWINPUTDEVICE rid[2];
    
    // TouchPad
    rid[0].usUsagePage = HID_USAGE_PAGE_DIGITIZER; // 0x0D
    rid[0].usUsage     = HID_USAGE_DIGITIZER_TOUCH_PAD; // 0x05
    rid[0].dwFlags     = RIDEV_INPUTSINK;
    rid[0].hwndTarget  = hWnd;

    // TouchScreen / Multi-touch
    rid[1].usUsagePage = HID_USAGE_PAGE_DIGITIZER; // 0x0D
    rid[1].usUsage     = HID_USAGE_DIGITIZER_TOUCH_SCREEN; // 0x04
    rid[1].dwFlags     = RIDEV_INPUTSINK;
    rid[1].hwndTarget  = hWnd;

    if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
    {
        OutputDebugString(_T("[TouchFreeze] RegisterRawInputDevices failed\n"));
        return FALSE;
    }

    SetTimer(hWnd, 99, 50, NULL); // 50ms check timer for touch release

    OutputDebugString(_T("[TouchFreeze] TouchGesture_Init succeeded\n"));
    return TRUE;
}

struct TouchDevCache
{
    HANDLE hDevice;
    PHIDP_PREPARSED_DATA pPreparsedData;
    ULONG minX, maxX, minY, maxY;
    BOOL valid;
};

static TouchDevCache g_DevCache = { NULL, NULL, 0, 1, 0, 1, FALSE };

void TouchGesture_Uninit(HWND hWnd)
{
    KillTimer(hWnd, 99);
    if (g_bRightDragLatched)
    {
        SendRightButtonInput(MOUSEEVENTF_RIGHTUP);
        TFHookSetRightDragActive(FALSE);
    }
    g_bRightDragLatched = FALSE;
    g_bTouchActive = FALSE;
    g_bInvalidStroke = FALSE;
    g_GestureState = GESTURE_STATE_IDLE;

    RAWINPUTDEVICE rid[2];
    rid[0].usUsagePage = HID_USAGE_PAGE_DIGITIZER;
    rid[0].usUsage     = HID_USAGE_DIGITIZER_TOUCH_PAD;
    rid[0].dwFlags     = RIDEV_REMOVE;
    rid[0].hwndTarget  = NULL;

    rid[1].usUsagePage = HID_USAGE_PAGE_DIGITIZER;
    rid[1].usUsage     = HID_USAGE_DIGITIZER_TOUCH_SCREEN;
    rid[1].dwFlags     = RIDEV_REMOVE;
    rid[1].hwndTarget  = NULL;

    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
    g_hWndTarget = NULL;

    if (g_DevCache.pPreparsedData)
    {
        delete[] (BYTE*)g_DevCache.pPreparsedData;
        g_DevCache.pPreparsedData = NULL;
    }
    g_DevCache.valid = FALSE;
    g_DevCache.hDevice = NULL;
}

static TouchDevCache* GetDevCache(HANDLE hDevice)
{

    if (g_DevCache.valid && g_DevCache.hDevice == hDevice)
    {
        return &g_DevCache;
    }

    if (g_DevCache.pPreparsedData)
    {
        delete[] (BYTE*)g_DevCache.pPreparsedData;
        g_DevCache.pPreparsedData = NULL;
    }

    g_DevCache.hDevice = hDevice;
    g_DevCache.valid = FALSE;

    UINT preparsedSize = 0;
    if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, NULL, &preparsedSize) != 0 || preparsedSize == 0)
        return NULL;

    BYTE* pPreparsedBuffer = new BYTE[preparsedSize];
    PHIDP_PREPARSED_DATA pPreparsedData = (PHIDP_PREPARSED_DATA)pPreparsedBuffer;

    if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &preparsedSize) == (UINT)-1)
    {
        delete[] pPreparsedBuffer;
        return NULL;
    }

    HIDP_CAPS caps;
    if (HidP_GetCaps(pPreparsedData, &caps) == HIDP_STATUS_SUCCESS)
    {
        USHORT numValueCaps = caps.NumberInputValueCaps;
        PHIDP_VALUE_CAPS valueCaps = new HIDP_VALUE_CAPS[numValueCaps];

        if (HidP_GetValueCaps(HidP_Input, valueCaps, &numValueCaps, pPreparsedData) == HIDP_STATUS_SUCCESS)
        {
            ULONG minX = 0, maxX = 1, minY = 0, maxY = 1;
            for (USHORT i = 0; i < numValueCaps; i++)
            {
                if (valueCaps[i].UsagePage == 0x01) // Generic Desktop
                {
                    if (valueCaps[i].NotRange.Usage == 0x30) // X
                    {
                        minX = valueCaps[i].LogicalMin;
                        maxX = valueCaps[i].LogicalMax;
                    }
                    else if (valueCaps[i].NotRange.Usage == 0x31) // Y
                    {
                        minY = valueCaps[i].LogicalMin;
                        maxY = valueCaps[i].LogicalMax;
                    }
                }
            }
            g_DevCache.minX = minX;
            g_DevCache.maxX = maxX;
            g_DevCache.minY = minY;
            g_DevCache.maxY = maxY;
            g_DevCache.pPreparsedData = pPreparsedData;
            g_DevCache.valid = TRUE;
        }
        delete[] valueCaps;
    }

    if (!g_DevCache.valid)
    {
        delete[] pPreparsedBuffer;
        return NULL;
    }

    return &g_DevCache;
}

static void ProcessHIDRawInput(HANDLE hDevice, PRAWINPUT pRawInput)
{
    if (!g_bEnabled || g_ZoneMode == PAD_ZONE_DISABLED)
        return;

    DWORD now = GetTickCount();

    if (pRawInput->header.dwType != RIM_TYPEHID)
        return;

    TouchDevCache* pCache = GetDevCache(hDevice);
    if (!pCache || !pCache->valid)
        return;

    ULONG rawX = 0, rawY = 0;
    BOOL hasX = (HidP_GetUsageValue(HidP_Input, 0x01, 0, 0x30, &rawX, pCache->pPreparsedData, (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS);
    BOOL hasY = (HidP_GetUsageValue(HidP_Input, 0x01, 0, 0x31, &rawY, pCache->pPreparsedData, (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS);

    ULONG minX = pCache->minX, maxX = pCache->maxX;
    ULONG minY = pCache->minY, maxY = pCache->maxY;

    if (maxX > minX && maxY > minY && (hasX || hasY))
    {
        if (g_bTouchActive && rawX == g_RawX && rawY == g_RawY)
        {
            g_LastTouchTime = now;
            return;
        }

        g_LastTouchTime = now;
        g_RawX = rawX;
        g_RawY = rawY;
        g_MinX = minX;
        g_MaxX = maxX;
        g_MinY = minY;
        g_MaxY = maxY;
        g_hLastDevice = hDevice;

        double ratioX = (double)(rawX - minX) / (double)(maxX - minX);
        double ratioY = (double)(rawY - minY) / (double)(maxY - minY);
        
        double deltaX = (g_LastRatioX >= 0) ? (ratioX - g_LastRatioX) : 0.0;
        double deltaY = (g_LastRatioY >= 0) ? (ratioY - g_LastRatioY) : 0.0;
        double deltaDist = sqrt(deltaX * deltaX + deltaY * deltaY);
        g_LastDeltaX = deltaX;
        g_LastDeltaY = deltaY;
        g_LastDeltaDist = deltaDist;

        g_LastRatioX = ratioX;
        g_LastRatioY = ratioY;

        // Check First Landing Point for new touch stroke
        if (!g_bTouchActive)
        {
            g_bTouchActive = TRUE;
            if (IsInPhysicalZone(ratioX, ratioY) && !TouchGesture_ShouldBlockMouse())
            {
                g_bRightDragLatched = TRUE;
                g_bInvalidStroke = FALSE;
                g_GestureState = GESTURE_STATE_DRAGGING;
                SendRightButtonInput(MOUSEEVENTF_RIGHTDOWN);
                TFHookSetRightDragActive(TRUE);
                OutputDebugString(_T("[TouchFreeze] First touch landed in Zone: LATCHED & RIGHTDOWN\n"));
                TouchGesture_AddLog(_T("Touch Landed (In Zone X:%.2f, Y:%.2f) -> Right Down"), ratioX, ratioY);
            }
            else
            {
                g_bInvalidStroke = TRUE;
                g_bRightDragLatched = FALSE;
                OutputDebugString(_T("[TouchFreeze] First touch landed outside zone or during typing block: Stroke Invalidated\n"));
                TouchGesture_AddLog(_T("Touch Landed (X:%.2f, Y:%.2f) -> %s"), ratioX, ratioY,
                    TouchGesture_ShouldBlockMouse() ? _T("Blocked by Typing") : _T("Normal Touch"));
            }
        }
        else
        {
            if (!g_bInvalidStroke && !g_bRightDragLatched && IsInPhysicalZone(ratioX, ratioY))
            {
                g_bRightDragLatched = TRUE;
                g_GestureState = GESTURE_STATE_DRAGGING;
                SendRightButtonInput(MOUSEEVENTF_RIGHTDOWN);
                TFHookSetRightDragActive(TRUE);
                TouchGesture_AddLog(_T("Moved Into Zone -> Right Down"));
            }
        }
    }
}

BOOL TouchGesture_HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!g_bEnabled || g_ZoneMode == PAD_ZONE_DISABLED)
        return FALSE;

    switch (uMsg)
    {
    case WM_TIMER:
        if (wParam == 99)
        {
            CheckTouchRelease();
        }
        break;

    case WM_INPUT:
        {
            UINT dwSize = 0;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            if (dwSize > 0)
            {
                BYTE stackBuf[512];
                BYTE* lpb = (dwSize <= sizeof(stackBuf)) ? stackBuf : new BYTE[dwSize];
                if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
                {
                    PRAWINPUT raw = (PRAWINPUT)lpb;
                    ProcessHIDRawInput(raw->header.hDevice, raw);
                }
                if (lpb != stackBuf)
                {
                    delete[] lpb;
                }
            }
        }
        break;
    }

    return FALSE;
}

static void UpdatePalmState()
{
    DWORD now = GetTickCount();
    DWORD lastKey = TFHookGetLastKeyTime();
    DWORD keyElapsed = (lastKey > 0) ? (now - lastKey) : 999999;
    DWORD touchElapsed = (g_LastTouchTime > 0) ? (now - g_LastTouchTime) : 999999;

    PalmRejectionState prevState = g_PalmState;

    if (g_bRightDragLatched)
    {
        g_PalmState = PALM_STATE_TOUCH_DETECTED;
        return;
    }

    if (keyElapsed < g_TypingTimeoutMs)
    {
        // During active typing window, clicks/taps must be blocked (Palm Rejection Active)
        g_PalmState = PALM_STATE_BLOCKED;
    }
    else if (keyElapsed < (g_TypingTimeoutMs + g_CooldownMs))
    {
        // In cooldown window, block if multi-touch or low confidence is detected
        if (g_bTouchActive && (g_ContactCount > 1 || !g_bConfidence || g_LastDeltaDist > 0.35))
        {
            g_PalmState = PALM_STATE_BLOCKED;
        }
        else if (g_bTouchActive)
        {
            g_PalmState = PALM_STATE_TOUCH_DETECTED;
        }
        else
        {
            g_PalmState = PALM_STATE_COOLDOWN;
        }
    }
    else
    {
        if (g_bTouchActive)
        {
            g_PalmState = PALM_STATE_TOUCH_DETECTED;
        }
        else
        {
            g_PalmState = PALM_STATE_IDLE;
        }
    }

    if (g_PalmState != prevState)
    {
        TouchGesture_AddLog(_T("State Changed: %s -> %s"),
            TouchGesture_GetStateName(prevState),
            TouchGesture_GetStateName(g_PalmState));
    }

    TFHookSetOverrideBlocked(g_PalmState == PALM_STATE_BLOCKED);
}


void TouchGesture_SetPalmConfig(DWORD typingTimeoutMs, DWORD cooldownMs, BOOL bFreezeCursor)
{
    g_TypingTimeoutMs = typingTimeoutMs;
    g_CooldownMs = cooldownMs;
}

BOOL TouchGesture_ShouldBlockMouse()
{
    UpdatePalmState();
    return (g_PalmState == PALM_STATE_BLOCKED);
}

LPCTSTR TouchGesture_GetStateName(PalmRejectionState state)
{
    switch (state)
    {
    case PALM_STATE_IDLE:           return _T("Idle");
    case PALM_STATE_TYPING:         return _T("Typing");
    case PALM_STATE_TOUCH_DETECTED: return _T("TouchDetected");
    case PALM_STATE_BLOCKED:        return _T("Blocked");
    case PALM_STATE_RELEASED:       return _T("Released");
    case PALM_STATE_COOLDOWN:       return _T("Cooldown");
    default:                        return _T("Unknown");
    }
}

void TouchGesture_GetDiagInfo(TouchDiagInfo* pOut)
{
    if (!pOut) return;
    UpdatePalmState();
    DWORD now = GetTickCount();
    DWORD lastKey = TFHookGetLastKeyTime();

    pOut->state = g_PalmState;
    pOut->dwLastKeyTime = lastKey;
    pOut->dwElapsedKeyMs = (lastKey > 0) ? (now - lastKey) : 999999;
    pOut->nLastVkCode = TFHookGetLastVkCode();
    pOut->dwLastTouchTime = g_LastTouchTime;
    pOut->dwElapsedTouchMs = (g_LastTouchTime > 0) ? (now - g_LastTouchTime) : 999999;
    pOut->fRatioX = g_LastRatioX;
    pOut->fRatioY = g_LastRatioY;
    pOut->fDeltaX = g_LastDeltaX;
    pOut->fDeltaY = g_LastDeltaY;
    pOut->fDeltaDist = g_LastDeltaDist;
    pOut->rawX = g_RawX;
    pOut->rawY = g_RawY;
    pOut->minX = g_MinX;
    pOut->maxX = g_MaxX;
    pOut->minY = g_MinY;
    pOut->maxY = g_MaxY;
    pOut->hDevice = g_hLastDevice;
    _tcscpy_s(pOut->szDeviceName, 256, g_szLastDeviceName);
    pOut->bTouchActive = g_bTouchActive;
    pOut->bBlocked = (g_PalmState == PALM_STATE_BLOCKED);
    pOut->bDragging = g_bRightDragLatched;
    pOut->nContactCount = g_ContactCount;
    pOut->bConfidence = g_bConfidence;
    pOut->bFreezeCursor = TFHookGetFreezeCursor();
    pOut->dwTypingTimeoutMs = g_TypingTimeoutMs;
    pOut->dwCooldownMs = g_CooldownMs;
}

int TouchGesture_GetLogCount()
{
    return g_LogCount;
}

BOOL TouchGesture_GetLogItem(int idx, TCHAR* szBuf, int cchMax)
{
    if (idx < 0 || idx >= g_LogCount || !szBuf)
        return FALSE;
    int realIdx = (g_LogHead + idx) % MAX_LOG_ENTRIES;
    _tcscpy_s(szBuf, cchMax, g_LogEntries[realIdx]);
    return TRUE;
}

void TouchGesture_ClearLog()
{
    g_LogCount = 0;
    g_LogHead = 0;
}

