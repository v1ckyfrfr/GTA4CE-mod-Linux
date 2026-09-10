// SuperpowerPack.cpp - GTA IV Complete Edition chaos superpower collection
// F5 opens the in-game menu. Designed for Aru ScriptHook 0.5.1 and Wine/Lutris.
// Build with i686-w64-mingw32-g++ as a DLL, using -nostdlib, -lkernel32,
// -O3, -s, and the _DllMain@12 entry point.

#include <windows.h>
#include <stdint.h>

#define PI_CONST 3.14159265358979323846f

typedef SHORT (WINAPI *fnGetAsyncKeyState)(int);
static SHORT KeyState(int key) {
    static fnGetAsyncKeyState fn = NULL;
    if (!fn) {
        HMODULE h = GetModuleHandleA("user32.dll");
        if (!h) h = LoadLibraryA("user32.dll");
        if (h) fn = (fnGetAsyncKeyState)GetProcAddress(h, "GetAsyncKeyState");
    }
    return fn ? fn(key) : 0;
}

static float MySqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float r; __asm__ __volatile__("fsqrt" : "=t"(r) : "0"(x)); return r;
}
static float MySin(float x) {
    float r; __asm__ __volatile__("fsin" : "=t"(r) : "0"(x)); return r;
}
static float MyCos(float x) {
    float r; __asm__ __volatile__("fcos" : "=t"(r) : "0"(x)); return r;
}
static float MyAbs(float x) { return x < 0.0f ? -x : x; }

extern "C" void* memcpy(void* d, const void* s, size_t n) {
    uint8_t* dd = (uint8_t*)d; const uint8_t* ss = (const uint8_t*)s;
    for (size_t i = 0; i < n; ++i) dd[i] = ss[i];
    return d;
}
extern "C" void* memset(void* d, int c, size_t n) {
    uint8_t* dd = (uint8_t*)d;
    for (size_t i = 0; i < n; ++i) dd[i] = (uint8_t)c;
    return d;
}
extern "C" size_t strlen(const char* s) {
    size_t n = 0; while (s && s[n]) ++n; return n;
}

struct scrNativeCallContext {
    void* m_pReturn;
    unsigned int m_nArgCount;
    void* m_pArgs;
    unsigned int m_nDataCount;
};
typedef void (__cdecl *NativeHandler)(scrNativeCallContext*);

class NativeContext : public scrNativeCallContext {
    uint8_t stack[32 * 4];
public:
    NativeContext() { Reset(); }
    void Reset() {
        m_pReturn = stack; m_pArgs = stack; m_nArgCount = 0; m_nDataCount = 0;
        memset(stack, 0, sizeof(stack));
    }
    template<typename T> void Push(T value) {
        uint32_t v = 0;
        if (sizeof(T) <= 4) *(T*)&v = value;
        else v = (uint32_t)(uintptr_t)value;
        *(uint32_t*)(stack + m_nArgCount * 4) = v;
        ++m_nArgCount;
    }
    template<typename T> T Result() { return *(T*)m_pReturn; }
};

namespace IV {
typedef uint32_t Player;
typedef uint32_t Ped;
typedef uint32_t Vehicle;
typedef uint32_t Cam;
}

typedef void* (*fnGetNativeAddress)(const char*);
typedef void (__thiscall *fnThreadCtor)(void*);
typedef void (*fnRegisterThread)(void*, HINSTANCE);
static HMODULE g_Hook = NULL;
static fnGetNativeAddress g_GetNative = NULL;
static fnThreadCtor g_ThreadCtor = NULL;
static fnRegisterThread g_RegisterThread = NULL;

static bool InitHook() {
    if (!g_Hook) {
        g_Hook = GetModuleHandleA("ScriptHook.dll");
        if (!g_Hook) g_Hook = LoadLibraryA("ScriptHook.dll");
        if (g_Hook) {
            g_GetNative = (fnGetNativeAddress)GetProcAddress(g_Hook, "?GetNativeAddress@Game@@SAPAXPBD@Z");
            g_ThreadCtor = (fnThreadCtor)GetProcAddress(g_Hook, "??0ScriptThread@@QAE@XZ");
            g_RegisterThread = (fnRegisterThread)GetProcAddress(g_Hook, "?RegisterThread@ScriptHookManager@@SAXPAVScriptThread@@PAUHINSTANCE__@@@Z");
        }
    }
    return g_GetNative && g_ThreadCtor && g_RegisterThread;
}
static void CallNative(const char* name, NativeContext& c) {
    if (!g_GetNative) return;
    void* h = g_GetNative(name);
    if (h) ((NativeHandler)h)(&c);
}

typedef void (*TickCallback)();
struct GCCThread { void* vtable[6]; void* context; TickCallback tick; };
static GCCThread g_Thread;
static void __attribute__((thiscall)) HookTick(GCCThread* t) { if (t && t->tick) t->tick(); }
static void __attribute__((thiscall)) HookStub(GCCThread*) {}
static bool RegisterScript(TickCallback cb, HINSTANCE instance) {
    if (!InitHook()) return false;
    g_ThreadCtor(&g_Thread);
    g_Thread.tick = cb;
    static void* table[6];
    void** old = *(void***)&g_Thread;
    table[0] = (void*)HookTick; table[1] = old[1];
    table[2] = (void*)HookStub; table[3] = (void*)HookStub;
    table[4] = old[4]; table[5] = old[5];
    *(void***)&g_Thread = table;
    g_RegisterThread(&g_Thread, instance);
    return true;
}

