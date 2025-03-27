// Copyright (C) 2007-2013 Ivan Zhakov.
// Main application source file

#include "stdafx.h"
#include "..\HookDll\HookDll.h"
#include "AboutDlg.h"
#include "resource.h"
#include "Constants.h"
#include <shellapi.h>
#include <tchar.h>

#define TOUCHFREEZE_KEY _T("Software\\TouchFreeze")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const UINT     wm_KBHookNotify = RegisterWindowMessage( TFHookNotifyMsg );
const UINT     wm_ShellNotify = WM_APP + 1;
NOTIFYICONDATA m_NotifyIcon;
HICON          g_hIconNormal;
HICON          g_hIconBlocked;

HINSTANCE      g_hInst = NULL;

const int IDT_HIDE_BALLOON  = 1;
DWORD g_HideBalloonTime = 0;
const int g_BalloonTimeout = 500; // 500ms

// Block time constants
const DWORD BLOCK_TIME_SHORT = 300;  // 300ms - for fast typing
const DWORD BLOCK_TIME_NORMAL = 500; // 500ms - for normal typing
const DWORD BLOCK_TIME_LONG = 700;   // 700ms - for careful typing
DWORD g_CurrentBlockTime = BLOCK_TIME_NORMAL;

static LONG RegSetStringValue(HKEY hKey, LPCTSTR valueName, LPCTSTR value)
{
    return ::RegSetValueEx(hKey, valueName, 0, REG_SZ,
        (const BYTE *) value, _tcslen(value)*sizeof(TCHAR)); 
}

static void OpenURL(HWND hwnd, LPCTSTR url)
{
    ShellExecute(hwnd, NULL, url, NULL, NULL, SW_SHOWNORMAL);
}

void ContactOrDonate(HWND hwnd, int type)
{
    // Website and donation links removed as they are no longer active
    return;
}

static void SetAutorun(BOOL autoRun)
{
    HKEY  regKey;
    TCHAR moduleFileName[_MAX_PATH];
    DWORD rv;

    DWORD moduleFileNameLen = GetModuleFileName(g_hInst, moduleFileName, _MAX_PATH);
    if (moduleFileNameLen == 0 || moduleFileNameLen >= _MAX_PATH)
    {
        return ;
    }

    // Create provider key
    rv = RegCreateKey(HKEY_CURRENT_USER, AUTORUN_KEY, &regKey);
    if (rv != ERROR_SUCCESS)
    {
        return ;
    }

    if (autoRun)
        RegSetStringValue(regKey, TOUCHFREEZE_KEY, moduleFileName);
    else
        RegDeleteValue(regKey, TOUCHFREEZE_KEY);

    RegCloseKey(regKey);
}

static BOOL IsAutorun()
{
    HKEY  regKey;
    TCHAR moduleFileName[_MAX_PATH];
    TCHAR regFileName[_MAX_PATH];
    
    // Get current module path and verify the result
    DWORD moduleFileNameLen = GetModuleFileName(g_hInst, moduleFileName, _MAX_PATH);
    if (moduleFileNameLen == 0 || moduleFileNameLen >= _MAX_PATH)
    {
        return FALSE;
    }

    // Open registry key with minimum required access rights
    if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_READ, &regKey) 
        != ERROR_SUCCESS)
    {
        return FALSE;
    }

    DWORD size = sizeof(regFileName);
    DWORD type = 0;
    
    // Query registry value and verify its type
    LONG result = RegQueryValueEx(regKey, TOUCHFREEZE_KEY, NULL, &type, 
                                (LPBYTE)regFileName, &size);
    
    RegCloseKey(regKey);
    
    if (result != ERROR_SUCCESS || type != REG_SZ)
    {
        return FALSE;
    }
    
    return _tcsicmp(regFileName, moduleFileName) == 0;
}

// Registry-related functions
static void SaveBlockTime(DWORD time)
{
    HKEY regKey;
    if (RegCreateKey(HKEY_CURRENT_USER, TOUCHFREEZE_KEY, &regKey) == ERROR_SUCCESS)
    {
        RegSetValueEx(regKey, _T("BlockTime"), 0, REG_DWORD, (LPBYTE)&time, sizeof(DWORD));
        RegCloseKey(regKey);
    }
}

static DWORD LoadBlockTime()
{
    HKEY regKey;
    DWORD time = BLOCK_TIME_NORMAL;
    DWORD size = sizeof(DWORD);
    
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TOUCHFREEZE_KEY, 0, KEY_READ, &regKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueEx(regKey, _T("BlockTime"), NULL, NULL, (LPBYTE)&time, &size) != ERROR_SUCCESS)
        {
            time = BLOCK_TIME_NORMAL;
        }
        RegCloseKey(regKey);
    }
    return time;
}

static void ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU));
    HMENU hMenuPopup = GetSubMenu(hMenu, 0);
    
    if (IsAutorun())
        DeleteMenu(hMenu, ID_AUTOSTART_ON, MF_BYCOMMAND);
    else
        DeleteMenu(hMenu, ID_AUTOSTART_OFF, MF_BYCOMMAND);

    // Set block time check
    UINT checkItem = ID_BLOCKTIME_NORMAL; // Set default value
    switch (g_CurrentBlockTime)
    {
    case BLOCK_TIME_SHORT:
        checkItem = ID_BLOCKTIME_SHORT;
        break;
    case BLOCK_TIME_LONG:
        checkItem = ID_BLOCKTIME_LONG;
        break;
    }
    CheckMenuRadioItem(hMenuPopup, ID_BLOCKTIME_SHORT, ID_BLOCKTIME_LONG,
                      checkItem, MF_BYCOMMAND);

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenuPopup, TPM_LEFTBUTTON|TPM_LEFTALIGN,
        pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static void ShowBlockedIcon(HWND hwnd)
{
    if (!g_hIconBlocked)
        g_hIconBlocked = LoadIcon(g_hInst, MAKEINTRESOURCE(IDR_MAINFRAME_BLOCKED));
    
    m_NotifyIcon.hIcon = g_hIconBlocked;
    Shell_NotifyIcon(NIM_MODIFY, &m_NotifyIcon);
}

