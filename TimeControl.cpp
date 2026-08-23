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

static inline bool IsCharInAnyCar(IV::Ped ped) {
    if (!ped) return false;
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("IS_CHAR_IN_ANY_CAR", cxt);
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

static inline void SetCharCoordinates(IV::Ped ped, float x, float y, float z) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    CallNative("SET_CHAR_COORDINATES", cxt);
}

static inline void SetCharVelocity(IV::Ped ped, float vx, float vy, float vz) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(vx);
    cxt.Push(vy);
    cxt.Push(vz);
    CallNative("SET_CHAR_VELOCITY", cxt);
}

static inline void FreezeCharPosition(IV::Ped ped, int frozen) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(frozen);
    CallNative("FREEZE_CHAR_POSITION", cxt);
}

static inline void FreezeCharPositionAndDontLoadCollisions(IV::Ped ped, int frozen) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(frozen);
    CallNative("FREEZE_CHAR_POSITION_AND_DONT_LOAD_COLLISION", cxt);
}

static inline bool IsCharFrozen(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("IS_CHAR_FROZEN", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void SetCharCollision(IV::Ped ped, int collision) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(collision);
    CallNative("SET_CHAR_COLLISION", cxt);
}

static inline void SetCharNeverTargetted(IV::Ped ped, int neverTargetted) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(neverTargetted);
    CallNative("SET_CHAR_NEVER_TARGETTED", cxt);
}

static inline void ClearCharTasks(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("CLEAR_CHAR_TASKS", cxt);
}

static inline void ClearCharTasksImmediately(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("CLEAR_CHAR_TASKS_IMMEDIATELY", cxt);
}

static inline uint32_t GetCharHealth(IV::Ped ped) {
    NativeContext cxt;
    uint32_t hp = 0;
    cxt.Push(ped);
    cxt.Push(&hp);
    CallNative("GET_CHAR_HEALTH", cxt);
    return hp;
}

static inline void SetCharHealth(IV::Ped ped, uint32_t hp) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(hp);
    CallNative("SET_CHAR_HEALTH", cxt);
}