namespace GTA {
static IV::Player PlayerId() { NativeContext c; CallNative("GET_PLAYER_ID", c); return c.Result<IV::Player>(); }
static IV::Ped PlayerPed(IV::Player player) {
    NativeContext c; IV::Ped ped = 0; c.Push(player); c.Push(&ped);
    CallNative("GET_PLAYER_CHAR", c); return ped;
}
static bool CharExists(IV::Ped ped) {
    if (!ped) return false;
    NativeContext c; c.Push(ped); CallNative("DOES_CHAR_EXIST", c); return c.Result<int>() != 0;
}
static bool CharDead(IV::Ped ped) {
    if (!ped) return true;
    NativeContext c; c.Push(ped); CallNative("IS_CHAR_DEAD", c); return c.Result<int>() != 0;
}
static bool CharInCar(IV::Ped ped) {
    NativeContext c; c.Push(ped); CallNative("IS_CHAR_IN_ANY_CAR", c); return c.Result<int>() != 0;
}
static bool CharInMelee(IV::Ped ped) {
    NativeContext c; c.Push(ped); CallNative("IS_CHAR_IN_MELEE_COMBAT", c); return c.Result<int>() != 0;
}
static bool CharShooting(IV::Ped ped) {
    NativeContext c; c.Push(ped); CallNative("IS_CHAR_SHOOTING", c); return c.Result<int>() != 0;
}
static void CharCoords(IV::Ped ped, float* x, float* y, float* z) {
    NativeContext c; c.Push(ped); c.Push(x); c.Push(y); c.Push(z); CallNative("GET_CHAR_COORDINATES", c);
}
static void CharHeading(IV::Ped ped, float* h) {
    NativeContext c; c.Push(ped); c.Push(h); CallNative("GET_CHAR_HEADING", c);
}
static void CharVelocity(IV::Ped ped, float* x, float* y, float* z) {
    NativeContext c; c.Push(ped); c.Push(x); c.Push(y); c.Push(z); CallNative("GET_CHAR_VELOCITY", c);
}
static void SetCharVelocity(IV::Ped ped, float x, float y, float z) {
    NativeContext c; c.Push(ped); c.Push(x); c.Push(y); c.Push(z); CallNative("SET_CHAR_VELOCITY", c);
}
static bool CurrentWeapon(IV::Ped ped, uint32_t* weapon) {
    NativeContext c; c.Push(ped); c.Push(weapon); CallNative("GET_CURRENT_CHAR_WEAPON", c); return c.Result<int>() != 0;
}
static bool PlayerTargetingChar(IV::Player player, IV::Ped ped) {
    NativeContext c; c.Push(player); c.Push(ped); CallNative("IS_PLAYER_TARGETTING_CHAR", c); return c.Result<int>() != 0;
}
static bool PlayerFreeAimingAtChar(IV::Player player, IV::Ped ped) {
    NativeContext c; c.Push(player); c.Push(ped); CallNative("IS_PLAYER_FREE_AIMING_AT_CHAR", c); return c.Result<int>() != 0;
}
static void Ragdoll(IV::Ped ped, int ms) {
    NativeContext unlock; unlock.Push(ped); unlock.Push(1); CallNative("UNLOCK_RAGDOLL", unlock);
    NativeContext c; c.Push(ped); c.Push(0); c.Push(ms); c.Push(1); c.Push(1); c.Push(1); c.Push(0);
    CallNative("SWITCH_PED_TO_RAGDOLL", c);
}
static void ForcePed(IV::Ped ped, float x, float y, float z, float sx, float sy, float sz) {
    NativeContext c; c.Push(ped); c.Push(3); c.Push(x); c.Push(y); c.Push(z);
    c.Push(sx); c.Push(sy); c.Push(sz); c.Push(0); c.Push(0); c.Push(1); c.Push(1);
    CallNative("APPLY_FORCE_TO_PED", c);
}
static bool ClosestCharFlags(float x, float y, float z, float radius,
                             uint32_t flag1, uint32_t flag2, IV::Ped* out) {
    // Scenario peds (workers, vendors, seated NPCs, etc.) are excluded from
    // some world searches unless this is enabled for the immediately next command.
    NativeContext allow; allow.Push(1); CallNative("ALLOW_SCENARIO_PEDS_TO_BE_RETURNED_BY_NEXT_COMMAND", allow);
    NativeContext c; c.Push(x); c.Push(y); c.Push(z); c.Push(radius); c.Push(flag1); c.Push(flag2); c.Push(out);
    CallNative("GET_CLOSEST_CHAR", c); return c.Result<int>() != 0;
}
static bool ClosestChar(float x, float y, float z, float radius, IV::Ped* out) {
    return ClosestCharFlags(x, y, z, radius, 1, 1, out);
}
static void SetCharInvincible(IV::Ped ped, bool enabled) {
    NativeContext c; c.Push(ped); c.Push(enabled ? 1 : 0); CallNative("SET_CHAR_INVINCIBLE", c);
}
static void SetPlayerInvincible(IV::Player player, bool enabled) {
    NativeContext c; c.Push(player); c.Push(enabled ? 1 : 0); CallNative("SET_PLAYER_INVINCIBLE", c);
}
static IV::Vehicle CarUsed(IV::Ped ped) {
    NativeContext c; IV::Vehicle car = 0; c.Push(ped); c.Push(&car); CallNative("GET_CAR_CHAR_IS_USING", c); return car;
}
static IV::Vehicle ClosestCarFlag(float x, float y, float z, float radius, uint32_t flags) {
    NativeContext c; c.Push(x); c.Push(y); c.Push(z); c.Push(radius); c.Push(0); c.Push(flags);
    CallNative("GET_CLOSEST_CAR", c); return c.Result<IV::Vehicle>();
}
static bool CarExists(IV::Vehicle car) {
    if (!car) return false;
    NativeContext c; c.Push(car); CallNative("DOES_VEHICLE_EXIST", c); return c.Result<int>() != 0;
}
static bool CarDead(IV::Vehicle car) {
    if (!car) return true;
    NativeContext c; c.Push(car); CallNative("IS_CAR_DEAD", c); return c.Result<int>() != 0;
}
static void CarCoords(IV::Vehicle car, float* x, float* y, float* z) {
    NativeContext c; c.Push(car); c.Push(x); c.Push(y); c.Push(z); CallNative("GET_CAR_COORDINATES", c);
}
static void SetCarCoords(IV::Vehicle car, float x, float y, float z) {
    NativeContext c; c.Push(car); c.Push(x); c.Push(y); c.Push(z); CallNative("SET_CAR_COORDINATES", c);
}
static void FreezeCar(IV::Vehicle car, bool frozen) {
    NativeContext c; c.Push(car); c.Push(frozen ? 1 : 0); CallNative("FREEZE_CAR_POSITION", c);
}
static void ForceCar(IV::Vehicle car, float x, float y, float z, float sx, float sy, float sz) {
    NativeContext c; c.Push(car); c.Push(3); c.Push(x); c.Push(y); c.Push(z);
    c.Push(sx); c.Push(sy); c.Push(sz); c.Push(0); c.Push(0); c.Push(1); c.Push(1);
    CallNative("APPLY_FORCE_TO_CAR", c);
}
static bool GameCamera(IV::Cam* cam) {
    NativeContext c; c.Push(cam); CallNative("GET_GAME_CAM", c); return *cam != 0;
}
static bool GameCameraChild(IV::Cam* cam) {
    NativeContext c; c.Push(cam); CallNative("GET_GAME_CAM_CHILD", c); return *cam != 0;
}
static void CamPosition(IV::Cam cam, float* x, float* y, float* z) {
    NativeContext c; c.Push(cam); c.Push(x); c.Push(y); c.Push(z); CallNative("GET_CAM_POS", c);
}
static void CamRotation(IV::Cam cam, float* x, float* y, float* z) {
    NativeContext c; c.Push(cam); c.Push(x); c.Push(y); c.Push(z); CallNative("GET_CAM_ROT", c);
}
static void TextScale(float x, float y) { NativeContext c; c.Push(x); c.Push(y); CallNative("SET_TEXT_SCALE", c); }
static void TextColour(int r, int g, int b, int a) {
    NativeContext c; c.Push(r); c.Push(g); c.Push(b); c.Push(a); CallNative("SET_TEXT_COLOUR", c);
}
static void TextCentre(bool enabled) { NativeContext c; c.Push(enabled ? 1 : 0); CallNative("SET_TEXT_CENTRE", c); }
static void TextShadow(int d, int r, int g, int b, int a) {
    NativeContext c; c.Push(d); c.Push(r); c.Push(g); c.Push(b); c.Push(a); CallNative("SET_TEXT_DROPSHADOW", c);
}
static void TextFont(int font) { NativeContext c; c.Push(font); CallNative("SET_TEXT_FONT", c); }
static void TextWrap(float x, float y) { NativeContext c; c.Push(x); c.Push(y); CallNative("SET_TEXT_WRAP", c); }
static void DisplayText(float x, float y, const char* text) {
    NativeContext c; c.Push(x); c.Push(y); c.Push("STRING"); c.Push(text); CallNative("DISPLAY_TEXT_WITH_LITERAL_STRING", c);
}
static void Notify(const char* text) {
    NativeContext c; c.Push("STRING"); c.Push(text); c.Push(2200); c.Push(1); CallNative("PRINT_STRING_WITH_LITERAL_STRING_NOW", c);
}
static void Shake(int ms, int frequency) {
    NativeContext c; c.Push(0); c.Push(ms); c.Push(frequency); CallNative("SHAKE_PAD", c);
}
}

