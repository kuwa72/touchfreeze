// Copyright (C) 2007-2013 Ivan Zhakov.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msi.h>
#include <tchar.h>
#include <ShellAPI.h>

#include "..\TestApp\Constants.h"


BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

