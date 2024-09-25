// MouseHook.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "MouseHook.h"

#define MUTEX_NAME _T("Global\\SENKDesktopShowMutex")
#define APP_RUNNING_ERROR_MSG _T("应用程序已经在运行了！")

#define MAX_LOADSTRING 100

// 菜单 ID
#define IDC_START_HOOK 1000
#define IDC_STOP_HOOK 1001
#define IDC_SELFSTART 1002
#define IDC_ABOUT 1003
#define IDC_QUIT 1004

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
HANDLE hMutex;                                  // 互斥锁
HINSTANCE hLib = NULL;                          // 动态链接库句柄
static BOOL(*StartHookFunc)();                  // 动态链接库导出函数
static void (*StopHookFunc)();                  // 动态链接库导出函数
HMENU hMenu;                                    // 菜单句柄
NOTIFYICONDATA nid;                             // 状态栏数据
UINT WM_TASKBARCREATED;                         // 任务栏创建消息
BOOL isHook = FALSE;                            // Hook是否正在运行

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);   // 注册窗口类
BOOL                InitInstance(HINSTANCE, int);           // 保存实例并初始化窗口
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);    // 主窗口过程
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);      // 关于对话框过程
BOOL                SetAppMutex();                          // 创建互斥锁
void                UpdateMenuItems(HWND);                  // 更新菜单
void                EnableStartup(HWND);                    // 启用自启动
void                DisableStartup(HWND);                   // 禁用自启动
BOOL                CheckStartup(HWND);                     // 检查自启动

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 创建互斥锁
    if (!SetAppMutex())
    {
        return FALSE;
    }

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MOUSEHOOK, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MOUSEHOOK));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW,szWindowClass, szTitle, WS_POPUP,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }
   // 不要修改TaskbarCreated，这是系统任务栏自定义的消息  
   WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: UpdateMenuItems(HWND)
//
//  目标: 更新菜单项
//
void UpdateMenuItems(HWND hWnd)
{
    EnableMenuItem(hMenu, IDC_START_HOOK, MF_BYCOMMAND | ((isHook) ? MF_DISABLED : MF_ENABLED));
    EnableMenuItem(hMenu, IDC_STOP_HOOK, MF_BYCOMMAND | ((isHook) ? MF_ENABLED : MF_DISABLED));
    CheckMenuItem(hMenu, IDC_SELFSTART, MF_BYCOMMAND | ((CheckStartup(hWnd)) ? MF_CHECKED : MF_UNCHECKED));
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // 加载动态链接库
    if (!hLib) {
        hLib = LoadLibrary(L"MouseHookDll.dll");
        if (hLib == NULL)
        {
            MessageBox(NULL, L"加载动态链接库失败", L"错误", MB_OK | MB_ICONERROR);
            return -1;
        }

        StartHookFunc = reinterpret_cast<BOOL(*)()>(GetProcAddress(hLib, "StartHook"));
        StopHookFunc = reinterpret_cast<void(*)()>(GetProcAddress(hLib, "StopHook"));

        if (!StartHookFunc || !StopHookFunc)
        {
            FreeLibrary(hLib);
            return 1;
        }
    }

    switch (message)
    {
    case WM_CREATE:
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = 0;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_USER;
        nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
        lstrcpy(nid.szTip, szTitle);
        Shell_NotifyIcon(NIM_ADD, &nid);
        hMenu = CreatePopupMenu();//生成菜单
        AppendMenu(hMenu, MF_STRING, IDC_START_HOOK, L"开始服务");//添加菜单项
        AppendMenu(hMenu, MF_STRING, IDC_STOP_HOOK, L"暂停服务");
        AppendMenu(hMenu, MF_STRING | ((CheckStartup(hWnd)) ? MF_CHECKED : MF_UNCHECKED), IDC_SELFSTART, L"开机启动");
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, IDC_ABOUT, L"关于");
        AppendMenu(hMenu, MF_STRING, IDC_QUIT, L"退出");
        isHook = StartHookFunc();
        break;
    case WM_USER:
        if (lParam == WM_RBUTTONDOWN)
        {
            POINT pt;//用于接收鼠标坐标
            int Select = IDC_START_HOOK;//用于接收菜单选项返回值

            GetCursorPos(&pt);//取鼠标坐标  
            ::SetForegroundWindow(hWnd);//解决在菜单外单击左键菜单不消失的问题
            UpdateMenuItems(hWnd);//更新菜单

            Select = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, NULL, hWnd, NULL);//显示菜单并获取选项ID

            switch (Select)
            {
            case IDC_START_HOOK:
                isHook = StartHookFunc();
                break;
            case IDC_STOP_HOOK:
                StopHookFunc();
                isHook = FALSE;
                break;
            case IDC_SELFSTART:
                CheckStartup(hWnd) ? DisableStartup(hWnd) : EnableStartup(hWnd);//启用或禁用开机启动
                break;
            case IDC_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDC_QUIT:
                PostMessage(hWnd, WM_DESTROY, wParam, lParam);
                break;
            default:
                break;
            }
        }
        break;
    case WM_DESTROY:
        if(isHook) StopHookFunc();
        if(hLib) FreeLibrary(hLib);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        CloseHandle(hMutex);
        PostQuitMessage(0);
        break;
    default:
        if (message == WM_TASKBARCREATED)
            SendMessage(hWnd, WM_CREATE, wParam, lParam);
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// 开机启动
void EnableStartup(HWND hWnd)
{
    TCHAR szPath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, szPath, MAX_PATH); // 获取应用程序的完整路径

    HKEY hKey;
    DWORD dwDisposition;
    LONG result = RegCreateKeyEx(
        HKEY_CURRENT_USER,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        &dwDisposition);

    if (result == ERROR_SUCCESS)
    {
        TCHAR szKeyName[MAX_PATH];
        _stprintf_s(szKeyName, TEXT("%s"), TEXT("DesktopShow")); // 设置键名，可以是应用名称

        // 写入注册表键值
        result = RegSetValueEx(
            hKey,
            szKeyName,
            0,
            REG_SZ,
            (BYTE*)szPath,
            (lstrlen(szPath) + 1) * sizeof(TCHAR));

        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS)
        {
            MessageBox(hWnd, TEXT("写入注册表失败！"), TEXT("错误"), MB_OK | MB_ICONERROR);
        }
        else
        {
            MessageBox(hWnd, TEXT("已设置开机启动！"), TEXT("提示"), MB_OK);
        }
    }
    else
    {
        MessageBox(hWnd, TEXT("设置开机启动失败，请确保有足够的权限。"), TEXT("错误"), MB_OK | MB_ICONERROR);
    }
}