struct Config {
    bool enabled;
    bool invincible;
    bool superPunch;
    bool blastPistol;
    bool gravityGun;
    bool telekinesis;
    bool superJump;
    bool vehicleBoost;
    bool chaosAura;
    int targetMode;   // 1 NPC, 2 vehicle, 3 both
    int powerLevel;   // 1 fun, 2 chaos, 3 apocalypse
    int gravityMode;  // 1 push, 2 pull, 3 launch, 4 grab
    int menuKey;
    int pullKey;
    int shockwaveKey;
    int boostKey;
    int gravityCycleKey;
    int gravityActionKey;
    int gravityLaunchKey;
};
static Config g_Cfg = {
    true, true, true, true, true, true, true, true, false,
    3, 2, 1, VK_F5, 'G', 'H', 'B', 'J', 'K', 'L'
};
static char g_IniPath[MAX_PATH];

static void BuildIniPath() {
    GetModuleFileNameA(NULL, g_IniPath, MAX_PATH);
    char* slash = NULL;
    for (int i = 0; g_IniPath[i]; ++i) if (g_IniPath[i] == '\\' || g_IniPath[i] == '/') slash = &g_IniPath[i];
    if (slash) *(slash + 1) = '\0';
    int n = 0; while (g_IniPath[n]) ++n;
    const char* name = "SuperpowerPack.ini";
    int i = 0; while (name[i] && n + i < MAX_PATH - 1) { g_IniPath[n + i] = name[i]; ++i; }
    g_IniPath[n + i] = '\0';
}
static bool ReadBool(const char* key, bool fallback) {
    return GetPrivateProfileIntA("POWERS", key, fallback ? 1 : 0, g_IniPath) != 0;
}
static void LoadConfig() {
    BuildIniPath();
    g_Cfg.enabled = ReadBool("Enabled", true);
    g_Cfg.invincible = ReadBool("Invincibility", true);
    g_Cfg.superPunch = ReadBool("SuperPunch", true);
    g_Cfg.blastPistol = ReadBool("BlastPistol", true);
    g_Cfg.gravityGun = ReadBool("GravityGun", true);
    g_Cfg.telekinesis = ReadBool("Telekinesis", true);
    g_Cfg.superJump = ReadBool("SuperJump", true);
    g_Cfg.vehicleBoost = ReadBool("VehicleBoost", true);
    g_Cfg.chaosAura = ReadBool("ChaosAura", false);
    g_Cfg.targetMode = GetPrivateProfileIntA("POWERS", "TargetMode", 3, g_IniPath);
    g_Cfg.powerLevel = GetPrivateProfileIntA("POWERS", "PowerLevel", 2, g_IniPath);
    g_Cfg.gravityMode = GetPrivateProfileIntA("POWERS", "GravityMode", 1, g_IniPath);
    g_Cfg.menuKey = GetPrivateProfileIntA("CONTROLS", "MenuKey", VK_F5, g_IniPath);
    g_Cfg.pullKey = GetPrivateProfileIntA("CONTROLS", "TelekinesisKey", 'G', g_IniPath);
    g_Cfg.shockwaveKey = GetPrivateProfileIntA("CONTROLS", "ShockwaveKey", 'H', g_IniPath);
    g_Cfg.boostKey = GetPrivateProfileIntA("CONTROLS", "VehicleBoostKey", 'B', g_IniPath);
    g_Cfg.gravityCycleKey = GetPrivateProfileIntA("CONTROLS", "GravityCycleKey", 'J', g_IniPath);
    g_Cfg.gravityActionKey = GetPrivateProfileIntA("CONTROLS", "GravityActionKey", 'K', g_IniPath);
    g_Cfg.gravityLaunchKey = GetPrivateProfileIntA("CONTROLS", "GravityLaunchKey", 'L', g_IniPath);
    if (g_Cfg.targetMode < 1 || g_Cfg.targetMode > 3) g_Cfg.targetMode = 3;
    if (g_Cfg.powerLevel < 1 || g_Cfg.powerLevel > 3) g_Cfg.powerLevel = 2;
    if (g_Cfg.gravityMode < 1 || g_Cfg.gravityMode > 4) g_Cfg.gravityMode = 1;
}
static void SaveValue(const char* key, bool v) {
    WritePrivateProfileStringA("POWERS", key, v ? "1" : "0", g_IniPath);
}
static void SaveConfig() {
    SaveValue("Enabled", g_Cfg.enabled);
    SaveValue("Invincibility", g_Cfg.invincible);
    SaveValue("SuperPunch", g_Cfg.superPunch);
    SaveValue("BlastPistol", g_Cfg.blastPistol);
    SaveValue("GravityGun", g_Cfg.gravityGun);
    SaveValue("Telekinesis", g_Cfg.telekinesis);
    SaveValue("SuperJump", g_Cfg.superJump);
    SaveValue("VehicleBoost", g_Cfg.vehicleBoost);
    SaveValue("ChaosAura", g_Cfg.chaosAura);
    const char* target = g_Cfg.targetMode == 1 ? "1" : (g_Cfg.targetMode == 2 ? "2" : "3");
    const char* level = g_Cfg.powerLevel == 1 ? "1" : (g_Cfg.powerLevel == 2 ? "2" : "3");
    const char* gravity = g_Cfg.gravityMode == 1 ? "1" : (g_Cfg.gravityMode == 2 ? "2" : (g_Cfg.gravityMode == 3 ? "3" : "4"));
    WritePrivateProfileStringA("POWERS", "TargetMode", target, g_IniPath);
    WritePrivateProfileStringA("POWERS", "PowerLevel", level, g_IniPath);
    WritePrivateProfileStringA("POWERS", "GravityMode", gravity, g_IniPath);
}
static float PowerMult() { return g_Cfg.powerLevel == 1 ? 0.75f : (g_Cfg.powerLevel == 2 ? 1.25f : 2.0f); }
static bool AffectPeds() { return g_Cfg.targetMode == 1 || g_Cfg.targetMode == 3; }
static bool AffectCars() { return g_Cfg.targetMode == 2 || g_Cfg.targetMode == 3; }

static void Forward(IV::Ped ped, float* x, float* y) {
    float heading = 0.0f; GTA::CharHeading(ped, &heading);
    float rad = heading * (PI_CONST / 180.0f);
    *x = -MySin(rad); *y = MyCos(rad);
}
static float Dist2D(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by; return MySqrt(dx * dx + dy * dy);
}
static IV::Vehicle ClosestCarAny(float x, float y, float z, float radius) {
    const uint32_t flags[3] = { 69, 70, 71 };
    IV::Vehicle best = 0;
    float bestDist = 1000000.0f;
    for (int i = 0; i < 3; ++i) {
        IV::Vehicle car = GTA::ClosestCarFlag(x, y, z, radius, flags[i]);
        if (!GTA::CarExists(car) || GTA::CarDead(car)) continue;
        float cx = 0, cy = 0, cz = 0;
        GTA::CarCoords(car, &cx, &cy, &cz);
        float dx = cx - x, dy = cy - y, dz = cz - z;
        float dist = dx * dx + dy * dy + dz * dz;
        if (dist < bestDist) { bestDist = dist; best = car; }
    }
    return best;
}
static bool AddUniqueHandle(uint32_t* handles, int* count, int capacity, uint32_t handle) {
    if (!handle) return false;
    for (int i = 0; i < *count; ++i) if (handles[i] == handle) return false;
    if (*count < capacity) handles[(*count)++] = handle;
    return true;
}

