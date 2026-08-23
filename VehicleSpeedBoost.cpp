#include <windows.h>
#include <stdint.h>

#define PI_CONST 3.14159265358979323846f

typedef SHORT (WINAPI *fnGetAsyncKeyState)(int vKey);
static inline SHORT MyGetAsyncKeyState(int vKey) {
    static fnGetAsyncKeyState pfn = NULL;
    if (!pfn) {
        HMODULE hUser = GetModuleHandleA("user32.dll");
        if (!hUser) hUser = LoadLibraryA("user32.dll");
        if (hUser) pfn = (fnGetAsyncKeyState)GetProcAddress(hUser, "GetAsyncKeyState");
    }
    if (pfn) return pfn(vKey);
    return 0;
}

static inline float MySqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float res;
    __asm__ __volatile__("fsqrt" : "=t"(res) : "0"(x));
    return res;
}

static inline float MySin(float x) {
    float res;
    __asm__ __volatile__("fsin" : "=t"(res) : "0"(x));
    return res;
}

static inline float MyCos(float x) {
    float res;
    __asm__ __volatile__("fcos" : "=t"(res) : "0"(x));
    return res;
}

extern "C" void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

extern "C" void* memset(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dest;
}

extern "C" size_t strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

struct scrNativeCallContext {
    void* m_pReturn;
    unsigned int m_nArgCount;
    void* m_pArgs;
    unsigned int m_nDataCount;
};

typedef void (__cdecl *NativeHandler)(scrNativeCallContext *cxt);

class NativeContext : public scrNativeCallContext {
private:
    uint8_t m_TempStack[32 * 4];
public:
    NativeContext() { Reset(); }
    void Reset() {
        m_pReturn = m_TempStack;
        m_pArgs = m_TempStack;
        m_nArgCount = 0;
        m_nDataCount = 0;
        memset(m_TempStack, 0, sizeof(m_TempStack));
    }
    template<typename T>
    void Push(T val) {
        uint32_t val32 = 0;
        if (sizeof(T) <= 4) {
            *(T*)&val32 = val;
        } else {
            val32 = (uint32_t)(uintptr_t)val;
        }
        *(uint32_t*)(m_TempStack + m_nArgCount * 4) = val32;
        m_nArgCount++;
    }
    template<typename T>
    T GetResult() {
        return *(T*)m_pReturn;
    }
};

namespace IV {
    typedef uint32_t Ped;
    typedef uint32_t Player;
    typedef uint32_t Vehicle;
    typedef uint32_t Object;
    typedef int boolean;
    typedef uint32_t uint;
}

typedef void* (*fnGetNativeAddress)(const char*);
typedef void (__thiscall *fnScriptThreadCtor)(void* pThis);
typedef void (*fnRegisterThread)(void* pThread, HINSTANCE hInstance);

static HMODULE g_hScriptHook = NULL;
static fnGetNativeAddress g_pfnGetNativeAddress = NULL;
static fnScriptThreadCtor g_pfnScriptThreadCtor = NULL;
static fnRegisterThread g_pfnRegisterThread = NULL;

static bool InitScriptHook() {
    if (!g_hScriptHook) {
        g_hScriptHook = GetModuleHandleA("ScriptHook.dll");
        if (!g_hScriptHook) {
            g_hScriptHook = LoadLibraryA("ScriptHook.dll");
        }
        if (g_hScriptHook) {
            g_pfnGetNativeAddress = (fnGetNativeAddress)GetProcAddress(g_hScriptHook, "?GetNativeAddress@Game@@SAPAXPBD@Z");
            g_pfnScriptThreadCtor = (fnScriptThreadCtor)GetProcAddress(g_hScriptHook, "??0ScriptThread@@QAE@XZ");
            g_pfnRegisterThread = (fnRegisterThread)GetProcAddress(g_hScriptHook, "?RegisterThread@ScriptHookManager@@SAXPAVScriptThread@@PAUHINSTANCE__@@@Z");
        }
    }
    return (g_pfnGetNativeAddress != NULL && g_pfnScriptThreadCtor != NULL && g_pfnRegisterThread != NULL);
}

static inline void CallNative(const char* name, NativeContext& cxt) {
    if (g_pfnGetNativeAddress) {
        void* handler = g_pfnGetNativeAddress(name);
        if (handler) {
            ((NativeHandler)handler)(&cxt);
        }
    }
}

