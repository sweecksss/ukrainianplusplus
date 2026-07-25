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
static char install_path[MAX_PATH];

// Embedded bytes of upp.exe will be appended or read
static const unsigned char upp_embedded_exe[] = {
#include "upp_bytes.inc"
};

static void add_to_path(const char* dir) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        char old_path[8192] = {0};
        DWORD size = sizeof(old_path);
        RegQueryValueExA(hKey, "Path", NULL, NULL, (LPBYTE)old_path, &size);

        if (strstr(old_path, dir) == NULL) {
            char new_path[8192];
            sprintf(new_path, "%s;%s", old_path, dir);
            RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (LPBYTE)new_path, (DWORD)strlen(new_path) + 1);
            SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
        }
        RegCloseKey(hKey);
    }
}

static void associate_file_extension(const char* exe_path) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\.upp", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "", 0, REG_SZ, (LPBYTE)"UPlusPlusScript", 16);
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\UPlusPlusScript\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        char cmd[MAX_PATH + 32];
        sprintf(cmd, "\"%s\" \"%%1\"", exe_path);
        RegSetValueExA(hKey, "", 0, REG_SZ, (LPBYTE)cmd, (DWORD)strlen(cmd) + 1);
        RegCloseKey(hKey);
    }
}

static void perform_install(HWND hwnd) {
    SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, install_path);
    strcat(install_path, "\\Programs\\UPlusPlus");
    CreateDirectoryA(install_path, NULL);

    char exe_target[MAX_PATH];
    sprintf(exe_target, "%s\\upp.exe", install_path);

    FILE* f = fopen(exe_target, "wb");
    if (f) {
        fwrite(upp_embedded_exe, 1, sizeof(upp_embedded_exe), f);
        fclose(f);
    }

    add_to_path(install_path);
    associate_file_extension(exe_target);

    SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontText = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            hTitle = CreateWindowA("STATIC", "Майстер інсталяції U++ (Ukrainian++)", WS_CHILD | WS_VISIBLE, 30, 20, 480, 35, hwnd, NULL, NULL, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            hSubTitle = CreateWindowA("STATIC", "Вітаємо у програмі інсталяції нативної мови U++!", WS_CHILD | WS_VISIBLE, 30, 60, 480, 25, hwnd, NULL, NULL, NULL);
            SendMessage(hSubTitle, WM_SETFONT, (WPARAM)hFontText, TRUE);

            hInfoText = CreateWindowA("STATIC", "Цей майстер встановить U++ на ваш комп'ютер.\n\n- Компактна нативна мова на C\n- Автоматичне додавання у системні змінні PATH\n- Готовність до запуску з будь-якого терміналу\n\nНатисніть 'Встановити' для початку інсталяції.", WS_CHILD | WS_VISIBLE, 30, 100, 480, 150, hwnd, NULL, NULL, NULL);
            SendMessage(hInfoText, WM_SETFONT, (WPARAM)hFontText, TRUE);

            hProgressBar = CreateWindowExA(0, PROGRESS_CLASSA, NULL, WS_CHILD | PBS_SMOOTH, 30, 230, 480, 25, hwnd, NULL, NULL, NULL);
            SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

            hBtnNext = CreateWindowA("BUTTON", "Встановити >", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 290, 280, 110, 32, hwnd, (HMENU)ID_BTN_NEXT, NULL, NULL);
            SendMessage(hBtnNext, WM_SETFONT, (WPARAM)hFontText, TRUE);

            hBtnCancel = CreateWindowA("BUTTON", "Скасувати", WS_CHILD | WS_VISIBLE, 410, 280, 100, 32, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);
            SendMessage(hBtnCancel, WM_SETFONT, (WPARAM)hFontText, TRUE);
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_CANCEL) {
                DestroyWindow(hwnd);
            } else if (LOWORD(wParam) == ID_BTN_NEXT) {
                if (current_step == 1) {
                    ShowWindow(hProgressBar, SW_SHOW);
                    SetWindowTextA(hSubTitle, "Йде процес інсталяції...");
                    SetWindowTextA(hInfoText, "Будь ласка, зачекайте. Файли копіюються та реєструються в системі...");
                    EnableWindow(hBtnNext, FALSE);

                    perform_install(hwnd);

                    current_step = 2;
                    SetWindowTextA(hTitle, "Інсталяцію успішно завершено! 🎉");
                    SetWindowTextA(hSubTitle, "Мову U++ успішно встановлено на ваш ПК.");
                    SetWindowTextA(hInfoText, "Тепер ви можете відкрити будь-який термінал (PowerShell або CMD) і заповнити команду:\n\n    upp <файл.upp>\n\nДякуємо за використання U++!");
                    SetWindowTextA(hBtnNext, "Завершити");
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
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "UPlusPlusInstallerClass";

    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(
        0, "UPlusPlusInstallerClass", "U++ (Ukrainian++) Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 360,
        NULL, NULL, hInstance, NULL
    );

    RECT rc, rw;
    GetClientRect(hwnd, &rc);
    GetWindowRect(hwnd, &rw);
    int xPos = (GetSystemMetrics(SM_CXSCREEN) - (rw.right - rw.left)) / 2;
    int yPos = (GetSystemMetrics(SM_CYSCREEN) - (rw.bottom - rw.top)) / 2;
    SetWindowPos(hwnd, NULL, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