static IV::Ped FindFrontPed(IV::Ped player, float range, float radius) {
    float px = 0, py = 0, pz = 0, fx = 0, fy = 0;
    GTA::CharCoords(player, &px, &py, &pz); Forward(player, &fx, &fy);
    IV::Ped best = 0; float bestScore = 1000000.0f;
    uint32_t seen[96]; int seenCount = 0;
    const float sides[3] = { 0.0f, -1.0f, 1.0f };
    const uint32_t searchFlags[2][2] = { {1,1}, {0,0} };
    for (int i = 1; i <= 16; ++i) {
        float d = range * (float)i / 16.0f;
        for (int s = 0; s < 3; ++s) {
            float side = sides[s] * radius * 0.55f;
            float sx = px + fx * d - fy * side, sy = py + fy * d + fx * side;
            for (int f = 0; f < 2; ++f) {
                IV::Ped candidate = 0;
                if (!GTA::ClosestCharFlags(sx, sy, pz + 0.6f, radius,
                                           searchFlags[f][0], searchFlags[f][1], &candidate)) continue;
                if (!AddUniqueHandle(seen, &seenCount, 96, candidate)) continue;
                if (!candidate || candidate == player || !GTA::CharExists(candidate) || GTA::CharDead(candidate)) continue;
                if (GTA::CharInCar(candidate)) continue;
                float tx = 0, ty = 0, tz = 0; GTA::CharCoords(candidate, &tx, &ty, &tz);
                float dx = tx - px, dy = ty - py, along = dx * fx + dy * fy;
                if (along < 0.0f || along > range + radius) continue;
                float lateral = MyAbs(dx * (-fy) + dy * fx);
                if (lateral > radius * 1.9f) continue;
                float score = lateral * lateral + along * 0.02f;
                if (score < bestScore) { bestScore = score; best = candidate; }
            }
        }
    }
    return best;
}
static IV::Vehicle FindFrontCar(IV::Ped player, float range, float radius) {
    float px = 0, py = 0, pz = 0, fx = 0, fy = 0;
    GTA::CharCoords(player, &px, &py, &pz); Forward(player, &fx, &fy);
    IV::Vehicle own = GTA::CharInCar(player) ? GTA::CarUsed(player) : 0;
    IV::Vehicle best = 0; float bestScore = 1000000.0f;
    uint32_t seen[96]; int seenCount = 0;
    const float sides[3] = { 0.0f, -1.0f, 1.0f };
    const uint32_t flags[3] = { 69, 70, 71 };
    for (int i = 1; i <= 16; ++i) {
        float d = range * (float)i / 16.0f;
        for (int s = 0; s < 3; ++s) {
            float side = sides[s] * radius * 0.65f;
            float sx = px + fx * d - fy * side, sy = py + fy * d + fx * side;
            for (int f = 0; f < 3; ++f) {
                IV::Vehicle car = GTA::ClosestCarFlag(sx, sy, pz + 0.3f, radius + 2.5f, flags[f]);
                if (!AddUniqueHandle(seen, &seenCount, 96, car)) continue;
                if (!car || car == own || !GTA::CarExists(car) || GTA::CarDead(car)) continue;
                float tx = 0, ty = 0, tz = 0; GTA::CarCoords(car, &tx, &ty, &tz);
                float dx = tx - px, dy = ty - py, along = dx * fx + dy * fy;
                if (along < 0.0f || along > range + radius) continue;
                float lateral = MyAbs(dx * (-fy) + dy * fx);
                if (lateral > radius * 2.2f) continue;
                float score = lateral * lateral + along * 0.02f;
                if (score < bestScore) { bestScore = score; best = car; }
            }
        }
    }
    return best;
}

static uint32_t g_Done[128];
static int g_DoneCount = 0;
static bool Done(uint32_t h) {
    for (int i = 0; i < g_DoneCount; ++i) if (g_Done[i] == h) return true;
    return false;
}
static void MarkDone(uint32_t h) { if (g_DoneCount < 128) g_Done[g_DoneCount++] = h; }
static uint32_t g_Random = 0xC0FFEEu;
static int FastRand() { g_Random = 1664525u * g_Random + 1013904223u; return (int)((g_Random >> 16) & 0x7fff); }

static void LaunchPed(IV::Ped ped, float dx, float dy, float horizontal, float upward) {
    GTA::Ragdoll(ped, 6500);
    GTA::SetCharVelocity(ped, dx * horizontal, dy * horizontal, upward);
    float sx = ((float)(FastRand() % 200) - 100.0f) * 0.12f;
    float sy = ((float)(FastRand() % 200) - 100.0f) * 0.12f;
    GTA::ForcePed(ped, dx * horizontal * 1.25f, dy * horizontal * 1.25f, upward * 1.25f, sx, sy, 0.0f);
}
static void LaunchCar(IV::Vehicle car, float dx, float dy, float horizontal, float upward) {
    float spin = PowerMult();
    float sx = ((float)(FastRand() % 200) - 100.0f) * 0.35f * spin;
    float sy = ((float)(FastRand() % 200) - 100.0f) * 0.35f * spin;
    float sz = ((float)(FastRand() % 200) - 100.0f) * 0.22f * spin;
    GTA::ForceCar(car, dx * horizontal * 1.5f, dy * horizontal * 1.5f, upward * 1.5f, sx, sy, sz);
}

static DWORD g_CarHoldTick = 0;
static void BeginCarControl(IV::Vehicle car) {
    if (!GTA::CarExists(car)) return;
    GTA::FreezeCar(car, true);
    g_CarHoldTick = GetTickCount();
}
static void EndCarControl(IV::Vehicle car) {
    if (GTA::CarExists(car)) GTA::FreezeCar(car, false);
    g_CarHoldTick = 0;
}
static void MoveCarStable(IV::Vehicle car, float holdX, float holdY, float holdZ, float maxSpeed) {
    if (!GTA::CarExists(car)) return;
    float cx = 0, cy = 0, cz = 0;
    GTA::CarCoords(car, &cx, &cy, &cz);
    float dx = holdX - cx, dy = holdY - cy, dz = holdZ - cz;
    float dist = MySqrt(dx * dx + dy * dy + dz * dz);
    if (dist <= 0.025f) {
        GTA::SetCarCoords(car, holdX, holdY, holdZ);
        GTA::FreezeCar(car, true);
        return;
    }
    DWORD now = GetTickCount();
    float dt = g_CarHoldTick ? (float)(now - g_CarHoldTick) * 0.001f : 0.016f;
    if (dt < 0.004f) dt = 0.004f;
    if (dt > 0.050f) dt = 0.050f;
    g_CarHoldTick = now;
    float speed = dist * 3.5f;
    if (speed < 2.0f) speed = 2.0f;
    if (speed > maxSpeed) speed = maxSpeed;
    float step = speed * dt;
    if (step > dist) step = dist;
    float scale = step / dist;
    GTA::FreezeCar(car, true);
    GTA::SetCarCoords(car, cx + dx * scale, cy + dy * scale, cz + dz * scale);
}

