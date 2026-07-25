#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")

#define ID_BTN_NEXT 1001
#define ID_BTN_CANCEL 1002

static int current_step = 1;
static HWND hTitle, hSubTitle, hInfoText, hBtnNext, hBtnCancel, hProgressBar;
static wchar_t install_path[MAX_PATH];

static const unsigned char upp_embedded_exe[] = {
#include "upp_bytes.inc"
};

static void add_to_path(const wchar_t* dir) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        wchar_t old_path[8192] = {0};
        DWORD size = sizeof(old_path);
        RegQueryValueExW(hKey, L"Path", NULL, NULL, (LPBYTE)old_path, &size);

        if (wcsstr(old_path, dir) == NULL) {
            wchar_t new_path[8192];
            swprintf(new_path, 8192, L"%s;%s", old_path, dir);
            RegSetValueExW(hKey, L"Path", 0, REG_EXPAND_SZ, (LPBYTE)new_path, (DWORD)(wcslen(new_path) + 1) * sizeof(wchar_t));
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
        }
        RegCloseKey(hKey);
    }
}

static void associate_file_extension(const wchar_t* exe_path) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.upp", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"", 0, REG_SZ, (LPBYTE)L"UPlusPlusScript", (DWORD)(wcslen(L"UPlusPlusScript") + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\UPlusPlusScript\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        wchar_t cmd[MAX_PATH + 32];
        swprintf(cmd, MAX_PATH + 32, L"\"%s\" \"%%1\"", exe_path);
        RegSetValueExW(hKey, L"", 0, REG_SZ, (LPBYTE)cmd, (DWORD)(wcslen(cmd) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

static void perform_install(HWND hwnd) {
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, install_path);
    wcscat(install_path, L"\\Programs\\UPlusPlus");
    CreateDirectoryW(install_path, NULL);

    wchar_t exe_target[MAX_PATH];
    swprintf(exe_target, MAX_PATH, L"%s\\upp.exe", install_path);

    FILE* f = _wfopen(exe_target, L"wb");
    if (f) {
        fwrite(upp_embedded_exe, 1, sizeof(upp_embedded_exe), f);
        fclose(f);
    }

    add_to_path(install_path);
    associate_file_extension(exe_target);

    SendMessageW(hProgressBar, PBM_SETPOS, 100, 0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hFontText = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            hTitle = CreateWindowExW(0, L"STATIC", L"Майстер інсталяції U++ (Ukrainian++)", WS_CHILD | WS_VISIBLE, 30, 20, 480, 35, hwnd, NULL, NULL, NULL);
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            hSubTitle = CreateWindowExW(0, L"STATIC", L"Вітаємо у програмі інсталяції нативної мови U++!", WS_CHILD | WS_VISIBLE, 30, 55, 480, 25, hwnd, NULL, NULL, NULL);
            SendMessageW(hSubTitle, WM_SETFONT, (WPARAM)hFontText, TRUE);

            hInfoText = CreateWindowExW(0, L"STATIC", L"Цей майстер встановить U++ на ваш комп'ютер.\n\n• Компактна нативна мова на C (без Python)\n• Автоматичне додавання у системні змінні PATH\n• Готовність до запуску з будь-якого терміналу\n\nНатисніть 'Встановити' для початку інсталяції.", WS_CHILD | WS_VISIBLE, 30, 90, 480, 130, hwnd, NULL, NULL, NULL);
            SendMessageW(hInfoText, WM_SETFONT, (WPARAM)hFontText, TRUE);

            hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL, WS_CHILD | PBS_SMOOTH, 30, 230, 480, 22, hwnd, NULL, NULL, NULL);
            SendMessageW(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

            hBtnNext = CreateWindowExW(0, L"BUTTON", L"Встановити >", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 270, 275, 120, 32, hwnd, (HMENU)ID_BTN_NEXT, NULL, NULL);
            SendMessageW(hBtnNext, WM_SETFONT, (WPARAM)hFontText, TRUE);

            hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Скасувати", WS_CHILD | WS_VISIBLE, 400, 275, 110, 32, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);
            SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFontText, TRUE);
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_CANCEL) {
                DestroyWindow(hwnd);
            } else if (LOWORD(wParam) == ID_BTN_NEXT) {
                if (current_step == 1) {
                    ShowWindow(hProgressBar, SW_SHOW);
                    SetWindowTextW(hSubTitle, L"Йде процес інсталяції...");
                    SetWindowTextW(hInfoText, L"Будь ласка, зачекайте. Файли копіюються та реєструються в системі...");
                    EnableWindow(hBtnNext, FALSE);

                    perform_install(hwnd);

                    current_step = 2;
                    SetWindowTextW(hTitle, L"Інсталяцію успішно завершено! 🎉");
                    SetWindowTextW(hSubTitle, L"Мову U++ успішно встановлено на ваш ПК.");
                    SetWindowTextW(hInfoText, L"Тепер ви можете відкрити будь-який термінал (PowerShell або CMD) і заповнити команду:\n\n    upp <файл.upp>\n\nДякуємо за використання U++!");
                    SetWindowTextW(hBtnNext, L"Завершити");
                    EnableWindow(hBtnNext, TRUE);
                    ShowWindow(hBtnCancel, SW_HIDE);
                } else if (current_step == 2) {
                    DestroyWindow(hwnd);
                }
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"UPlusPlusInstallerClass";

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"UPlusPlusInstallerClass", L"U++ (Ukrainian++) Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 360,
        NULL, NULL, hInstance, NULL
    );

    RECT rw;
    GetWindowRect(hwnd, &rw);
    int xPos = (GetSystemMetrics(SM_CXSCREEN) - (rw.right - rw.left)) / 2;
    int yPos = (GetSystemMetrics(SM_CYSCREEN) - (rw.bottom - rw.top)) / 2;
    SetWindowPos(hwnd, NULL, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
