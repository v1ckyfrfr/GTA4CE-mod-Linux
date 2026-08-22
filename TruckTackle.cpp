#include <windows.h>
#include <stdint.h>

#define PI_CONST 3.14159265358979323846f

// Dynamic User32 input helper
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

// Inline math using x87 FPU instructions (zero CRT dependency)
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

// Builtin memory helpers
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

// Native Call Context
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
    NativeContext() {
        Reset();
    }

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
    typedef uint32_t Camera;
    typedef int boolean;
    typedef uint32_t uint;
}

// ScriptHook Dynamic Resolver
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

// Custom ScriptThread for GCC
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
    s_VTable[0] = (void*)Hook_RunTick; // RunTick
    void** origVTable = *(void***)&g_ScriptThread;
    s_VTable[1] = origVTable[1]; // RunScript
    s_VTable[2] = (void*)Hook_Stub; // OnStart
    s_VTable[3] = (void*)Hook_Stub; // OnKill
    s_VTable[4] = origVTable[4]; // dtor
    s_VTable[5] = origVTable[5];

    *(void***)&g_ScriptThread = s_VTable;

    g_pfnRegisterThread(&g_ScriptThread, hInstance);
    return true;
}

// Native Wrappers
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

static inline void SetCharVelocity(IV::Ped ped, float vx, float vy, float vz) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(vx);
    cxt.Push(vy);
    cxt.Push(vz);
    CallNative("SET_CHAR_VELOCITY", cxt);
}

static inline void ApplyForceToPed(IV::Ped ped, uint32_t type, float x, float y, float z,
                                   float spinX, float spinY, float spinZ,
                                   uint32_t u4, uint32_t u5, uint32_t u6, uint32_t u7) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(type);
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    cxt.Push(spinX);
    cxt.Push(spinY);
    cxt.Push(spinZ);
    cxt.Push(u4);
    cxt.Push(u5);
    cxt.Push(u6);
    cxt.Push(u7);
    CallNative("APPLY_FORCE_TO_PED", cxt);
}

static inline void GetCharSpeed(IV::Ped ped, float* speed) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(speed);
    CallNative("GET_CHAR_SPEED", cxt);
}

static inline void GetCharHeading(IV::Ped ped, float* heading) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(heading);
    CallNative("GET_CHAR_HEADING", cxt);
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