static void Shockwave(IV::Ped player, bool mega) {
    float px = 0, py = 0, pz = 0; GTA::CharCoords(player, &px, &py, &pz);
    IV::Vehicle own = GTA::CharInCar(player) ? GTA::CarUsed(player) : 0;
    float mult = PowerMult() * (mega ? 1.65f : 1.0f);
    float radius = mega ? 34.0f : 22.0f;
    g_DoneCount = 0;
    const float heights[3] = { -1.0f, 0.8f, 2.5f };
    for (int a = 0; a < 12; ++a) {
        float angle = (float)a * (PI_CONST * 2.0f / 12.0f);
        float ux = MyCos(angle), uy = MySin(angle);
        for (int d = 1; d <= 5; ++d) {
            float dist = radius * (float)d / 5.0f;
            for (int h = 0; h < 3; ++h) {
                float sx = px + ux * dist, sy = py + uy * dist, sz = pz + heights[h];
                if (AffectPeds()) {
                    IV::Ped ped = 0;
                    if (GTA::ClosestChar(sx, sy, sz, radius / 5.0f + 2.2f, &ped) && ped && ped != player &&
                        GTA::CharExists(ped) && !GTA::CharDead(ped) && !Done(ped)) {
                        float tx = 0, ty = 0, tz = 0; GTA::CharCoords(ped, &tx, &ty, &tz);
                        float dx = tx - px, dy = ty - py, actual = MySqrt(dx * dx + dy * dy);
                        if (actual <= radius) {
                            if (actual > 0.05f) { dx /= actual; dy /= actual; } else { dx = ux; dy = uy; }
                            MarkDone(ped); LaunchPed(ped, dx, dy, 42.0f * mult, 14.0f * mult);
                        }
                    }
                }
                if (AffectCars()) {
                    IV::Vehicle car = ClosestCarAny(sx, sy, sz, radius / 5.0f + 2.8f);
                    if (car && car != own && GTA::CarExists(car) && !GTA::CarDead(car) && !Done(car)) {
                        float tx = 0, ty = 0, tz = 0; GTA::CarCoords(car, &tx, &ty, &tz);
                        float dx = tx - px, dy = ty - py, actual = MySqrt(dx * dx + dy * dy);
                        if (actual <= radius) {
                            if (actual > 0.05f) { dx /= actual; dy /= actual; } else { dx = ux; dy = uy; }
                            MarkDone(car); LaunchCar(car, dx, dy, 60.0f * mult, 18.0f * mult);
                        }
                    }
                }
            }
        }
    }
    GTA::Shake(mega ? 450 : 260, mega ? 255 : 210);
    GTA::Notify(mega ? "~r~MEGA SHOCKWAVE!~w~" : "~y~FORCE SHOCKWAVE!~w~");
}

enum HoldType { HOLD_NONE, HOLD_PED, HOLD_CAR };
static HoldType g_HoldType = HOLD_NONE;
static IV::Ped g_HeldPed = 0;
static IV::Vehicle g_HeldCar = 0;
static bool g_PrevPull = false;
static void ReleaseHold(bool throwIt, float fx, float fy) {
    float mult = PowerMult();
    if (throwIt && g_HoldType == HOLD_PED && GTA::CharExists(g_HeldPed)) LaunchPed(g_HeldPed, fx, fy, 50.0f * mult, 11.0f * mult);
    if (g_HoldType == HOLD_CAR && GTA::CarExists(g_HeldCar)) {
        EndCarControl(g_HeldCar);
        if (throwIt) LaunchCar(g_HeldCar, fx, fy, 68.0f * mult, 16.0f * mult);
    }
    g_HoldType = HOLD_NONE; g_HeldPed = 0; g_HeldCar = 0;
}
static void UpdateTelekinesis(IV::Ped player) {
    bool down = (KeyState(g_Cfg.pullKey) & 0x8000) != 0;
    float fx = 0, fy = 0; Forward(player, &fx, &fy);
    if (down && !g_PrevPull && g_HoldType == HOLD_NONE) {
        IV::Ped ped = AffectPeds() ? FindFrontPed(player, 32.0f, 5.0f) : 0;
        IV::Vehicle car = AffectCars() ? FindFrontCar(player, 32.0f, 6.0f) : 0;
        float px = 0, py = 0, pz = 0; GTA::CharCoords(player, &px, &py, &pz);
        float pd = 100000.0f, cd = 100000.0f;
        if (ped) { float x=0,y=0,z=0; GTA::CharCoords(ped,&x,&y,&z); pd=Dist2D(px,py,x,y); }
        if (car) { float x=0,y=0,z=0; GTA::CarCoords(car,&x,&y,&z); cd=Dist2D(px,py,x,y); }
        if (ped && pd <= cd) { g_HoldType = HOLD_PED; g_HeldPed = ped; GTA::Ragdoll(ped, 10000); }
        else if (car) { g_HoldType = HOLD_CAR; g_HeldCar = car; BeginCarControl(car); }
        if (g_HoldType != HOLD_NONE) GTA::Notify("~b~TELEKINESIS LOCKED~w~");
    }
    if (down && g_HoldType != HOLD_NONE) {
        float px=0,py=0,pz=0; GTA::CharCoords(player,&px,&py,&pz);
        float hx=px+fx*4.0f, hy=py+fy*4.0f, hz=pz+1.4f;
        float tx=0,ty=0,tz=0;
        if (g_HoldType == HOLD_PED && GTA::CharExists(g_HeldPed)) {
            GTA::CharCoords(g_HeldPed,&tx,&ty,&tz); GTA::Ragdoll(g_HeldPed,10000);
            GTA::SetCharVelocity(g_HeldPed,(hx-tx)*12.0f,(hy-ty)*12.0f,(hz-tz)*12.0f);
        } else if (g_HoldType == HOLD_CAR && GTA::CarExists(g_HeldCar)) {
            MoveCarStable(g_HeldCar,hx,hy,hz,18.0f);
        } else ReleaseHold(false,fx,fy);
    }
    if (!down && g_PrevPull && g_HoldType != HOLD_NONE) { ReleaseHold(true,fx,fy); GTA::Shake(180,190); }
    g_PrevPull = down;
}

enum GravityTargetType { GRAVITY_NONE, GRAVITY_PED, GRAVITY_CAR };
static GravityTargetType g_GravityType = GRAVITY_NONE;
static IV::Ped g_GravityPed = 0;
static IV::Vehicle g_GravityCar = 0;
static bool g_PrevGravityCycle = false;
static bool g_PrevGravityAction = false;
static bool g_PrevGravityLaunch = false;

static void ClearGravityTarget() {
    if (g_GravityType == GRAVITY_CAR && GTA::CarExists(g_GravityCar))
        EndCarControl(g_GravityCar);
    g_GravityType = GRAVITY_NONE;
    g_GravityPed = 0;
    g_GravityCar = 0;
}

static bool GravityGunAiming(IV::Player playerId, IV::Ped player) {
    (void)playerId;
    uint32_t weapon = 0;
    GTA::CurrentWeapon(player, &weapon);
    bool firearm = weapon == 7 || (weapon >= 9 && weapon <= 18);
    bool aiming = (KeyState(VK_RBUTTON) & 0x8000) != 0;
    return firearm && aiming;
}

static bool GetAimRay(IV::Ped player, float* ox, float* oy, float* oz,
                      float* dx, float* dy, float* dz) {
    IV::Cam cam = 0;
    if (!GTA::GameCameraChild(&cam)) GTA::GameCamera(&cam);
    if (cam) {
        float rx = 0, ry = 0, rz = 0;
        GTA::CamPosition(cam, ox, oy, oz);
        GTA::CamRotation(cam, &rx, &ry, &rz);
        float pitch = rx * (PI_CONST / 180.0f);
        float yaw = rz * (PI_CONST / 180.0f);
        float cp = MyCos(pitch);
        *dx = -MySin(yaw) * cp;
        *dy =  MyCos(yaw) * cp;
        *dz =  MySin(pitch);
        float length = MySqrt(*dx * *dx + *dy * *dy + *dz * *dz);
        if (length > 0.01f) { *dx /= length; *dy /= length; *dz /= length; return true; }
    }
    GTA::CharCoords(player, ox, oy, oz); *oz += 1.1f;
    Forward(player, dx, dy); *dz = 0.0f;
    return false;
}

static float AimScore(float ox, float oy, float oz, float dx, float dy, float dz,
                      float tx, float ty, float tz, float* alongOut) {
    float vx = tx - ox, vy = ty - oy, vz = tz - oz;
    float along = vx * dx + vy * dy + vz * dz;
    float totalSq = vx * vx + vy * vy + vz * vz;
    float perpSq = totalSq - along * along;
    if (perpSq < 0.0f) perpSq = 0.0f;
    *alongOut = along;
    return perpSq + along * 0.0005f;
}

