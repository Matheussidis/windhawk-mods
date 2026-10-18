// ==WindhawkMod==
// @id              explorer-transparent-win10
// @name            Explorer Transparente (Win10)
// @description     Deixa a janela do File Explorer com moldura transparente (blur/acrylic)
// @version         1.0
// @author          Nelson
// @github          https://github.com/Matheussidis
// @include         explorer.exe
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*...*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*...*/
// ==/WindhawkModSettings==

#include <windows.h>

typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19,
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef enum _ACCENT_STATE {
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

using SetWindowCompositionAttribute_t = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
SetWindowCompositionAttribute_t SetWindowCompositionAttribute_Original = nullptr;

struct {
    bool useAcrylic;
    int opacity;
    int pollingMs;
} settings;

volatile bool g_stopThread = false;

void ApplyTransparency(HWND hWnd) {
    if (!hWnd || !SetWindowCompositionAttribute_Original) return;

    ACCENT_POLICY policy;
    ZeroMemory(&policy, sizeof(policy));
    policy.AccentState = settings.useAcrylic
        ? ACCENT_ENABLE_ACRYLICBLURBEHIND
        : ACCENT_ENABLE_BLURBEHIND;
    policy.AccentFlags = 2;
    policy.GradientColor = ((DWORD)settings.opacity << 24) | 0x000000;

    WINDOWCOMPOSITIONATTRIBDATA data;
    ZeroMemory(&data, sizeof(data));
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    SetWindowCompositionAttribute_Original(hWnd, &data);
}

bool IsExplorerWindow(const WCHAR* className) {
    return wcscmp(className, L"CabinetWClass") == 0 ||
           wcscmp(className, L"ExploreWClass") == 0;
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM) {
    WCHAR className[64] = {0};
    GetClassNameW(hWnd, className, 64);
    if (IsExplorerWindow(className)) {
        ApplyTransparency(hWnd);
    }
    return TRUE;
}

void ApplyToAllExplorers() {
    EnumWindows(EnumWindowsProc, 0);
}

BOOL WINAPI SetWindowCompositionAttribute_Hook(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA* data) {
    if (data && data->Attrib == WCA_ACCENT_POLICY &&
        data->pvData && data->cbData >= sizeof(ACCENT_POLICY)) {
        WCHAR className[64] = {0};
        GetClassNameW(hWnd, className, 64);

        if (IsExplorerWindow(className)) {
            ACCENT_POLICY* policy = (ACCENT_POLICY*)data->pvData;
            policy->AccentState = settings.useAcrylic
                ? ACCENT_ENABLE_ACRYLICBLURBEHIND
                : ACCENT_ENABLE_BLURBEHIND;
            policy->AccentFlags = 2;
            policy->GradientColor = ((DWORD)settings.opacity << 24) | 0x000000;
        }
    }
    return SetWindowCompositionAttribute_Original(hWnd, data);
}

DWORD WINAPI ReapplyThread(LPVOID) {
    while (!g_stopThread) {
        Sleep(settings.pollingMs);
        if (g_stopThread) break;
        ApplyToAllExplorers();
    }
    return 0;
}

void LoadSettings() {
    PCWSTR mode = Wh_GetStringSetting(L"mode");
    settings.useAcrylic = (wcscmp(mode, L"acrylic") == 0);
    Wh_FreeStringSetting(mode);

    settings.opacity = Wh_GetIntSetting(L"opacity");
    if (settings.opacity < 0) settings.opacity = 0;
    if (settings.opacity > 255) settings.opacity = 255;

    settings.pollingMs = Wh_GetIntSetting(L"pollingMs");
    if (settings.pollingMs < 100) settings.pollingMs = 100;
    if (settings.pollingMs > 5000) settings.pollingMs = 5000;
}

BOOL Wh_ModInit() {
    Wh_Log(L"Init");
    LoadSettings();
    g_stopThread = false;

    HMODULE user32 = GetModuleHandle(L"user32.dll");
    void* pFunc = (void*)GetProcAddress(user32, "SetWindowCompositionAttribute");

    if (pFunc) {
        Wh_SetFunctionHook(pFunc,
                           (void*)SetWindowCompositionAttribute_Hook,
                           (void**)&SetWindowCompositionAttribute_Original);
        Wh_Log(L"SetWindowCompositionAttribute hooked");
    } else {
        Wh_Log(L"SetWindowCompositionAttribute not found!");
        return FALSE;
    }

    ApplyToAllExplorers();

    HANDLE hThread = CreateThread(nullptr, 0, ReapplyThread, nullptr, 0, nullptr);
    if (hThread) CloseHandle(hThread);

    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Uninit");
    g_stopThread = true;
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
    LoadSettings();
    ApplyToAllExplorers();
}
