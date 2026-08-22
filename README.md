# 🥊 TruckTackle — GTA IV ASI Mod

> **One punch and they're gone.** TruckTackle is a custom ASI plugin for **GTA IV Complete Edition** that gives Niko superhuman punching and collision power — one melee hit sends NPCs and vehicles flying like they were hit by a freight truck.

![Platform](https://img.shields.io/badge/Platform-GTA%20IV%20Complete%20Edition-blue)
![Loader](https://img.shields.io/badge/ASI%20Loader-dinput8.dll-green)
![ScriptHook](https://img.shields.io/badge/ScriptHook-0.5.1%20Aru-orange)
![OS](https://img.shields.io/badge/OS-Windows%20%7C%20Linux%20Wine-lightgrey)

---

## 🎮 What Does This Mod Do?

When **TruckTackle** is installed, the game transforms Niko's basic melee attacks into devastating superhuman strikes:

- **Punch an NPC** → They instantly ragdoll and fly several meters through the air, spinning wildly before crashing down.
- **Punch a vehicle** → The car/truck/motorcycle gets launched into the air with massive force, flipping and tumbling mid-flight with heavy body damage.
- **Optional sprint mode** → If you prefer, the effect can also (or alternatively) trigger when Niko runs into targets at high speed.

The mod is fully configurable via an `.ini` file — no recompilation needed for tuning.

---

## 📋 Requirements

| Requirement | Details |
|-------------|---------|
| Game | GTA IV Complete Edition (v1.2.0.30 / v1.0.7.0) |
| ASI Loader | `dinput8.dll` (or `xlive.dll` replacement) |
| ScriptHook | ScriptHook 0.5.1 by Aru |
| OS | Windows or Linux (Wine / Lutris) |

---

## 📁 File Structure

```
~/Documents/Games/
├── TruckTackle.cpp            ← Full mod source code (C++)
├── TruckTackle_Panduan.md     ← Indonesian usage guide
└── README.md                  ← This file

~/Documents/Games/Grand Theft Auto IV Complete Edition/GTAIV/
├── TruckTackle.asi            ← Compiled mod plugin (loaded by game)
├── TruckTackle.ini            ← Configuration file
└── plugins/
    ├── TruckTackle.asi
    └── TruckTackle.ini
```

---

## 🚀 Installation

1. Make sure **ScriptHook 0.5.1** and an **ASI Loader** (`dinput8.dll`) are installed in your GTAIV folder.
2. Copy `TruckTackle.asi` and `TruckTackle.ini` to:
   - `GTAIV/` (game root), **and**
   - `GTAIV/plugins/` (for compatibility)
3. Launch the game — the mod loads automatically.

---

## ⚙️ Configuration (`TruckTackle.ini`)

All settings can be changed **without recompiling** — just edit the `.ini` file and restart the game.

```ini
[SETTINGS]

; Enable or disable the mod entirely
; 1 = enabled, 0 = disabled
Enabled = 1

; ────────────────────────────────────────────
;  TRIGGER MODE
; ────────────────────────────────────────────
; Controls when the launch effect activates:
;   1 = Punch Only (default) - only triggers on melee attack (left click)
;   2 = Sprint Only          - only triggers when running into a target
;   3 = Both                 - triggers on punch OR sprint collision
TriggerMode = 1

; Minimum speed required for sprint trigger (TriggerMode 2 or 3)
;   1.5 = fast walk / light jog
;   3.5 = full run (default)
;   5.0 = full sprint
MinRunningSpeed = 3.5

; ────────────────────────────────────────────
;  NPC / PEDESTRIAN SETTINGS
; ────────────────────────────────────────────
AffectNPC = 1

; Horizontal launch force (how far the NPC flies)
LaunchForce = 38.0

; Vertical launch force (how high the NPC goes)
UpwardForce = 9.5

; How long the ragdoll state lasts in milliseconds
RagdollDuration = 6000

; Direct health damage dealt on impact
ImpactDamage = 40

; ────────────────────────────────────────────
;  VEHICLE SETTINGS
; ────────────────────────────────────────────
AffectVehicles = 1

; Horizontal launch force on vehicles (how far the car flies)
VehicleLaunchForce = 75.0

; Vertical launch force on vehicles (how high the car goes)
VehicleUpwardForce = 20.0

; Direct body damage to the vehicle on impact
VehicleDamage = 180

; ────────────────────────────────────────────
;  FEEDBACK
; ────────────────────────────────────────────
; Camera/controller shake on hit
; 1 = enabled, 0 = disabled
ScreenShake = 1
```

### Configuration Presets

**🔥 Extreme Mode** — maximum chaos:
```ini
TriggerMode = 1
LaunchForce = 65.0
UpwardForce = 18.0
VehicleLaunchForce = 120.0
VehicleUpwardForce = 40.0
```

**🌀 Balanced Mode** — fun but not overpowered:
```ini
TriggerMode = 1
LaunchForce = 25.0
UpwardForce = 7.0
VehicleLaunchForce = 50.0
VehicleUpwardForce = 15.0
```

**🏃 Sprint Tackle Mode** — run into people like a truck:
```ini
TriggerMode = 2
MinRunningSpeed = 3.0
LaunchForce = 30.0
UpwardForce = 8.0
VehicleLaunchForce = 60.0
VehicleUpwardForce = 18.0
```

---

## 🔧 Building from Source

### Prerequisites

Install the MinGW i686 cross-compiler:

```bash
# Ubuntu / Debian / Linux Mint
sudo apt install gcc-mingw-w64-i686 g++-mingw-w64-i686
```

### Compile Command

```bash
i686-w64-mingw32-g++ \
  -shared -nostdlib -e _DllMain@12 \
  -o ~/Documents/Games/TruckTackle.asi \
  ~/Documents/Games/TruckTackle.cpp \
  -lkernel32 -O3 -s
```

> ⚠️ **Critical:** Do **NOT** add `-lm`, `-lstdc++`, `-lpthread`, or include standard C++ headers like `<vector>`, `<map>`, `<cmath>`, `<string>`. These pull in MinGW runtime DLLs (`libwinpthread-1.dll`, UCRT, etc.) which cause **Error 126** when the game tries to load the plugin.

### Deploy After Compiling

```bash
GTAIV="$HOME/Documents/Games/Grand Theft Auto IV Complete Edition/GTAIV"

cp ~/Documents/Games/TruckTackle.asi "$GTAIV/TruckTackle.asi"
cp ~/Documents/Games/TruckTackle.asi "$GTAIV/plugins/TruckTackle.asi"
```

### Verify Dependencies (should only show KERNEL32.dll)

```bash
i686-w64-mingw32-objdump -p ~/Documents/Games/TruckTackle.asi | grep "DLL Name:"
# Expected output:
#   DLL Name: KERNEL32.dll
```

---

## 🛠️ Source Code Overview

The mod is written in a single zero-dependency C++ file (`TruckTackle.cpp`) using only Win32 APIs from `KERNEL32.dll`.

### Key Architecture

| Component | Description |
|-----------|-------------|
| `NativeContext` | Custom GTA IV native call context (push args → call handler → read result) |
| `GCCScriptThread` | Manual vtable-based script thread compatible with GCC (replaces MSVC `ScriptThread`) |
| `GTA::*` namespace | Thin inline wrappers for every GTA IV native used |
| `LoadConfig()` | Custom INI parser — no CRT file I/O, uses `GetPrivateProfileStringA` (Win32) |
| `OnTick()` | Main per-frame logic: detects punch/sprint → scans nearby NPCs/vehicles → applies forces |
| Cooldown system | Fixed-size array-based cooldown tracker prevents repeated triggers on same entity |
| x87 math helpers | `MySqrt()`, `MySin()`, `MyCos()` — inline assembly FPU instructions, no `libm` |

### How the Punch Detection Works

```
Every frame:
  1. Get player ped + check alive & on foot
  2. Detect left mouse button (melee attack) or IsCharInMeleeCombat()
  3. If punch detected → record timestamp
  4. isPunchActive = (now - lastPunchTime < 450ms)
  5. If isPunchActive:
       → Scan 5 sample points ahead of player for nearby NPCs (radius 2.5m)
       → Scan 3 sample points ahead for nearby vehicles (radius 4.0m)
       → Check distance / dot product / IsCharTouching* / HasBeenDamagedBy*
       → Apply SwitchPedToRagdoll + SetCharVelocity + ApplyForceToPed
       → Apply ApplyForceToCar with random spin torque
       → Apply damage + screen shake
```

### Modifying Punch Detection Timing

In `TruckTackle.cpp`, find this line:

```cpp
bool isPunchActive = (now - s_LastPunchTime < 450);
```

Change `450` (milliseconds) to control how long after a punch the launch window stays open:
- `200` → very tight, must be close when punching
- `450` → default, good feel
- `800` → generous window, more forgiving

### Changing the Trigger Key

Find this section in `TruckTackle.cpp`:

```cpp
bool isLeftClick = (MyGetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
```

Replace `VK_LBUTTON` with any of:

| Constant | Key |
|----------|-----|
| `VK_RBUTTON` | Right mouse button |
| `VK_XBUTTON1` | Mouse side button 1 |
| `VK_XBUTTON2` | Mouse side button 2 |
| `'E'` | E key |
| `VK_SPACE` | Spacebar |
| `VK_SHIFT` | Shift key |
| `VK_CONTROL` | Ctrl key |

---

## ❓ Troubleshooting

### Error 126 — Missing dependency

**Cause:** The compiled `.asi` depends on a DLL that Wine doesn't have.

**Fix:** Always compile with **exactly** this command — no extra libs:
```bash
i686-w64-mingw32-g++ -shared -nostdlib -e _DllMain@12 \
  -o TruckTackle.asi TruckTackle.cpp -lkernel32 -O3 -s
```

Verify by running:
```bash
i686-w64-mingw32-objdump -p TruckTackle.asi | grep "DLL Name:"
# Must only show: KERNEL32.dll
```

### Mod loads but nothing happens

- Make sure `Enabled = 1` in `TruckTackle.ini`
- Make sure the game is running with ScriptHook loaded (check ScriptHook's own log)
- Try punching from very close range (< 1.5m from target)

### NPCs launch too far / not far enough

Edit `LaunchForce` and `UpwardForce` in `TruckTackle.ini`.

### Game crashes on startup

The `.asi` may have extra runtime dependencies. Recompile with the exact command above.

---

## 📜 License

This mod is free to use, modify, and redistribute for personal use.

---

*Built for GTA IV Complete Edition with ScriptHook 0.5.1 by Aru.*
*Compatible with Linux via Wine/Lutris.*