static bool AcquireGravityTarget(IV::Player playerId, IV::Ped player) {
    float ox = 0, oy = 0, oz = 0, dx = 0, dy = 0, dz = 0;
    GetAimRay(player, &ox, &oy, &oz, &dx, &dy, &dz);
    IV::Ped bestPed = 0;
    IV::Vehicle bestCar = 0;
    float bestPedScore = 1000000.0f, bestCarScore = 1000000.0f;
    uint32_t seenPeds[96], seenCars[96];
    int seenPedCount = 0, seenCarCount = 0;
    IV::Vehicle own = GTA::CharInCar(player) ? GTA::CarUsed(player) : 0;
    const uint32_t pedFlags[2][2] = { {1,1}, {0,0} };
    const uint32_t carFlags[3] = { 69, 70, 71 };

    for (int i = 1; i <= 30; ++i) {
        float distance = 2.0f * (float)i;
        float sx = ox + dx * distance, sy = oy + dy * distance, sz = oz + dz * distance;
        if (AffectPeds()) {
            for (int f = 0; f < 2; ++f) {
                IV::Ped candidate = 0;
                if (!GTA::ClosestCharFlags(sx, sy, sz, 2.8f,
                                           pedFlags[f][0], pedFlags[f][1], &candidate) ||
                    !AddUniqueHandle(seenPeds, &seenPedCount, 96, candidate) ||
                    candidate == player || !GTA::CharExists(candidate) ||
                    GTA::CharDead(candidate)) continue;
                if (GTA::CharInCar(candidate)) continue;
                float tx = 0, ty = 0, tz = 0, along = 0;
                GTA::CharCoords(candidate, &tx, &ty, &tz); tz += 0.7f;
                float score = AimScore(ox, oy, oz, dx, dy, dz, tx, ty, tz, &along);
                bool exactAim = GTA::PlayerFreeAimingAtChar(playerId, candidate) || GTA::PlayerTargetingChar(playerId, candidate);
                float maxPerpendicular = exactAim ? 2.5f : (1.35f + along * 0.010f);
                if (along > 0.0f && along <= 62.0f && score <= maxPerpendicular * maxPerpendicular && score < bestPedScore) {
                    bestPedScore = score; bestPed = candidate;
                }
            }
        }
        if (AffectCars()) {
            for (int f = 0; f < 3; ++f) {
                IV::Vehicle candidate = GTA::ClosestCarFlag(sx, sy, sz, 6.5f, carFlags[f]);
                if (!AddUniqueHandle(seenCars, &seenCarCount, 96, candidate) ||
                    candidate == own || !GTA::CarExists(candidate) ||
                    GTA::CarDead(candidate)) continue;
                float tx = 0, ty = 0, tz = 0, along = 0;
                GTA::CarCoords(candidate, &tx, &ty, &tz); tz += 0.8f;
                float score = AimScore(ox, oy, oz, dx, dy, dz, tx, ty, tz, &along);
                float maxPerpendicular = 4.2f + along * 0.012f;
                if (along > 0.0f && along <= 62.0f && score <= maxPerpendicular * maxPerpendicular && score < bestCarScore) {
                    bestCarScore = score; bestCar = candidate;
                }
            }
        }
    }

    if (bestPed && (!bestCar || bestPedScore <= bestCarScore)) {
        g_GravityType = GRAVITY_PED; g_GravityPed = bestPed; GTA::Ragdoll(bestPed, 12000); return true;
    }
    if (bestCar) {
        g_GravityType = GRAVITY_CAR; g_GravityCar = bestCar;
        BeginCarControl(bestCar);
        return true;
    }
    return false;
}

static void MoveGravityTarget(IV::Ped player, float distance, float pullSpeed) {
    float px = 0, py = 0, pz = 0;
    float ox = 0, oy = 0, oz = 0, ax = 0, ay = 0, az = 0;
    GTA::CharCoords(player, &px, &py, &pz);
    GetAimRay(player, &ox, &oy, &oz, &ax, &ay, &az);

    // Put the hold point directly on the camera ray. The distance starts at
    // the player's depth along that ray, keeping the target in front of Niko
    // while allowing full horizontal and vertical crosshair control.
    float playerDepth = (px - ox) * ax + (py - oy) * ay + (pz - oz) * az;
    if (playerDepth < 1.0f || playerDepth > 12.0f)
        playerDepth = MySqrt((px - ox) * (px - ox) + (py - oy) * (py - oy) + (pz - oz) * (pz - oz));
    float holdDepth = playerDepth + distance;
    float hx = ox + ax * holdDepth;
    float hy = oy + ay * holdDepth;
    float hz = oz + az * holdDepth;
    float tx = 0, ty = 0, tz = 0;
    if (g_GravityType == GRAVITY_PED && GTA::CharExists(g_GravityPed)) {
        GTA::CharCoords(g_GravityPed, &tx, &ty, &tz); GTA::Ragdoll(g_GravityPed, 12000);
        GTA::SetCharVelocity(g_GravityPed, (hx - tx) * pullSpeed, (hy - ty) * pullSpeed, (hz - tz) * pullSpeed);
    } else if (g_GravityType == GRAVITY_CAR && GTA::CarExists(g_GravityCar)) {
        MoveCarStable(g_GravityCar, hx, hy, hz, pullSpeed * 1.5f);
    } else {
        ClearGravityTarget();
    }
}

static void FireGravityTarget(IV::Ped player, bool launchMode) {
    float ox = 0, oy = 0, oz = 0, ax = 0, ay = 0, az = 0;
    GetAimRay(player, &ox, &oy, &oz, &ax, &ay, &az);
    float mult = PowerMult();
    float force = (launchMode ? 92.0f : 44.0f) * mult;
    float vx = ax * force, vy = ay * force, vz = az * force;
    if (g_GravityType == GRAVITY_PED && GTA::CharExists(g_GravityPed)) {
        GTA::Ragdoll(g_GravityPed, 6500);
        GTA::SetCharVelocity(g_GravityPed, vx, vy, vz);
        GTA::ForcePed(g_GravityPed, vx * 1.25f, vy * 1.25f, vz * 1.25f, 0, 0, 0);
    }
    if (g_GravityType == GRAVITY_CAR && GTA::CarExists(g_GravityCar)) {
        EndCarControl(g_GravityCar);
        GTA::ForceCar(g_GravityCar, vx * 1.5f, vy * 1.5f, vz * 1.5f, 0, 0, 0);
    }
    ClearGravityTarget();
    GTA::Shake(launchMode ? 260 : 150, launchMode ? 240 : 190);
}

static void ShowGravityMode() {
    if (g_Cfg.gravityMode == 1) GTA::Notify("~b~GRAVITY GUN: PUSH~w~");
    else if (g_Cfg.gravityMode == 2) GTA::Notify("~b~GRAVITY GUN: PULL~w~");
    else if (g_Cfg.gravityMode == 3) GTA::Notify("~b~GRAVITY GUN: LAUNCH~w~");
    else GTA::Notify("~b~GRAVITY GUN: GRAB~w~");
}