static inline bool IsCharTouchingChar(IV::Ped ped1, IV::Ped ped2) {
    NativeContext cxt;
    cxt.Push(ped1);
    cxt.Push(ped2);
    CallNative("IS_CHAR_TOUCHING_CHAR", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void SwitchPedToRagdoll(IV::Ped ped, int unk, int time, int f0, int f1, int f2, int f3) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(unk);
    cxt.Push(time);
    cxt.Push(f0);
    cxt.Push(f1);
    cxt.Push(f2);
    cxt.Push(f3);
    CallNative("SWITCH_PED_TO_RAGDOLL", cxt);
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

static inline void ShakePad(uint32_t pad, uint32_t time, uint32_t freq) {
    NativeContext cxt;
    cxt.Push(pad);
    cxt.Push(time);
    cxt.Push(freq);
    CallNative("SHAKE_PAD", cxt);
}

static inline bool GetCurrentCharWeapon(IV::Ped ped, uint32_t* pWeapon) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(pWeapon);
    CallNative("GET_CURRENT_CHAR_WEAPON", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool IsCharInMeleeCombat(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("IS_CHAR_IN_MELEE_COMBAT", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool GetCharMeleeActionFlag0(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("GET_CHAR_MELEE_ACTION_FLAG0", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool GetCharMeleeActionFlag1(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("GET_CHAR_MELEE_ACTION_FLAG1", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline bool HasCharBeenDamagedByChar(IV::Ped ped, IV::Ped otherPed) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(otherPed);
    cxt.Push(0);
    CallNative("HAS_CHAR_BEEN_DAMAGED_BY_CHAR", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void ClearCharLastDamageEntity(IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(ped);
    CallNative("CLEAR_CHAR_LAST_DAMAGE_ENTITY", cxt);
}

// Vehicle Natives
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

static inline bool IsCharTouchingVehicle(IV::Ped ped, IV::Vehicle vehicle) {
    NativeContext cxt;
    cxt.Push(ped);
    cxt.Push(vehicle);
    CallNative("IS_CHAR_TOUCHING_VEHICLE", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void ApplyForceToCar(IV::Vehicle vehicle, uint32_t type, float x, float y, float z,
                                   float spinX, float spinY, float spinZ,
                                   uint32_t u4, uint32_t u5, uint32_t u6, uint32_t u7) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(type);
    cxt.Push(x);
    cxt.Push(y);
    cxt.Push(z);
    cxt.Push(spinX);
    cxt.Push(spinY);
    cxt.Push(spinZ);
    cxt.Push(u4);
    cxt.Push(u5);
    cxt.Push(u6);
    cxt.Push(u7);
    CallNative("APPLY_FORCE_TO_CAR", cxt);
}

static inline uint32_t GetCarHealth(IV::Vehicle vehicle) {
    NativeContext cxt;
    uint32_t hp = 0;
    cxt.Push(vehicle);
    cxt.Push(&hp);
    CallNative("GET_CAR_HEALTH", cxt);
    return hp;
}

static inline void SetCarHealth(IV::Vehicle vehicle, uint32_t hp) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(hp);
    CallNative("SET_CAR_HEALTH", cxt);
}

static inline bool HasCarBeenDamagedByChar(IV::Vehicle vehicle, IV::Ped ped) {
    NativeContext cxt;
    cxt.Push(vehicle);
    cxt.Push(ped);
    CallNative("HAS_CAR_BEEN_DAMAGED_BY_CHAR", cxt);
    return cxt.GetResult<int>() != 0;
}

static inline void ClearCarLastDamageEntity(IV::Vehicle vehicle) {
    NativeContext cxt;
    cxt.Push(vehicle);
    CallNative("CLEAR_CAR_LAST_DAMAGE_ENTITY", cxt);
}

} // namespace GTA

// Mod Configuration
struct Config {
    bool enabled;
    int triggerMode; // 1 = Punch Only, 2 = Sprint Only, 3 = Both (Punch & Sprint)
    float minRunningSpeed;
    
    // NPC settings
    bool affectNPC;
    float launchForce;
    float upwardForce;
    int ragdollDuration;
    int impactDamage;

    // Vehicle settings
    bool affectVehicles;
    float vehLaunchForce;
    float vehUpwardForce;
    int vehDamage;

    // General
    bool screenShake;
};

static Config g_Config = {
    true,   // enabled
    1,      // triggerMode: 1 = Punch Only (Meninju / Melee Attack)
    3.5f,   // minRunningSpeed (for sprint mode)
    true,   // affectNPC
    38.0f,  // launchForce
    9.5f,   // upwardForce
    6000,   // ragdollDuration
    40,     // impactDamage
    true,   // affectVehicles
    75.0f,  // vehLaunchForce
    20.0f,  // vehUpwardForce
    180,    // vehDamage
    true    // screenShake
};

// Fixed size cooldown trackers
struct CooldownItem {
    uint32_t handle;
    DWORD time;
};

#define MAX_COOLDOWNS 64
static CooldownItem g_RecentPeds[MAX_COOLDOWNS];
static int g_PedCooldownCount = 0;

static CooldownItem g_RecentCars[MAX_COOLDOWNS];
static int g_CarCooldownCount = 0;

static inline bool IsItemOnCooldown(CooldownItem* list, int& count, uint32_t handle, DWORD now, DWORD duration) {
    for (int i = 0; i < count; i++) {
        if (list[i].handle == handle) {
            if (now - list[i].time < duration) {
                return true;
            } else {
                list[i] = list[count - 1];
                count--;
                i--;
            }
        }
    }
    return false;
}

static inline void AddItemCooldown(CooldownItem* list, int& count, uint32_t handle, DWORD now) {
    if (count < MAX_COOLDOWNS) {
        list[count].handle = handle;
        list[count].time = now;
        count++;
    } else {
        list[0].handle = handle;
        list[0].time = now;
    }
}

// INI Float reader
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
    
    const char* iniName = "TruckTackle.ini";
    int len = 0;
    while (iniPath[len] != '\0') len++;
    int j = 0;
    while (iniName[j] != '\0') {
        iniPath[len + j] = iniName[j];
        j++;
    }
    iniPath[len + j] = '\0';

    g_Config.enabled = GetPrivateProfileIntA("SETTINGS", "Enabled", 1, iniPath) != 0;
    g_Config.triggerMode = GetPrivateProfileIntA("SETTINGS", "TriggerMode", 1, iniPath);
    g_Config.minRunningSpeed = ReadIniFloat("SETTINGS", "MinRunningSpeed", 3.5f, iniPath);
    
    // NPC settings
    g_Config.affectNPC = GetPrivateProfileIntA("SETTINGS", "AffectNPC", 1, iniPath) != 0;
    g_Config.launchForce = ReadIniFloat("SETTINGS", "LaunchForce", 38.0f, iniPath);
    g_Config.upwardForce = ReadIniFloat("SETTINGS", "UpwardForce", 9.5f, iniPath);
    g_Config.ragdollDuration = GetPrivateProfileIntA("SETTINGS", "RagdollDuration", 6000, iniPath);
    g_Config.impactDamage = GetPrivateProfileIntA("SETTINGS", "ImpactDamage", 40, iniPath);

    // Vehicle settings
    g_Config.affectVehicles = GetPrivateProfileIntA("SETTINGS", "AffectVehicles", 1, iniPath) != 0;
    g_Config.vehLaunchForce = ReadIniFloat("SETTINGS", "VehicleLaunchForce", 75.0f, iniPath);
    g_Config.vehUpwardForce = ReadIniFloat("SETTINGS", "VehicleUpwardForce", 20.0f, iniPath);
    g_Config.vehDamage = GetPrivateProfileIntA("SETTINGS", "VehicleDamage", 180, iniPath);

    // Feedback
    g_Config.screenShake = GetPrivateProfileIntA("SETTINGS", "ScreenShake", 1, iniPath) != 0;
}

static unsigned long g_RndSeed = 987654321;
static inline int FastRand() {
    g_RndSeed = (1103515245 * g_RndSeed + 12345);
    return (int)((g_RndSeed / 65536) % 32768);
}

static DWORD s_LastPunchTime = 0;

static void OnTick() {
    if (!g_Config.enabled) return;

    IV::Player player = GTA::GetPlayerId();
    IV::Ped playerPed = GTA::GetPlayerPed(player);
    if (!GTA::DoesCharExist(playerPed) || GTA::IsCharDead(playerPed) || GTA::IsCharInAnyCar(playerPed)) {
        return;
    }

    DWORD now = GetTickCount();

    // Check weapon and attack inputs
    uint32_t currentWeapon = 0;
    GTA::GetCurrentCharWeapon(playerPed, &currentWeapon);

    bool isLeftClick = (MyGetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool isMeleeKey = (MyGetAsyncKeyState('R') & 0x8000) != 0 || (MyGetAsyncKeyState('F') & 0x8000) != 0;
    bool inCombat = GTA::IsCharInMeleeCombat(playerPed) || 
                    GTA::GetCharMeleeActionFlag0(playerPed) || 
                    GTA::GetCharMeleeActionFlag1(playerPed);

    // Register punch when player attacks with unarmed/melee weapon or in melee combat
    if ((currentWeapon <= 3 && (isLeftClick || isMeleeKey)) || inCombat) {
        s_LastPunchTime = now;
    }

    bool isPunchActive = (now - s_LastPunchTime < 450);

    float speed = 0.0f;
    GTA::GetCharSpeed(playerPed, &speed);
    bool isRunning = (speed >= g_Config.minRunningSpeed);

    // Determine if condition met based on TriggerMode
    bool shouldTrigger = false;
    if (g_Config.triggerMode == 1) {
        // Mode 1: Punch Only
        shouldTrigger = isPunchActive;
    } else if (g_Config.triggerMode == 2) {
        // Mode 2: Sprint Only
        shouldTrigger = isRunning;
    } else if (g_Config.triggerMode == 3) {
        // Mode 3: Both Punch and Sprint
        shouldTrigger = isPunchActive || isRunning;
    }

    if (!shouldTrigger) {
        return;
    }

    float px = 0.0f, py = 0.0f, pz = 0.0f;
    GTA::GetCharCoordinates(playerPed, &px, &py, &pz);

    float heading = 0.0f;
    GTA::GetCharHeading(playerPed, &heading);

    float rad = heading * (PI_CONST / 180.0f);
    float fwdX = -MySin(rad);
    float fwdY = MyCos(rad);

    // ==========================================
    // 1. TACKLE / PUNCH NPC
    // ==========================================
    if (g_Config.affectNPC) {
        struct Point { float x, y, z; };
        Point samplePoints[5] = {
            { px + fwdX * 0.9f, py + fwdY * 0.9f, pz },
            { px + fwdX * 1.6f, py + fwdY * 1.6f, pz },
            { px, py, pz },
            { px + (fwdX - fwdY * 0.5f) * 0.8f, py + (fwdY + fwdX * 0.5f) * 0.8f, pz },
            { px + (fwdX + fwdY * 0.5f) * 0.8f, py + (fwdY - fwdX * 0.5f) * 0.8f, pz }
        };

        for (int i = 0; i < 5; i++) {
            IV::Ped targetPed = 0;
            if (!GTA::GetClosestChar(samplePoints[i].x, samplePoints[i].y, samplePoints[i].z, 2.5f, 1, 1, &targetPed)) {
                continue;
            }

            if (!targetPed || targetPed == playerPed) continue;
            if (!GTA::DoesCharExist(targetPed) || GTA::IsCharDead(targetPed) || GTA::IsCharInAnyCar(targetPed)) continue;

            if (IsItemOnCooldown(g_RecentPeds, g_PedCooldownCount, targetPed, now, 1200)) continue;

            float tx = 0.0f, ty = 0.0f, tz = 0.0f;
            GTA::GetCharCoordinates(targetPed, &tx, &ty, &tz);

            float dx = tx - px;
            float dy = ty - py;
            float dz = tz - pz;
            float dist2D = MySqrt(dx * dx + dy * dy);

            bool isTouching = GTA::IsCharTouchingChar(playerPed, targetPed);
            bool wasDamaged = GTA::HasCharBeenDamagedByChar(targetPed, playerPed);

            // Punch check or distance check
            if (!wasDamaged && !isTouching && (dist2D > 2.5f || (dz * dz > 2.2f * 2.2f))) continue;

            float dot = (dx * fwdX + dy * fwdY);
            if (!wasDamaged && !isTouching && dot < 0.0f) continue;

            // Trigger Super Punch / Tackle on NPC
            AddItemCooldown(g_RecentPeds, g_PedCooldownCount, targetPed, now);
            GTA::ClearCharLastDamageEntity(targetPed);

            float dirX = fwdX;
            float dirY = fwdY;
            if (dist2D > 0.01f) {
                dirX = (dx / dist2D) * 0.7f + fwdX * 0.3f;
                dirY = (dy / dist2D) * 0.7f + fwdY * 0.3f;
                float dlen = MySqrt(dirX * dirX + dirY * dirY);
                if (dlen > 0.001f) {
                    dirX /= dlen;
                    dirY /= dlen;
                }
            }

            GTA::SwitchPedToRagdoll(targetPed, 0, g_Config.ragdollDuration, 1, 1, 1, 0);

            float vx = dirX * g_Config.launchForce;
            float vy = dirY * g_Config.launchForce;
            float vz = g_Config.upwardForce;
            GTA::SetCharVelocity(targetPed, vx, vy, vz);

            float spinX = ((float)(FastRand() % 200) - 100.0f) * 0.2f;
            float spinY = ((float)(FastRand() % 200) - 100.0f) * 0.2f;
            float spinZ = ((float)(FastRand() % 200) - 100.0f) * 0.2f;
            GTA::ApplyForceToPed(targetPed, 3, vx * 1.3f, vy * 1.3f, vz * 1.3f, spinX, spinY, spinZ, 0, 1, 1, 1);

            if (g_Config.impactDamage > 0) {
                uint32_t hp = GTA::GetCharHealth(targetPed);
                if (hp > (uint32_t)g_Config.impactDamage) {
                    GTA::SetCharHealth(targetPed, hp - g_Config.impactDamage);
                } else {
                    GTA::SetCharHealth(targetPed, 1);
                }
            }

            if (g_Config.screenShake) {
                GTA::ShakePad(0, 200, 240);
            }
        }
    }

    // ==========================================
    // 2. TACKLE / PUNCH VEHICLES
    // ==========================================
    if (g_Config.affectVehicles) {
        struct Point { float x, y, z; };
        Point carSamplePoints[3] = {
            { px + fwdX * 1.4f, py + fwdY * 1.4f, pz },
            { px + fwdX * 2.4f, py + fwdY * 2.4f, pz },
            { px, py, pz }
        };

        for (int i = 0; i < 3; i++) {
            IV::Vehicle car = GTA::GetClosestCar(carSamplePoints[i].x, carSamplePoints[i].y, carSamplePoints[i].z, 4.0f);
            if (!car || !GTA::DoesVehicleExist(car) || GTA::IsCarDead(car)) {
                continue;
            }

            if (IsItemOnCooldown(g_RecentCars, g_CarCooldownCount, car, now, 1500)) continue;

            float cx = 0.0f, cy = 0.0f, cz = 0.0f;
            GTA::GetCarCoordinates(car, &cx, &cy, &cz);

            float cdx = cx - px;
            float cdy = cy - py;
            float cdz = cz - pz;
            float carDist2D = MySqrt(cdx * cdx + cdy * cdy);

            bool isTouchingCar = GTA::IsCharTouchingVehicle(playerPed, car);
            bool carDamaged = GTA::HasCarBeenDamagedByChar(car, playerPed);

            if (!carDamaged && !isTouchingCar && (carDist2D > 3.8f || (cdz * cdz > 3.0f * 3.0f))) continue;

            float carDot = (cdx * fwdX + cdy * fwdY);
            if (!carDamaged && !isTouchingCar && carDot < -0.1f) continue;

            // Trigger Super Punch / Massive Launch on Vehicle
            AddItemCooldown(g_RecentCars, g_CarCooldownCount, car, now);
            GTA::ClearCarLastDamageEntity(car);

            float dirX = fwdX;
            float dirY = fwdY;
            if (carDist2D > 0.05f) {
                dirX = (cdx / carDist2D) * 0.7f + fwdX * 0.3f;
                dirY = (cdy / carDist2D) * 0.7f + fwdY * 0.3f;
                float dlen = MySqrt(dirX * dirX + dirY * dirY);
                if (dlen > 0.001f) {
                    dirX /= dlen;
                    dirY /= dlen;
                }
            }

            float forceX = dirX * g_Config.vehLaunchForce;
            float forceY = dirY * g_Config.vehLaunchForce;
            float forceZ = g_Config.vehUpwardForce;

            // Random torque spin to make car flip/tumble in air
            float spinX = ((float)(FastRand() % 200) - 100.0f) * 0.35f;
            float spinY = ((float)(FastRand() % 200) - 100.0f) * 0.35f;
            float spinZ = ((float)(FastRand() % 200) - 100.0f) * 0.35f;

            GTA::ApplyForceToCar(car, 3, forceX * 1.5f, forceY * 1.5f, forceZ * 1.5f, spinX, spinY, spinZ, 0, 1, 1, 1);

            if (g_Config.vehDamage > 0) {
                uint32_t hp = GTA::GetCarHealth(car);
                if (hp > (uint32_t)g_Config.vehDamage) {
                    GTA::SetCarHealth(car, hp - g_Config.vehDamage);
                } else {
                    GTA::SetCarHealth(car, 50);
                }
            }

            if (g_Config.screenShake) {
                GTA::ShakePad(0, 320, 255);
            }
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