static void ShowNormalIcon(HWND hwnd)
{
    if (!g_hIconNormal)
        g_hIconNormal = LoadIcon(g_hInst, MAKEINTRESOURCE(IDR_MAINFRAME));
    
    m_NotifyIcon.hIcon = g_hIconNormal;
    Shell_NotifyIcon(NIM_MODIFY, &m_NotifyIcon);
}

LRESULT CALLBACK MainWindowProc(
   HWND hWnd,
   UINT uMsg,
   WPARAM wParam,
   LPARAM lParam 
)
{
    switch(uMsg)
    {
    case WM_CREATE:
        ZeroMemory(&m_NotifyIcon, sizeof(m_NotifyIcon));
        m_NotifyIcon.cbSize = sizeof(m_NotifyIcon);
        g_hIconNormal = LoadIcon(g_hInst, MAKEINTRESOURCE(IDR_MAINFRAME));
        m_NotifyIcon.hIcon = g_hIconNormal;
        m_NotifyIcon.hWnd = hWnd;
        m_NotifyIcon.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
        m_NotifyIcon.uCallbackMessage = wm_ShellNotify;
        m_NotifyIcon.uID = 1;                
        _tcscpy_s(m_NotifyIcon.szTip, sizeof(m_NotifyIcon.szTip),
                  _T("TouchFreeze (Auto Mode)"));
        Shell_NotifyIcon(NIM_ADD, &m_NotifyIcon);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &m_NotifyIcon);
        if (g_hIconNormal)
            DestroyIcon(g_hIconNormal);
        if (g_hIconBlocked)
            DestroyIcon(g_hIconBlocked);
        return 0;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case ID_ABOUT:
            ShowAboutDlg(g_hInst, hWnd);
            break;
        case ID_EXIT:
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            break;
        case ID_AUTOSTART_ON:
            SetAutorun(TRUE);
            break;
        case ID_AUTOSTART_OFF:
            SetAutorun(FALSE);
            break;
        case ID_BLOCKTIME_SHORT:
            g_CurrentBlockTime = BLOCK_TIME_SHORT;
            TFHookSetBlockTime(BLOCK_TIME_SHORT);
            SaveBlockTime(BLOCK_TIME_SHORT);
            break;
        case ID_BLOCKTIME_NORMAL:
            g_CurrentBlockTime = BLOCK_TIME_NORMAL;
            TFHookSetBlockTime(BLOCK_TIME_NORMAL);
            SaveBlockTime(BLOCK_TIME_NORMAL);
            break;
        case ID_BLOCKTIME_LONG:
            g_CurrentBlockTime = BLOCK_TIME_LONG;
            TFHookSetBlockTime(BLOCK_TIME_LONG);
            SaveBlockTime(BLOCK_TIME_LONG);
            break;
        case ID_DONATE:
            ContactOrDonate(hWnd, 1);
            break;
        }
        return 0;

    case WM_CLOSE:
        PostQuitMessage(1);
        return 0;
    }

    if (uMsg == wm_KBHookNotify)
    {
        switch (wParam)
        {
        case TFNT_Blocked:
            ShowBlockedIcon(hWnd);
            break;
        case TFNT_UnBlocked:
            ShowNormalIcon(hWnd);
            break;
        }
        return 0;
    }
    else if (uMsg == wm_ShellNotify)
    {
        switch(lParam)
        {
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            ShowContextMenu(hWnd);
            break;
        }
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow)
{
    g_hInst = hInst;

    HWND hPrevHWND = FindWindow(WINDOW_CLASS_NAME, WINDOW_NAME);

    if (hPrevHWND)
        PostMessage(hPrevHWND, WM_CLOSE, 0, 0);

    if(_tcsstr(lpCmdLine, _T("unload")) != 0)
    {
        return 0;
    }

    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    WNDCLASSEX wndClass;
    ZeroMemory(&wndClass, sizeof(wndClass));
    wndClass.cbSize        = sizeof(wndClass);
    wndClass.hInstance     = hInst;
    wndClass.lpfnWndProc   = MainWindowProc;
    wndClass.lpszClassName = WINDOW_CLASS_NAME;
    if (!RegisterClassEx(&wndClass))
        return -1;

    HWND hwnd = CreateWindow(WINDOW_CLASS_NAME, WINDOW_NAME, 
        WS_OVERLAPPED, 0,0,0,0, HWND_MESSAGE, NULL, hInst, NULL);

    if (!hwnd)
        return -2;

    // Load block time and set
    g_CurrentBlockTime = LoadBlockTime();
    TFHookSetBlockTime(g_CurrentBlockTime);

    TFHookInstall(hwnd);

    while( GetMessage( &msg, NULL, 0, 0 ))
    { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    }
    
    TFHookUninstall();
    
    DestroyWindow(hwnd);
    UnregisterClass(WINDOW_CLASS_NAME, hInst);
    return 0;
}