// 取消开机启动
void DisableStartup(HWND hWnd)
{
    TCHAR szKeyName[MAX_PATH] = { 0 };
    _tcscpy_s(szKeyName, TEXT("DesktopShow")); // 这里键名与EnableStartup中的一致

    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        0,
        KEY_WRITE,
        &hKey);

    if (result == ERROR_SUCCESS)
    {
        // 删除注册表键值
        result = RegDeleteValue(hKey, szKeyName);
        if (result != ERROR_SUCCESS)
        {
            MessageBox(hWnd, TEXT("删除注册表键值失败！"), TEXT("错误"), MB_OK | MB_ICONERROR);
        }
        else
        {
            MessageBox(hWnd, TEXT("已取消开机启动设置！"), TEXT("提示"), MB_OK);
        }

        RegCloseKey(hKey);
    }
    else
    {
        MessageBox(hWnd, TEXT("无法打开注册表项，请确保有足够的权限。"), TEXT("错误"), MB_OK | MB_ICONERROR);
    }
}

// 检查开机启动
BOOL CheckStartup(HWND hWnd)
{
    TCHAR szKeyName[MAX_PATH] = { 0 };
    _tcscpy_s(szKeyName, TEXT("DesktopShow")); // 这里键名与EnableStartup中的一致

    HKEY hKey;
    TCHAR szPath[MAX_PATH] = { 0 };

    // 尝试打开注册表项
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        0,
        KEY_READ,
        &hKey);

    if (result == ERROR_SUCCESS)
    {
        DWORD cbData = sizeof(szPath);
        DWORD dwType;

        // 尝试读取键值
        result = RegQueryValueEx(
            hKey,
            szKeyName,
            NULL,
            &dwType,
            (BYTE*)szPath,
            &cbData);

        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS && dwType == REG_SZ)
        {
            // 如果键值存在并且类型正确，则返回true
            //MessageBox(hWnd, TEXT("已设置开机启动！"), TEXT("状态"), MB_OK);
            return TRUE;
        }
        else
        {
            // 如果键值不存在或类型不正确，则返回false
            //MessageBox(hWnd, TEXT("未设置开机启动！"), TEXT("状态"), MB_OK);
            return FALSE;
        }
    }
    else
    {
        // 打开注册表项失败
        MessageBox(hWnd, TEXT("无法打开注册表项，请确保有足够的权限。"), TEXT("错误"), MB_OK | MB_ICONERROR);
        return FALSE;
    }
}

// 设置互斥量
BOOL SetAppMutex()
{
    hMutex = NULL;

    // 尝试打开已存在的互斥量
    hMutex = OpenMutex(SYNCHRONIZE | MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
    if (hMutex == NULL)
    {
        // 如果无法打开，尝试创建一个新的互斥量
        hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
        if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
        {
            // 如果创建失败或者已经有其他实例持有这个互斥量，则显示消息框并返回FALSE
            MessageBox(NULL, APP_RUNNING_ERROR_MSG, _T("警告"), MB_OK | MB_ICONWARNING);
            if (hMutex != NULL) CloseHandle(hMutex); // 关闭无效句柄
            return FALSE;
        }
    }
    else
    {
        // 如果打开了一个已存在的互斥量，则显示消息框并返回FALSE
        MessageBox(NULL, APP_RUNNING_ERROR_MSG, _T("警告"), MB_OK | MB_ICONWARNING);
        CloseHandle(hMutex);
        return FALSE;
    }

    return TRUE;
}