static inline bool GetClosestChar(float x, float y, float z, float radius, uint32_t flag1, uint32_t flag2, IV::Ped* pPed) {
    NativeContext cxt;
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    cxt.Push(radius);
    cxt.Push(flag1);
    cxt.Push(flag2);
    cxt.Push(pPed);
    CallNative("GET_CLOSEST_CHAR", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool IsPedAPlayer(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("IS_CHAR_A_PLAYER", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void ShakePad(uint32_t pad, uint32_t time, uint32_t freq) {
    NativeContext cxt;
    cxt.Push(pad);
    cxt.Push(time);
    cxt.Push(freq);
    CallNative("SHAKE_PAD", cxt);
}

static inline void SetTimeScale(float scale) {
    NativeContext cxt;
    cxt.Push(scale);
    CallNative("SET_TIME_SCALE", cxt);
}

static inline float GetTimeScale() {
    NativeContext cxt;
    CallNative("GET_TIME_SCALE", cxt);
    return cxt.GetResult<float>();
}

static inline void SetGameSpeed(float speed) {
    NativeContext cxt;
    cxt.Push(speed);
    CallNative("SET_GAME_SPEED", cxt);
}

static inline void SetPedDensityMultiplier(float mult) {
    NativeContext cxt;
    cxt.Push(mult);
    CallNative("SET_PED_DENSITY_MULTIPLIER", cxt);
}

static inline void SetVehicleDensityMultiplier(float mult) {
    NativeContext cxt;
    cxt.Push(mult);
    CallNative("SET_VEHICLE_DENSITY_MULTIPLIER", cxt);
}

static inline void SetRandomPedDensityMultiplier(float mult) {
    NativeContext cxt;
    cxt.Push(mult);
    CallNative("SET_RANDOM_PED_DENSITY_MULTIPLIER", cxt);
}

static inline void SetScenarioPedDensityMultiplier(float mult, float mult2) {
    NativeContext cxt;
    cxt.Push(mult);
    cxt.Push(mult2);
    CallNative("SET_SCENARIO_PED_DENSITY_MULTIPLIER", cxt);
}

static inline void SetParkedVehicleDensityMultiplier(float mult) {
    NativeContext cxt;
    cxt.Push(mult);
    CallNative("SET_PARKED_VEHICLE_DENSITY_MULTIPLIER", cxt);
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

static inline void FreezeCarPosition(IV::Vehicle vehicle, int frozen) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(frozen);
    CallNative("FREEZE_CAR_POSITION", cxt);
}

static inline void SetCarVelocity(IV::Vehicle vehicle, float vx, float vy, float vz) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(vx);
    cxt.Push(vy);
    cxt.Push(vz);
    CallNative("SET_CAR_VELOCITY", cxt);
}

static inline void SetCarCollision(IV::Vehicle vehicle, int collision) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(collision);
    CallNative("SET_CAR_COLLISION", cxt);
}

static inline void FreezeCarPositionAndDontLoadCollision(IV::Vehicle vehicle, int frozen) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(frozen);
    CallNative("FREEZE_CAR_POSITION_AND_DONT_LOAD_COLLISION", cxt);
}

static inline bool IsCarFrozen(IV::Vehicle vehicle) {
    NativeContext cxt;
    cxt.Push(vehicle);
    CallNative("IS_CAR_FROZEN", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline uint32_t GetCarHealth(IV::Vehicle vehicle) {
    NativeContext cxt;
    uint32_t hp = 0;
    cxt.Push(vehicle);
    cxt.Push(&hp);
    CallNative("GET_CAR_HEALTH", cxt);
    return hp;
}

static inline void PrintStringWithLiteralStringNow(const char* gxt, const char* text, uint32_t time, int flag) {
    NativeContext cxt;
    cxt.Push(gxt);
    cxt.Push(text);
    cxt.Push(time);
    cxt.Push(flag);
    CallNative("PRINT_STRING_WITH_LITERAL_STRING_NOW", cxt);
}

static inline void DisplayTextWithLiteralString(float x, float y, const char* gxt, const char* text) {
    NativeContext cxt;
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(gxt);
    cxt.Push(text);
    CallNative("DISPLAY_TEXT_WITH_LITERAL_STRING", cxt);
}

static inline void SetTextScale(float x, float y) {
    NativeContext cxt;
    cxt.Push(x);
    cxt.Push(y);
    CallNative("SET_TEXT_SCALE", cxt);
}

static inline void SetTextColour(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    NativeContext cxt;
    cxt.Push(r);
    cxt.Push(g);
    cxt.Push(b);
    cxt.Push(a);
    CallNative("SET_TEXT_COLOUR", cxt);
}

static inline void SetTextCentre(int centre) {
    NativeContext cxt;
    cxt.Push(centre);
    CallNative("SET_TEXT_CENTRE", cxt);
}

static inline void SetTextDropshadow(uint32_t distance, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    NativeContext cxt;
    cxt.Push(distance);
    cxt.Push(r);
    cxt.Push(g);
    cxt.Push(b);
    cxt.Push(a);
    CallNative("SET_TEXT_DROPSHADOW", cxt);
}

static inline void SetTextFont(uint32_t font) {
    NativeContext cxt;
    cxt.Push(font);
    CallNative("SET_TEXT_FONT", cxt);
}

static inline void SetTextEdge(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    NativeContext cxt;
    cxt.Push(r);
    cxt.Push(g);
    cxt.Push(b);
    cxt.Push(a);
    CallNative("SET_TEXT_EDGE", cxt);
}

static inline void SetTextWrap(float x, float y) {
    NativeContext cxt;
    cxt.Push(x);
    cxt.Push(y);
    CallNative("SET_TEXT_WRAP", cxt);
}

} // namespace GTA

struct Config {
    bool enabled;
    int toggleKey;
    bool freezeNPCs;
    bool freezeVehicles;
    bool freezeTimeScale;
    bool showNotification;
    bool invincibleWhenFrozen;
    int freezeRadius;
};

static Config g_Config = {
    true,   // enabled
    0x54,   // toggleKey: 'T' key (VK_T)
    true,   // freezeNPCs
    true,   // freezeVehicles
    true,   // freezeTimeScale
    true,   // showNotification
    true,   // invincibleWhenFrozen
    200     // freezeRadius (meters)
};

struct FrozenEntity {
    uint32_t handle;
    bool isVehicle;
};

#define MAX_FROZEN_ENTITIES 512
static FrozenEntity g_FrozenEntities[MAX_FROZEN_ENTITIES];
static int g_FrozenCount = 0;

static bool g_TimeFrozen = false;
static bool g_KeyPressed = false;
static DWORD g_LastToggleTime = 0;

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

    const char* iniName = "TimeControl.ini";
    int len = 0;
    while (iniPath[len] != '\0') len++;
    int j = 0;
    while (iniName[j] != '\0') {
        iniPath[len + j] = iniName[j];
        j++;
    }
    iniPath[len + j] = '\0';

    g_Config.enabled = GetPrivateProfileIntA("SETTINGS", "Enabled", 1, iniPath) != 0;
    g_Config.toggleKey = GetPrivateProfileIntA("SETTINGS", "ToggleKey", 0x54, iniPath);
    g_Config.freezeNPCs = GetPrivateProfileIntA("SETTINGS", "FreezeNPCs", 1, iniPath) != 0;
    g_Config.freezeVehicles = GetPrivateProfileIntA("SETTINGS", "FreezeVehicles", 1, iniPath) != 0;
    g_Config.freezeTimeScale = GetPrivateProfileIntA("SETTINGS", "FreezeTimeScale", 1, iniPath) != 0;
    g_Config.showNotification = GetPrivateProfileIntA("SETTINGS", "ShowNotification", 1, iniPath) != 0;
    g_Config.invincibleWhenFrozen = GetPrivateProfileIntA("SETTINGS", "InvincibleWhenFrozen", 1, iniPath) != 0;
    g_Config.freezeRadius = GetPrivateProfileIntA("SETTINGS", "FreezeRadius", 200, iniPath);
}

static void AddFrozenEntity(uint32_t handle, bool isVehicle) {
    if (g_FrozenCount < MAX_FROZEN_ENTITIES) {
        g_FrozenEntities[g_FrozenCount].handle = handle;
        g_FrozenEntities[g_FrozenCount].isVehicle = isVehicle;
        g_FrozenCount++;
    }
}

static void ClearFrozenEntities() {
    g_FrozenCount = 0;
}

static void FreezeAllNearbyEntities(IV::Ped playerPed) {
    ClearFrozenEntities();

    float px = 0.0f, py = 0.0f, pz = 0.0f;
    GTA::GetCharCoordinates(playerPed, &px, &py, &pz);

    if (g_Config.freezeNPCs) {
        struct Point { float x, y, z; };
        Point samplePoints[9] = {
            { px, py, pz },
            { px + 50.0f, py, pz },
            { px - 50.0f, py, pz },
            { px, py + 50.0f, pz },
            { px, py - 50.0f, pz },
            { px + 35.0f, py + 35.0f, pz },
            { px - 35.0f, py + 35.0f, pz },
            { px + 35.0f, py - 35.0f, pz },
            { px - 35.0f, py - 35.0f, pz }
        };

        for (int s = 0; s < 9; s++) {
            for (int i = 0; i < 50; i++) {
                IV::Ped ped = 0;
                if (!GTA::GetClosestChar(samplePoints[s].x, samplePoints[s].y, samplePoints[s].z, (float)g_Config.freezeRadius, 1, 1, &ped)) break;
                if (!ped || ped == playerPed) continue;
                if (!GTA::DoesCharExist(ped) || GTA::IsCharDead(ped) || GTA::IsCharInAnyCar(ped)) continue;
                if (GTA::IsPedAPlayer(ped)) continue;

                bool alreadyFrozen = false;
                for (int k = 0; k < g_FrozenCount; k++) {
                    if (g_FrozenEntities[k].handle == ped && !g_FrozenEntities[k].isVehicle) {
                        alreadyFrozen = true;
                        break;
                    }
                }
                if (alreadyFrozen) continue;

                GTA::FreezeCharPosition(ped, 1);
                GTA::FreezeCharPositionAndDontLoadCollisions(ped, 1);
                GTA::SetCharCollision(ped, 0);
                GTA::SetCharNeverTargetted(ped, 1);
                GTA::ClearCharTasksImmediately(ped);
                GTA::SetCharVelocity(ped, 0.0f, 0.0f, 0.0f);

                if (g_Config.invincibleWhenFrozen) {
                    GTA::SetCharHealth(ped, 10000);
                }

                AddFrozenEntity(ped, false);
            }
        }
    }

    if (g_Config.freezeVehicles) {
        struct Point { float x, y, z; };
        Point carSamplePoints[9] = {
            { px, py, pz },
            { px + 50.0f, py, pz },
            { px - 50.0f, py, pz },
            { px, py + 50.0f, pz },
            { px, py - 50.0f, pz },
            { px + 35.0f, py + 35.0f, pz },
            { px - 35.0f, py + 35.0f, pz },
            { px + 35.0f, py - 35.0f, pz },
            { px - 35.0f, py - 35.0f, pz }
        };

        for (int s = 0; s < 9; s++) {
            for (int i = 0; i < 30; i++) {
                IV::Vehicle car = GTA::GetClosestCar(carSamplePoints[s].x, carSamplePoints[s].y, carSamplePoints[s].z, (float)g_Config.freezeRadius);
                if (!car || !GTA::DoesVehicleExist(car) || GTA::IsCarDead(car)) break;

                bool alreadyFrozen = false;
                for (int k = 0; k < g_FrozenCount; k++) {
                    if (g_FrozenEntities[k].handle == car && g_FrozenEntities[k].isVehicle) {
                        alreadyFrozen = true;
                        break;
                    }
                }
                if (alreadyFrozen) continue;

                GTA::FreezeCarPosition(car, 1);
                GTA::FreezeCarPositionAndDontLoadCollision(car, 1);
                GTA::SetCarCollision(car, 0);
                GTA::SetCarVelocity(car, 0.0f, 0.0f, 0.0f);

                AddFrozenEntity(car, true);
            }
        }
    }
}

static void UnfreezeAllEntities() {
    for (int i = 0; i < g_FrozenCount; i++) {
        if (g_FrozenEntities[i].isVehicle) {
            if (GTA::DoesVehicleExist(g_FrozenEntities[i].handle)) {
                GTA::FreezeCarPosition(g_FrozenEntities[i].handle, 0);
                GTA::FreezeCarPositionAndDontLoadCollision(g_FrozenEntities[i].handle, 0);
                GTA::SetCarCollision(g_FrozenEntities[i].handle, 1);
            }
        } else {
            if (GTA::DoesCharExist(g_FrozenEntities[i].handle)) {
                GTA::FreezeCharPosition(g_FrozenEntities[i].handle, 0);
                GTA::FreezeCharPositionAndDontLoadCollisions(g_FrozenEntities[i].handle, 0);
                GTA::SetCharCollision(g_FrozenEntities[i].handle, 1);
                GTA::SetCharNeverTargetted(g_FrozenEntities[i].handle, 0);
            }
        }
    }
    ClearFrozenEntities();
}

static void ShowNotification(const char* text) {
    GTA::PrintStringWithLiteralStringNow("STRING", text, 2000, 1);
}

static void DrawTimeFrozenOverlay() {
    GTA::SetTextScale(0.5f, 0.8f);
    GTA::SetTextColour(0, 255, 255, 255);
    GTA::SetTextCentre(1);
    GTA::SetTextDropshadow(2, 0, 0, 0, 255);
    GTA::SetTextFont(1);
    GTA::SetTextEdge(0, 0, 0, 255);
    GTA::SetTextWrap(0.0f, 1.0f);
    GTA::DisplayTextWithLiteralString(0.5f, 0.05f, "STRING", "~c~TIME FROZEN~w~");
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
            g_TimeFrozen = !g_TimeFrozen;
            g_LastToggleTime = now;

            if (g_TimeFrozen) {
                FreezeAllNearbyEntities(playerPed);
                if (g_Config.showNotification) {
                    ShowNotification("~c~TIME FROZEN~w~ - Press T to unfreeze");
                }
                GTA::ShakePad(0, 300, 200);
            } else {
                UnfreezeAllEntities();
                if (g_Config.showNotification) {
                    ShowNotification("~g~TIME RESUMED~w~");
                }
                GTA::ShakePad(0, 200, 150);
            }
        }
    }

    g_KeyPressed = keyDown;

    if (g_TimeFrozen) {
        DrawTimeFrozenOverlay();

        if (g_Config.freezeNPCs || g_Config.freezeVehicles) {
            FreezeAllNearbyEntities(playerPed);
        }
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