static void UpdateGravityGun(IV::Player playerId, IV::Ped player) {
    bool cycle = (KeyState(g_Cfg.gravityCycleKey) & 0x8000) != 0;
    bool action = (KeyState(g_Cfg.gravityActionKey) & 0x8000) != 0;
    bool launch = (KeyState(g_Cfg.gravityLaunchKey) & 0x8000) != 0;
    if (cycle && !g_PrevGravityCycle) {
        ClearGravityTarget();
        ++g_Cfg.gravityMode; if (g_Cfg.gravityMode > 4) g_Cfg.gravityMode = 1;
        SaveConfig(); ShowGravityMode();
    }
    if (g_Cfg.gravityMode == 1 || g_Cfg.gravityMode == 3) {
        if (action && !g_PrevGravityAction) {
            if (!GravityGunAiming(playerId, player)) GTA::Notify("~r~AIM A FIREARM AT A TARGET~w~");
            else if (AcquireGravityTarget(playerId, player)) FireGravityTarget(player, g_Cfg.gravityMode == 3);
            else GTA::Notify("~r~NO TARGET ON CROSSHAIR~w~");
        }
    } else if (g_Cfg.gravityMode == 2) {
        if (action && !g_PrevGravityAction) {
            if (!GravityGunAiming(playerId, player)) GTA::Notify("~r~AIM A FIREARM AT A TARGET~w~");
            else if (!AcquireGravityTarget(playerId, player)) GTA::Notify("~r~NO TARGET ON CROSSHAIR~w~");
        }
        if (action && g_GravityType != GRAVITY_NONE) MoveGravityTarget(player, 2.2f, 14.0f);
        if (!action && g_PrevGravityAction) ClearGravityTarget();
    } else {
        if (action && !g_PrevGravityAction) {
            if (g_GravityType != GRAVITY_NONE) {
                ClearGravityTarget(); GTA::Notify("~w~GRAVITY TARGET RELEASED");
            } else if (!GravityGunAiming(playerId, player)) {
                GTA::Notify("~r~AIM A FIREARM AT A TARGET~w~");
            } else if (AcquireGravityTarget(playerId, player)) {
                GTA::Notify("~g~GRAVITY TARGET GRABBED~w~");
            } else GTA::Notify("~r~NO TARGET ON CROSSHAIR~w~");
        }
        if (g_GravityType != GRAVITY_NONE) {
            MoveGravityTarget(player, 4.2f, 12.0f);
            if (launch && !g_PrevGravityLaunch) FireGravityTarget(player, true);
        }
    }
    g_PrevGravityCycle = cycle;
    g_PrevGravityAction = action;
    g_PrevGravityLaunch = launch;
}

static DWORD g_LastPunch = 0, g_LastPunchHit = 0, g_LastShot = 0, g_LastAura = 0;
static bool g_PrevLeft = false, g_PrevJump = false, g_PrevBoost = false, g_PrevShock = false;
static void UpdatePunch(IV::Ped player, DWORD now) {
    bool left = (KeyState(VK_LBUTTON) & 0x8000) != 0;
    if ((left && !g_PrevLeft) || GTA::CharInMelee(player)) g_LastPunch = now;
    g_PrevLeft = left;
    if (now - g_LastPunch > 430 || now - g_LastPunchHit < 320) return;
    IV::Ped ped = AffectPeds() ? FindFrontPed(player, 4.5f, 2.2f) : 0;
    IV::Vehicle car = AffectCars() ? FindFrontCar(player, 5.5f, 3.0f) : 0;
    float fx=0,fy=0; Forward(player,&fx,&fy); float mult=PowerMult();
    if (ped) { LaunchPed(ped,fx,fy,38.0f*mult,10.0f*mult); g_LastPunchHit=now; GTA::Shake(150,190); }
    if (car) { LaunchCar(car,fx,fy,54.0f*mult,15.0f*mult); g_LastPunchHit=now; GTA::Shake(190,220); }
}
static void UpdateBlastPistol(IV::Ped player, DWORD now) {
    uint32_t weapon=0; GTA::CurrentWeapon(player,&weapon);
    if (weapon != 7 && weapon != 9) return;
    if (!GTA::CharShooting(player) || now - g_LastShot < 180) return;
    g_LastShot=now;
    IV::Ped ped = AffectPeds() ? FindFrontPed(player,42.0f,5.0f) : 0;
    IV::Vehicle car = AffectCars() ? FindFrontCar(player,42.0f,6.0f) : 0;
    float fx=0,fy=0; Forward(player,&fx,&fy); float mult=PowerMult(); bool hit=false;
    if (ped) { LaunchPed(ped,fx,fy,48.0f*mult,12.0f*mult); hit=true; }
    if (car) { LaunchCar(car,fx,fy,66.0f*mult,18.0f*mult); hit=true; }
    if (hit) GTA::Shake(120,190);
}
static void UpdateJump(IV::Ped player) {
    bool space=(KeyState(VK_SPACE)&0x8000)!=0, shift=(KeyState(VK_SHIFT)&0x8000)!=0;
    if (space && !g_PrevJump && shift && !GTA::CharInCar(player)) {
        float vx=0,vy=0,vz=0,fx=0,fy=0; GTA::CharVelocity(player,&vx,&vy,&vz); Forward(player,&fx,&fy);
        float mult=PowerMult(); GTA::SetCharVelocity(player,vx+fx*8.0f*mult,vy+fy*8.0f*mult,12.5f*mult);
        GTA::Notify("~b~SUPER JUMP!~w~");
    }
    g_PrevJump=space;
}
static void UpdateVehicleBoost(IV::Ped player) {
    bool down=(KeyState(g_Cfg.boostKey)&0x8000)!=0;
    if (down && !g_PrevBoost && GTA::CharInCar(player)) {
        IV::Vehicle car=GTA::CarUsed(player); float fx=0,fy=0; Forward(player,&fx,&fy); float mult=PowerMult();
        if (GTA::CarExists(car)) { GTA::ForceCar(car,fx*72.0f*mult,fy*72.0f*mult,3.5f*mult,0,0,0); GTA::Shake(160,210); }
    }
    g_PrevBoost=down;
}

