// Copyright (C) 2007-2013 Ivan Zhakov.
#include "stdafx.h"

#include "HookDll.h"

static const LPCWSTR SharedDataMutexName = L"TouchFreezeGlobalDataMutex";

#pragma data_seg("shareddata")

HHOOK               g_hhookKeyboard = NULL;
HHOOK               g_hhookMouse    = NULL;

HWND                g_hWnd          = NULL;
DWORD               g_LastKeyTime   = 0;
DWORD               g_FreezeCount   = 0;
DWORD               g_FreezeTicks   = 700;
BOOL                g_bSuppressOSGesture = FALSE;
int                 g_RightDragZoneMode = ZONE_DISABLED;
BOOL                g_bConvertedRightDrag = FALSE;
DWORD               g_LastVkCode    = 0;
BOOL                g_bRightDragActive  = FALSE;
BOOL                g_bOverrideBlocked = FALSE;
#pragma data_seg()

HANDLE              g_hMutex = NULL;
UINT                g_nNotifyMessage = ::RegisterWindowMessage(TFHookNotifyMsg);

#pragma comment(linker,"/SECTION:shareddata,SRW")

HINSTANCE g_hInstDLL;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            g_hInstDLL = (HINSTANCE) hModule;
            g_hMutex = ::CreateMutex(NULL, FALSE, SharedDataMutexName);
            break;
        
        case DLL_PROCESS_DETACH:
            ::CloseHandle(g_hMutex);
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

void SendNotify(NotifyType notifyType, LPARAM data)
{
    ::PostMessage(HWND_BROADCAST, g_nNotifyMessage, (WPARAM) notifyType, data);
}

static void LockGlobals()
{
    ::WaitForSingleObject(g_hMutex, INFINITE);
}

static void UnlockGlobals()
{
    ::ReleaseMutex(g_hMutex);
}

static bool IgnoredKey(DWORD vk)
{
    static DWORD ignoreKeys[] = {
        VK_CONTROL, VK_RCONTROL, VK_LCONTROL,
        VK_MENU,    VK_RMENU,    VK_LMENU,
        VK_SHIFT,   VK_RSHIFT,   VK_LSHIFT};
    for (int i = 0;i < sizeof(ignoreKeys)/sizeof(ignoreKeys[0]); i++)
    {
        if (ignoreKeys[i] == vk)
            return true;
    }
    
    return false;
}

__declspec(dllexport) LRESULT CALLBACK KeyboardHookProc( int code, 
                              WPARAM wParam, 
                              LPARAM lParam 
                              )
{
    KBDLLHOOKSTRUCT *pkbhs = (KBDLLHOOKSTRUCT *) lParam;

    if (code < 0) 
        return CallNextHookEx (g_hhookKeyboard, code, wParam, lParam);
    
    if (g_bSuppressOSGesture)
    {
        DWORD vk = pkbhs->vkCode;
        if (vk == VK_LWIN || vk == VK_RWIN || vk == VK_TAB || 
            vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
            vk == 'D' || vk == 'S')
        {
            return 1; // Suppress OS touchpad gesture keys (Win+Tab, Alt+Tab, Win+D, etc.)
        }
    }

    if ((pkbhs->flags & LLKHF_INJECTED) == 0 && !IgnoredKey(pkbhs->vkCode))
    {
        g_LastKeyTime = GetTickCount();
        g_LastVkCode = pkbhs->vkCode;
    }

    return CallNextHookEx (g_hhookKeyboard, code, wParam, lParam);
}

static bool IsBlockMouseMessage(UINT msg)
{
    static UINT ignoreMsgs[] = {
        WM_LBUTTONDOWN,   WM_MBUTTONDOWN,   WM_RBUTTONDOWN,
        WM_LBUTTONUP,     WM_MBUTTONUP,     WM_RBUTTONUP,
        WM_LBUTTONDBLCLK, WM_MBUTTONDBLCLK, WM_RBUTTONDBLCLK,
    };
   
    for (int i = 0;i < sizeof(ignoreMsgs)/sizeof(ignoreMsgs[0]); i++)
    {
        if (ignoreMsgs[i] == msg)
            return true;
    }
    
    return false;
}

