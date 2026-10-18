// ==WindhawkMod==
// @id              taskbar-transparent-win10
// @name            Barra de Tarefas Transparente (Win10)
// @description     Deixa a barra de tarefas do Windows 10 transparente (blur ou acrylic)
// @version         1.0
// @author          Nelson
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

// ============================================================================
// Definições da API SetWindowCompositionAttribute (não documentada)
// ============================================================================

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
    DWORD GradientColor; // Formato ABGR (Alpha, Blue, Green, Red)
    DWORD AnimationId;
} ACCENT_POLICY;

using SetWindowCompositionAttribute_t = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
SetWindowCompositionAttribute_t SetWindowCompositionAttribute_Original = nullptr;

// ============================================================================
// Configurações globais
// ============================================================================

struct {
    bool useAcrylic;
    int opacity;
    int pollingMs;
} settings;

volatile bool g_stopThread = false;

// ============================================================================
// Aplicar transparência
// ============================================================================
void ApplyTransparency(HWND hWnd) {
    if (!hWnd || !SetWindowCompositionAttribute_Original) return;

    ACCENT_POLICY policy;
    ZeroMemory(&policy, sizeof(policy));
    policy.AccentState = settings.useAcrylic
        ? ACCENT_ENABLE_ACRYLICBLURBEHIND
        : ACCENT_ENABLE_BLURBEHIND;
    policy.AccentFlags = 2;
    // GradientColor = AABBGGRR (alpha, blue, green, red) — preto com alpha
    policy.GradientColor = ((DWORD)settings.opacity << 24) | 0x000000;

    WINDOWCOMPOSITIONATTRIBDATA data;
    ZeroMemory(&data, sizeof(data));
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    SetWindowCompositionAttribute_Original(hWnd, &data);
}







bool IsTaskbarClass(const WCHAR* className) {
    return wcscmp(className, L"Shell_TrayWnd") == 0 ||
           wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

void ApplyToAllTaskbars() {
    // Barra principal
    HWND hTaskbar = FindWindow(L"Shell_TrayWnd", nullptr);
    if (hTaskbar) ApplyTransparency(hTaskbar);

    // Barras secundárias (multi-monitor)
    HWND hSecondary = nullptr;
    while ((hSecondary = FindWindowEx(nullptr, hSecondary,
                                     L"Shell_SecondaryTrayWnd", nullptr))) {
        ApplyTransparency(hSecondary);
    }
}








// ============================================================================
// Hook em SetWindowCompositionAttribute
// ============================================================================

BOOL WINAPI SetWindowCompositionAttribute_Hook(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA* data) {
    if (data && data->Attrib == WCA_ACCENT_POLICY &&
        data->pvData && data->cbData >= sizeof(ACCENT_POLICY)) {
        WCHAR className[64] = {0};
        GetClassNameW(hWnd, className, 64);

        if (IsTaskbarClass(className)) {
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

// ============================================================================
// Thread de reaplicação periódica
// ============================================================================

DWORD WINAPI ReapplyThread(LPVOID) {
    while (!g_stopThread) {
        Sleep(settings.pollingMs);
        if (g_stopThread) break;
        ApplyToAllTaskbars();
    }
    return 0;
}

// ============================================================================
// Carregar configurações
// ============================================================================

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

// ============================================================================
// Entry points do Windhawk
// ============================================================================

BOOL Wh_ModInit() {
    Wh_Log(L"Init");
    LoadSettings();
    g_stopThread = false;

    // Pega o endereço de SetWindowCompositionAttribute no user32.dll
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

    // Aplica imediatamente
    ApplyToAllTaskbars();

    // Inicia thread de reaplicação
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
    ApplyToAllTaskbars();
}