static bool g_MenuOpen=false;
static int g_MenuRow=0;
static bool g_PrevMenu=false,g_PrevUp=false,g_PrevDown=false,g_PrevLeftMenu=false,g_PrevRight=false,g_PrevEnter=false,g_PrevBack=false;
static void DrawLine(float y,const char* text,bool selected) {
    GTA::TextScale(selected?0.35f:0.31f,selected?0.60f:0.53f);
    GTA::TextColour(selected?255:225,selected?205:225,selected?35:225,255);
    GTA::TextCentre(true); GTA::TextShadow(2,0,0,0,255); GTA::TextFont(1); GTA::TextWrap(0.0f,1.0f);
    GTA::DisplayText(0.5f,y,text);
}
static const char* ToggleText(bool value, const char* on, const char* off) {
    return value ? on : off;
}
static void DrawMenu() {
    DrawLine(0.100f,"~r~SUPERPOWER MOD PACK~w~",false);
    DrawLine(0.155f,ToggleText(g_Cfg.enabled,"MASTER POWER: ~g~ON~w~","MASTER POWER: ~r~OFF~w~"),g_MenuRow==0);
    DrawLine(0.190f,ToggleText(g_Cfg.invincible,"INVINCIBILITY: ~g~ON~w~","INVINCIBILITY: ~r~OFF~w~"),g_MenuRow==1);
    DrawLine(0.225f,ToggleText(g_Cfg.superPunch,"SUPER PUNCH: ~g~ON~w~","SUPER PUNCH: ~r~OFF~w~"),g_MenuRow==2);
    DrawLine(0.260f,ToggleText(g_Cfg.blastPistol,"BLAST PISTOL: ~g~ON~w~","BLAST PISTOL: ~r~OFF~w~"),g_MenuRow==3);
    DrawLine(0.295f,ToggleText(g_Cfg.gravityGun,"GRAVITY GUN: ~g~ON~w~","GRAVITY GUN: ~r~OFF~w~"),g_MenuRow==4);
    DrawLine(0.330f,ToggleText(g_Cfg.telekinesis,"TELEKINESIS: ~g~ON~w~","TELEKINESIS: ~r~OFF~w~"),g_MenuRow==5);
    DrawLine(0.365f,ToggleText(g_Cfg.superJump,"SUPER JUMP: ~g~ON~w~","SUPER JUMP: ~r~OFF~w~"),g_MenuRow==6);
    DrawLine(0.400f,ToggleText(g_Cfg.vehicleBoost,"VEHICLE BOOST: ~g~ON~w~","VEHICLE BOOST: ~r~OFF~w~"),g_MenuRow==7);
    DrawLine(0.435f,ToggleText(g_Cfg.chaosAura,"CHAOS AURA: ~g~ON~w~","CHAOS AURA: ~r~OFF~w~"),g_MenuRow==8);
    const char* gravity = g_Cfg.gravityMode==1 ? "GRAVITY MODE: PUSH" : (g_Cfg.gravityMode==2 ? "GRAVITY MODE: PULL" : (g_Cfg.gravityMode==3 ? "GRAVITY MODE: LAUNCH" : "GRAVITY MODE: GRAB"));
    DrawLine(0.470f,gravity,g_MenuRow==9);
    const char* target = g_Cfg.targetMode==1 ? "TARGETS: NPC" : (g_Cfg.targetMode==2 ? "TARGETS: VEHICLES" : "TARGETS: BOTH");
    DrawLine(0.505f,target,g_MenuRow==10);
    const char* level = g_Cfg.powerLevel==1 ? "POWER: FUN" : (g_Cfg.powerLevel==2 ? "POWER: CHAOS" : "POWER: APOCALYPSE");
    DrawLine(0.540f,level,g_MenuRow==11);
    DrawLine(0.575f,"TRIGGER MEGA SHOCKWAVE",g_MenuRow==12);
    DrawLine(0.610f,"SAVE & CLOSE",g_MenuRow==13);
    DrawLine(0.675f,"UP/DOWN SELECT  LEFT/RIGHT CHANGE  ENTER CONFIRM",false);
}
static void ToggleRow(int row) {
    if(row==0)g_Cfg.enabled=!g_Cfg.enabled;
    if(row==1)g_Cfg.invincible=!g_Cfg.invincible;
    if(row==2)g_Cfg.superPunch=!g_Cfg.superPunch;
    if(row==3)g_Cfg.blastPistol=!g_Cfg.blastPistol;
    if(row==4)g_Cfg.gravityGun=!g_Cfg.gravityGun;
    if(row==5)g_Cfg.telekinesis=!g_Cfg.telekinesis;
    if(row==6)g_Cfg.superJump=!g_Cfg.superJump;
    if(row==7)g_Cfg.vehicleBoost=!g_Cfg.vehicleBoost;
    if(row==8)g_Cfg.chaosAura=!g_Cfg.chaosAura;
}
static bool UpdateMenu(IV::Ped player) {
    bool menu=(KeyState(g_Cfg.menuKey)&0x8000)!=0;
    bool up=(KeyState(VK_UP)&0x8000)!=0, down=(KeyState(VK_DOWN)&0x8000)!=0;
    bool left=(KeyState(VK_LEFT)&0x8000)!=0, right=(KeyState(VK_RIGHT)&0x8000)!=0;
    bool enter=(KeyState(VK_RETURN)&0x8000)!=0;
    bool back=(KeyState(VK_ESCAPE)&0x8000)!=0 || (KeyState(VK_BACK)&0x8000)!=0;
    if(menu&&!g_PrevMenu){g_MenuOpen=!g_MenuOpen;if(g_MenuOpen){ClearGravityTarget();GTA::Notify("~r~SUPERPOWER MENU~w~");}}
    if(g_MenuOpen){
        if(up&&!g_PrevUp){--g_MenuRow;if(g_MenuRow<0)g_MenuRow=13;}
        if(down&&!g_PrevDown){++g_MenuRow;if(g_MenuRow>13)g_MenuRow=0;}
        bool change=(left&&!g_PrevLeftMenu)||(right&&!g_PrevRight)||(enter&&!g_PrevEnter);
        if(change){
            if(g_MenuRow<=8)ToggleRow(g_MenuRow);
            else if(g_MenuRow==9){g_Cfg.gravityMode += (left&&!g_PrevLeftMenu)?-1:1;if(g_Cfg.gravityMode<1)g_Cfg.gravityMode=4;if(g_Cfg.gravityMode>4)g_Cfg.gravityMode=1;}
            else if(g_MenuRow==10){g_Cfg.targetMode += (left&&!g_PrevLeftMenu)?-1:1;if(g_Cfg.targetMode<1)g_Cfg.targetMode=3;if(g_Cfg.targetMode>3)g_Cfg.targetMode=1;}
            else if(g_MenuRow==11){g_Cfg.powerLevel += (left&&!g_PrevLeftMenu)?-1:1;if(g_Cfg.powerLevel<1)g_Cfg.powerLevel=3;if(g_Cfg.powerLevel>3)g_Cfg.powerLevel=1;}
            else if(g_MenuRow==12 && enter&&!g_PrevEnter)Shockwave(player,true);
            else if(g_MenuRow==13 && enter&&!g_PrevEnter){SaveConfig();g_MenuOpen=false;GTA::Notify("~g~SUPERPOWER SETTINGS SAVED~w~");}
            if(g_MenuRow<=11)SaveConfig();
        }
        if(back&&!g_PrevBack){SaveConfig();g_MenuOpen=false;}
        if(g_MenuOpen)DrawMenu();
    }
    g_PrevMenu=menu;g_PrevUp=up;g_PrevDown=down;g_PrevLeftMenu=left;g_PrevRight=right;g_PrevEnter=enter;g_PrevBack=back;
    return g_MenuOpen;
}

static bool g_InvincibleApplied=false;
static void OnTick() {
    IV::Player player=GTA::PlayerId(); IV::Ped ped=GTA::PlayerPed(player);
    if(!GTA::CharExists(ped))return;
    bool menuOpen=UpdateMenu(ped);
    bool wantInvincible=g_Cfg.enabled&&g_Cfg.invincible;
    if(wantInvincible){GTA::SetPlayerInvincible(player,true);GTA::SetCharInvincible(ped,true);g_InvincibleApplied=true;}
    else if(g_InvincibleApplied){GTA::SetPlayerInvincible(player,false);GTA::SetCharInvincible(ped,false);g_InvincibleApplied=false;}
    if(menuOpen||!g_Cfg.enabled||GTA::CharDead(ped)){
        if(!g_Cfg.enabled||GTA::CharDead(ped)){ClearGravityTarget();if(g_HoldType!=HOLD_NONE){float fx=0,fy=0;Forward(ped,&fx,&fy);ReleaseHold(false,fx,fy);}}
        return;
    }
    DWORD now=GetTickCount();
    bool shock=(KeyState(g_Cfg.shockwaveKey)&0x8000)!=0;
    if(shock&&!g_PrevShock)Shockwave(ped,false);
    g_PrevShock=shock;
    if(g_Cfg.chaosAura&&now-g_LastAura>=2500){Shockwave(ped,false);g_LastAura=now;}
    if(g_Cfg.gravityGun&&g_HoldType==HOLD_NONE)UpdateGravityGun(player,ped);else if(g_GravityType!=GRAVITY_NONE)ClearGravityTarget();
    if(g_Cfg.telekinesis&&g_GravityType==GRAVITY_NONE)UpdateTelekinesis(ped);else if(g_HoldType!=HOLD_NONE){float fx=0,fy=0;Forward(ped,&fx,&fy);ReleaseHold(false,fx,fy);}
    if(g_Cfg.superPunch&&!GTA::CharInCar(ped))UpdatePunch(ped,now);
    if(g_Cfg.blastPistol)UpdateBlastPistol(ped,now);
    if(g_Cfg.superJump)UpdateJump(ped);
    if(g_Cfg.vehicleBoost)UpdateVehicleBoost(ped);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);LoadConfig();RegisterScript(OnTick,instance);}
    return TRUE;
}
