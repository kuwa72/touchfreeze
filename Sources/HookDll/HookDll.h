// Copyright (C) 2007-2013 Ivan Zhakov.

#include <Windows.h>

#ifdef HOOKDLL_EXPORTS
#define HOOKDLL_API __declspec(dllexport)
#else
#define HOOKDLL_API __declspec(dllimport)
#endif


extern HOOKDLL_API int nHookDll;

const LPCWSTR TFHookNotifyMsg = L"TouchFreezeNotifyMessage";

enum NotifyType
{
    TFNT_Blocked   = 1,
    TFNT_UnBlocked = 2
};

HOOKDLL_API int TFHookInstall     (HWND hwnd);
HOOKDLL_API int TFHookUninstall   ();

HOOKDLL_API void TFHookGetStat(int * pFreezeCount);

enum RightDragZoneMode
{
    ZONE_DISABLED = 0,
    ZONE_ANYWHERE,
    ZONE_RIGHT_THIRD,
    ZONE_RIGHT_HALF,
    ZONE_TOP_RIGHT,
    ZONE_BOTTOM_RIGHT
};

HOOKDLL_API void TFHookSetBlockTime(DWORD milliseconds);

HOOKDLL_API void TFHookSetSuppressOSGesture(BOOL bSuppress);
HOOKDLL_API BOOL TFHookGetSuppressOSGesture();

HOOKDLL_API void TFHookSetRightDragZone(int mode);
HOOKDLL_API int  TFHookGetRightDragZone();

HOOKDLL_API void TFHookSetRightDragActive(BOOL bActive);
HOOKDLL_API BOOL TFHookGetRightDragActive();

HOOKDLL_API BOOL TFHookIsBlocked();

HOOKDLL_API DWORD TFHookGetLastKeyTime();
HOOKDLL_API DWORD TFHookGetLastVkCode();

HOOKDLL_API void TFHookSetOverrideBlocked(BOOL bBlocked);
HOOKDLL_API BOOL TFHookGetOverrideBlocked();