typedef void (*ScriptTickCallback)();

struct GCCScriptThread {
    void* vtable[6];
    void* m_pContext;
    ScriptTickCallback tickCallback;
};

static GCCScriptThread g_ScriptThread;

static void __attribute__((thiscall)) Hook_RunTick(GCCScriptThread* pThis) {
    if (pThis && pThis->tickCallback) {
        pThis->tickCallback();
    }
}

static void __attribute__((thiscall)) Hook_Stub(GCCScriptThread* pThis) {
    (void)pThis;
}

static bool RegisterGCCScript(ScriptTickCallback callback, HINSTANCE hInstance) {
    if (!InitScriptHook()) return false;

    g_pfnScriptThreadCtor(&g_ScriptThread);

    g_ScriptThread.tickCallback = callback;

    static void* s_VTable[6];
    s_VTable[0] = (void*)Hook_RunTick;
    void** origVTable = *(void***)&g_ScriptThread;
    s_VTable[1] = origVTable[1];
    s_VTable[2] = (void*)Hook_Stub;
    s_VTable[3] = (void*)Hook_Stub;
    s_VTable[4] = origVTable[4];
    s_VTable[5] = origVTable[5];

    *(void***)&g_ScriptThread = s_VTable;

    g_pfnRegisterThread(&g_ScriptThread, hInstance);
    return true;
}

