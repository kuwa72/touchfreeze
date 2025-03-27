// Copyright (C) 2007-2013 Ivan Zhakov.
#include "stdafx.h"

#include "AboutDlg.h"
#include "KeyHookTest.h"

#include "resource.h"

static BOOL CALLBACK AboutDlgProc(
   HWND hWnd,
   UINT uMsg,
   WPARAM wParam,
   LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_CLOSE:
        EndDialog(hWnd, IDCANCEL);
        break;
    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
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