__declspec(dllexport) LRESULT CALLBACK MouseHookProc( int code, 
                              WPARAM wParam, 
                              LPARAM lParam 
                              )
{
    MSLLHOOKSTRUCT  * pMouseHS = (MSLLHOOKSTRUCT *) lParam;

    // Block physical Left click during active Right Drag (prevents LButton+RButton collision)
    if (g_bRightDragActive && ((pMouseHS->flags & LLMHF_INJECTED) == 0))
    {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP || wParam == WM_LBUTTONDBLCLK)
        {
            return 1;
        }
    }

    DWORD currentTime = GetTickCount();
    DWORD timeSinceLastKey = currentTime - g_LastKeyTime;
    BOOL bShouldBlock = g_bOverrideBlocked || (timeSinceLastKey < g_FreezeTicks);
    
    if (bShouldBlock && IsBlockMouseMessage((UINT)wParam))
    {
        g_FreezeCount++;     
        
        if (g_hWnd)
            PostMessage(g_hWnd, g_nNotifyMessage, TFNT_Blocked, 0);
        
        return 1;
    }
    else if (!bShouldBlock && g_LastKeyTime != 0)
    {
        // Notify that block time has ended
        g_LastKeyTime = 0; // Reset
        if (g_hWnd)
            PostMessage(g_hWnd, g_nNotifyMessage, TFNT_UnBlocked, 0);
    }

    return CallNextHookEx (g_hhookMouse, code, wParam, lParam);
}

HOOKDLL_API int TFHookInstall (HWND hwnd)
{
    LockGlobals();
    TFHookUninstall();

    g_hWnd = hwnd;      
    g_hhookKeyboard = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, g_hInstDLL, 0);
    g_hhookMouse    = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInstDLL, 0);
    UnlockGlobals();

    if (g_hhookKeyboard == NULL || g_hhookMouse == NULL)
    {
      MessageBox(NULL, L"Failed to setup hooks.", L"TouchFreeze", MB_OK);
    }

    return 0;
}

HOOKDLL_API int TFHookUninstall()
{
    LockGlobals();
    if (g_hhookKeyboard != NULL)
    {
        UnhookWindowsHookEx(g_hhookKeyboard);
        g_hhookKeyboard = NULL;
        UnhookWindowsHookEx(g_hhookMouse);
        g_hhookMouse = NULL;
    }
    UnlockGlobals();
    return 0;
}

HOOKDLL_API void TFHookGetStat(int * pFreezeCount)
{
    if(pFreezeCount)
        *pFreezeCount = g_FreezeCount;
}

HOOKDLL_API void TFHookSetBlockTime(DWORD milliseconds)
{
    LockGlobals();
    g_FreezeTicks = milliseconds;
    UnlockGlobals();
}

HOOKDLL_API void TFHookSetSuppressOSGesture(BOOL bSuppress)
{
    LockGlobals();
    g_bSuppressOSGesture = bSuppress;
    UnlockGlobals();
}

HOOKDLL_API BOOL TFHookGetSuppressOSGesture()
{
    return g_bSuppressOSGesture;
}

HOOKDLL_API void TFHookSetRightDragZone(int mode)
{
    LockGlobals();
    g_RightDragZoneMode = mode;
    UnlockGlobals();
}

HOOKDLL_API int TFHookGetRightDragZone()
{
    return g_RightDragZoneMode;
}

HOOKDLL_API void TFHookSetRightDragActive(BOOL bActive)
{
    LockGlobals();
    g_bRightDragActive = bActive;
    UnlockGlobals();
}

HOOKDLL_API BOOL TFHookGetRightDragActive()
{
    return g_bRightDragActive;
}

HOOKDLL_API BOOL TFHookIsBlocked()
{
    DWORD currentTime = GetTickCount();
    return (g_LastKeyTime != 0 && currentTime - g_LastKeyTime < g_FreezeTicks);
}

HOOKDLL_API DWORD TFHookGetLastKeyTime()
{
    return g_LastKeyTime;
}

HOOKDLL_API DWORD TFHookGetLastVkCode()
{
    return g_LastVkCode;
}

HOOKDLL_API void TFHookSetOverrideBlocked(BOOL bBlocked)
{
    LockGlobals();
    g_bOverrideBlocked = bBlocked;
    UnlockGlobals();
}

HOOKDLL_API BOOL TFHookGetOverrideBlocked()
{
    return g_bOverrideBlocked;
}