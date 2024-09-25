// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

HINSTANCE hInstance = NULL;
HHOOK hHook = NULL;


BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        hInstance = hModule;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

HWND FindDesktopWindow()
{
    UINT uFindCount = 0;
    HWND hSysListView32Wnd = NULL;
    while (NULL == hSysListView32Wnd && uFindCount < 10)
    {
        HWND hParentWnd = ::GetShellWindow();
        HWND hSHELLDLL_DefViewWnd = ::FindWindowEx(hParentWnd, NULL, L"SHELLDLL_DefView", NULL);
        hSysListView32Wnd = ::FindWindowEx(hSHELLDLL_DefViewWnd, NULL, L"SysListView32", L"FolderView");

        if (NULL == hSysListView32Wnd)
        {
            hParentWnd = ::FindWindowEx(NULL, NULL, L"WorkerW", L"");
            while ((!hSHELLDLL_DefViewWnd) && hParentWnd)
            {
                hSHELLDLL_DefViewWnd = ::FindWindowEx(hParentWnd, NULL, L"SHELLDLL_DefView", NULL);
                hParentWnd = FindWindowEx(NULL, hParentWnd, L"WorkerW", L"");
            }
            hSysListView32Wnd = ::FindWindowEx(hSHELLDLL_DefViewWnd, 0, L"SysListView32", L"FolderView");
        }

        if (NULL == hSysListView32Wnd)
        {
            Sleep(1000);
            uFindCount++;
        }
        else
        {
            break;
        }
    }

    return hSysListView32Wnd;
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    HWND hDesktop = FindDesktopWindow();
    if (nCode >= 0) {
        if (wParam == WM_MBUTTONDOWN)
        {
            LPMOUSEHOOKSTRUCT pMouse = (LPMOUSEHOOKSTRUCT)lParam;
            if (pMouse->hwnd == hDesktop || pMouse->hwnd == GetParent(hDesktop))
            {
                if (IsWindowVisible(hDesktop))
                    ShowWindow(hDesktop, SW_HIDE);
                else 
                    ShowWindow(hDesktop, SW_SHOW);
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

extern "C" _declspec(dllexport) BOOL StartHook()
{
    hHook = SetWindowsHookEx(WH_MOUSE, MouseProc, hInstance, 0);
    if (!hHook) return FALSE;
    return TRUE;
}

extern "C" _declspec(dllexport) void StopHook()
{
    if (hHook) {
        UnhookWindowsHookEx(hHook);
        hHook = NULL;
    }
}