namespace GTA {

static inline IV::Player GetPlayerId() {
    NativeContext cxt;
    CallNative("GET_PLAYER_ID", cxt);
    return cxt.GetResult<IV::Player>();
}

static inline IV::Ped GetPlayerPed(IV::Player player) {
    NativeContext cxt;
    IV::Ped ped = 0;
    cxt.Push(player);
    cxt.Push(&ped);
    CallNative("GET_PLAYER_CHAR", cxt);
    return ped;
}

static inline bool DoesCharExist(IV::Ped ped) {
    if (!ped) return false;
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("DOES_CHAR_EXIST", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool IsCharDead(IV::Ped ped) {
    if (!ped) return true;
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("IS_CHAR_DEAD", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void GetCharCoordinates(IV::Ped ped, float* x, float* y, float* z) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    CallNative("GET_CHAR_COORDINATES", cxt);
}

static inline void GetCharSpeed(IV::Ped ped, float* speed) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(speed);
    CallNative("GET_CHAR_SPEED", cxt);
}

static inline IV::Vehicle GetClosestCar(float x, float y, float z, float radius) {
    NativeContext cxt;
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    cxt.Push(radius);
    cxt.Push(0);
    cxt.Push(70);
    CallNative("GET_CLOSEST_CAR", cxt);
    return cxt.GetResult<IV::Vehicle>();
}

static inline bool DoesVehicleExist(IV::Vehicle vehicle) {
    if (!vehicle) return false;
    NativeContext cxt;
    cxt.Push(vehicle);
    CallNative("DOES_VEHICLE_EXIST", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool IsCarDead(IV::Vehicle vehicle) {
    if (!vehicle) return true;
    NativeContext cxt;
    cxt.Push(vehicle);
    CallNative("IS_CAR_DEAD", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void GetCarCoordinates(IV::Vehicle vehicle, float* x, float* y, float* z) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    CallNative("GET_CAR_COORDINATES", cxt);
}

static inline void SetCarVelocity(IV::Vehicle vehicle, float vx, float vy, float vz) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(vx);
    cxt.Push(vy);
    cxt.Push(vz);
    CallNative("SET_CAR_VELOCITY", cxt);
}

static inline void GetCarSpeed(IV::Vehicle vehicle, float* speed) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(speed);
    CallNative("GET_CAR_SPEED", cxt);
}

static inline void GetCarForwardVector(IV::Vehicle vehicle, float* x, float* y, float* z) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    CallNative("GET_CAR_FORWARD_VECTOR", cxt);
}

static inline void SetCarForwardSpeed(IV::Vehicle vehicle, float speed) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(speed);
    CallNative("SET_CAR_FORWARD_SPEED", cxt);
}

static inline bool IsCharInAnyCar(IV::Ped ped) {
    if (!ped) return false;
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("IS_CHAR_IN_ANY_CAR", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline IV::Vehicle GetCarCharIsUsing(IV::Ped ped) {
    NativeContext cxt;
    IV::Vehicle car = 0;
    cxt.Push(ped);
    cxt.Push(&car);
    CallNative("GET_CAR_CHAR_IS_USING", cxt);
    return car;
}

static inline void PrintStringWithLiteralStringNow(const char* gxt, const char* text, uint32_t time, int flag) {
    NativeContext cxt;
    cxt.Push(gxt);
    cxt.Push(text);
    cxt.Push(time);
    cxt.Push(flag);
    CallNative("PRINT_STRING_WITH_LITERAL_STRING_NOW", cxt);
}

static inline void ShakePad(uint32_t pad, uint32_t time, uint32_t freq) {
    NativeContext cxt;
    cxt.Push(pad);
    cxt.Push(time);
    cxt.Push(freq);
    CallNative("SHAKE_PAD", cxt);
}

} // namespace GTA

struct Config {
    bool enabled;
    int toggleKey;
    float speedMultiplier;
    int boostRadius;
    bool showNotification;
    bool screenShake;
    bool includePlayerVehicle;
};

static Config g_Config = {
    true,       // enabled
    0x56,       // toggleKey: 'V' key (VK_V)
    5.0f,       // speedMultiplier: 5x normal speed
    50,         // boostRadius: 50 meters
    true,       // showNotification
    true,       // screenShake
    false       // includePlayerVehicle
};

static bool g_BoostActive = false;
static bool g_KeyPressed = false;
static DWORD g_LastToggleTime = 0;

struct BoostedVehicle {
    uint32_t handle;
};

#define MAX_BOOSTED_VEHICLES 128
static BoostedVehicle g_BoostedVehicles[MAX_BOOSTED_VEHICLES];
static int g_BoostedCount = 0;

static float ReadIniFloat(const char* section, const char* key, float defVal, const char* file) {
    char buf[64];
    GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), file);
    if (buf[0] == '\0') return defVal;

    float res = 0.0f;
    float sign = 1.0f;
    int i = 0;
    if (buf[0] == '-') { sign = -1.0f; i++; }
    while (buf[i] >= '0' && buf[i] <= '9') {
        res = res * 10.0f + (buf[i] - '0');
        i++;
    }
    if (buf[i] == '.') {
        i++;
        float factor = 0.1f;
        while (buf[i] >= '0' && buf[i] <= '9') {
            res += (buf[i] - '0') * factor;
            factor *= 0.1f;
            i++;
        }
    }
    return res * sign;
}

static void LoadConfig() {
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    char* lastSlash = NULL;
    for (int i = 0; iniPath[i] != '\0'; i++) {
        if (iniPath[i] == '\\' || iniPath[i] == '/') lastSlash = &iniPath[i];
    }
    if (lastSlash) *(lastSlash + 1) = '\0';

    const char* iniName = "VehicleSpeedBoost.ini";
    int len = 0;
    while (iniPath[len] != '\0') len++;
    int j = 0;
    while (iniName[j] != '\0') {
        iniPath[len + j] = iniName[j];
        j++;
    }
    iniPath[len + j] = '\0';

    g_Config.enabled = GetPrivateProfileIntA("SETTINGS", "Enabled", 1, iniPath) != 0;
    g_Config.toggleKey = GetPrivateProfileIntA("SETTINGS", "ToggleKey", 0x56, iniPath);
    g_Config.speedMultiplier = ReadIniFloat("SETTINGS", "SpeedMultiplier", 5.0f, iniPath);
    g_Config.boostRadius = GetPrivateProfileIntA("SETTINGS", "BoostRadius", 50, iniPath);
    g_Config.showNotification = GetPrivateProfileIntA("SETTINGS", "ShowNotification", 1, iniPath) != 0;
    g_Config.screenShake = GetPrivateProfileIntA("SETTINGS", "ScreenShake", 1, iniPath) != 0;
    g_Config.includePlayerVehicle = GetPrivateProfileIntA("SETTINGS", "IncludePlayerVehicle", 0, iniPath) != 0;
}

static void ClearBoostedVehicles() {
    g_BoostedCount = 0;
}

static bool IsVehicleBoosted(uint32_t vehicle) {
    for (int i = 0; i < g_BoostedCount; i++) {
        if (g_BoostedVehicles[i].handle == vehicle) return true;
    }
    return false;
}

static void AddBoostedVehicle(uint32_t vehicle) {
    if (g_BoostedCount < MAX_BOOSTED_VEHICLES) {
        g_BoostedVehicles[g_BoostedCount].handle = vehicle;
        g_BoostedCount++;
    }
}

static void RemoveBoostedVehicle(uint32_t vehicle) {
    for (int i = 0; i < g_BoostedCount; i++) {
        if (g_BoostedVehicles[i].handle == vehicle) {
            g_BoostedVehicles[i] = g_BoostedVehicles[g_BoostedCount - 1];
            g_BoostedCount--;
            return;
        }
    }
}

static void BoostAllNearbyVehicles(IV::Ped playerPed) {
    ClearBoostedVehicles();

    float px = 0.0f, py = 0.0f, pz = 0.0f;
    GTA::GetCharCoordinates(playerPed, &px, &py, &pz);

    struct Point { float x, y, z; };
    Point samplePoints[9] = {
        { px, py, pz },
        { px + 25.0f, py, pz },
        { px - 25.0f, py, pz },
        { px, py + 25.0f, pz },
        { px, py - 25.0f, pz },
        { px + 18.0f, py + 18.0f, pz },
        { px - 18.0f, py + 18.0f, pz },
        { px + 18.0f, py - 18.0f, pz },
        { px - 18.0f, py - 18.0f, pz }
    };

    for (int s = 0; s < 9; s++) {
        for (int i = 0; i < 20; i++) {
            IV::Vehicle car = GTA::GetClosestCar(samplePoints[s].x, samplePoints[s].y, samplePoints[s].z, (float)g_Config.boostRadius);
            if (!car || !GTA::DoesVehicleExist(car) || GTA::IsCarDead(car)) break;

            if (!g_Config.includePlayerVehicle && GTA::IsCharInAnyCar(playerPed)) {
                IV::Vehicle playerCar = GTA::GetCarCharIsUsing(playerPed);
                if (car == playerCar) continue;
            }

            if (IsVehicleBoosted(car)) continue;

            float cx = 0.0f, cy = 0.0f, cz = 0.0f;
            GTA::GetCarCoordinates(car, &cx, &cy, &cz);

            float fwdX = 0.0f, fwdY = 0.0f, fwdZ = 0.0f;
            GTA::GetCarForwardVector(car, &fwdX, &fwdY, &fwdZ);

            float currentSpeed = 0.0f;
            GTA::GetCarSpeed(car, &currentSpeed);

            float targetSpeed = currentSpeed * g_Config.speedMultiplier;
            if (targetSpeed < 30.0f) targetSpeed = 30.0f;

            GTA::SetCarForwardSpeed(car, targetSpeed);

            AddBoostedVehicle(car);
        }
    }
}

static void ShowNotification(const char* text) {
    GTA::PrintStringWithLiteralStringNow("STRING", text, 2000, 1);
}

static void OnTick() {
    if (!g_Config.enabled) return;

    IV::Player player = GTA::GetPlayerId();
    IV::Ped playerPed = GTA::GetPlayerPed(player);
    if (!GTA::DoesCharExist(playerPed) || GTA::IsCharDead(playerPed)) {
        return;
    }

    DWORD now = GetTickCount();

    bool keyDown = (MyGetAsyncKeyState(g_Config.toggleKey) & 0x8000) != 0;

    if (keyDown && !g_KeyPressed) {
        if (now - g_LastToggleTime > 300) {
            g_BoostActive = !g_BoostActive;
            g_LastToggleTime = now;

            if (g_BoostActive) {
                if (g_Config.showNotification) {
                    ShowNotification("~g~VEHICLE SPEED BOOST ACTIVATED~w~ - Press V to deactivate");
                }
                if (g_Config.screenShake) {
                    GTA::ShakePad(0, 300, 200);
                }
            } else {
                ClearBoostedVehicles();
                if (g_Config.showNotification) {
                    ShowNotification("~r~VEHICLE SPEED BOOST DEACTIVATED~w~");
                }
                if (g_Config.screenShake) {
                    GTA::ShakePad(0, 200, 150);
                }
            }
        }
    }

    g_KeyPressed = keyDown;

    if (g_BoostActive) {
        BoostAllNearbyVehicles(playerPed);
    }
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        LoadConfig();
        RegisterGCCScript(OnTick, hinstDLL);
    }
    return TRUE;
}