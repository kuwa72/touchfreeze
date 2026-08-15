// TouchGesture.h - Precision Touchpad gesture recognition & Palm Rejection for TouchFreeze
#pragma once
#include <windows.h>

#define TF_GESTURE_EXTRA_INFO 0x54464753 // "TFGS"

enum TouchGestureState
{
    GESTURE_STATE_IDLE = 0,
    GESTURE_STATE_ARMED,      // Multi-touch detected
    GESTURE_STATE_DRAGGING,   // Right drag in progress
    GESTURE_STATE_CANCELLED
};

enum PalmRejectionState
{
    PALM_STATE_IDLE = 0,
    PALM_STATE_TYPING,
    PALM_STATE_TOUCH_DETECTED,
    PALM_STATE_BLOCKED,
    PALM_STATE_RELEASED,
    PALM_STATE_COOLDOWN
};

struct TouchDiagInfo
{
    PalmRejectionState state;
    DWORD dwLastKeyTime;
    DWORD dwElapsedKeyMs;
    UINT  nLastVkCode;
    DWORD dwLastTouchTime;
    DWORD dwElapsedTouchMs;
    double fRatioX;
    double fRatioY;
    double fDeltaX;
    double fDeltaY;
    double fDeltaDist;
    ULONG rawX;
    ULONG rawY;
    ULONG minX;
    ULONG maxX;
    ULONG minY;
    ULONG maxY;
    HANDLE hDevice;
    TCHAR szDeviceName[256];
    BOOL bTouchActive;
    BOOL bBlocked;
    BOOL bDragging;
    int  nContactCount;
    BOOL bConfidence;
    BOOL bAllowSingleFingerMove;
    DWORD dwTypingTimeoutMs;
    DWORD dwCooldownMs;
};

// Initialize TouchGesture (registers RawInput/Pointer APIs)
BOOL TouchGesture_Init(HWND hWnd);

// Uninitialize TouchGesture
void TouchGesture_Uninit(HWND hWnd);

// Handle window messages (WM_INPUT, WM_POINTER*, etc.)
// Returns TRUE if the message was handled by TouchGesture.
BOOL TouchGesture_HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

enum TouchpadZoneMode
{
    PAD_ZONE_DISABLED = 0,
    PAD_ZONE_ANYWHERE,
    PAD_ZONE_RIGHT_THIRD,
    PAD_ZONE_RIGHT_HALF,
    PAD_ZONE_TOP_RIGHT,
    PAD_ZONE_BOTTOM_RIGHT,
    PAD_ZONE_RIGHT_QUARTER,
    PAD_ZONE_RIGHT_FIFTH,
    PAD_ZONE_BOTTOM_RIGHT_CORNER,
    PAD_ZONE_CUSTOM
};

// Enable / Disable TouchGesture feature
void TouchGesture_SetEnabled(BOOL bEnable);
BOOL TouchGesture_IsEnabled();

void TouchGesture_SetZoneMode(int mode);
int  TouchGesture_GetZoneMode();

void TouchGesture_SetCustomZone(double minX, double maxX, double minY, double maxY);
void TouchGesture_GetCustomZone(double *pMinX, double *pMaxX, double *pMinY, double *pMaxY);

// Check if currently performing a right drag injected by TouchGesture
BOOL TouchGesture_IsDragging();

// Get recent physical touchpad touch ratio coordinates (0.0 to 1.0)
BOOL TouchGesture_GetLastRatio(double *pRatioX, double *pRatioY);

// Palm Rejection Configuration & Evaluation
void TouchGesture_SetPalmConfig(DWORD typingTimeoutMs, DWORD cooldownMs, BOOL bAllowSingleFingerMove);
BOOL TouchGesture_ShouldBlockMouse();

// Diagnostic Information & Log Functions for Monitor GUI
void TouchGesture_GetDiagInfo(TouchDiagInfo* pOut);
LPCTSTR TouchGesture_GetStateName(PalmRejectionState state);
int  TouchGesture_GetLogCount();
BOOL TouchGesture_GetLogItem(int idx, TCHAR* szBuf, int cchMax);
void TouchGesture_ClearLog();


