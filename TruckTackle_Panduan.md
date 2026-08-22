# 🥊 TruckTackle Mod — Panduan Lengkap

Mod **TruckTackle** adalah plugin ASI untuk **GTA IV Complete Edition** yang membuat NPC dan kendaraan terlempar jauh saat ditinju / ditabrak karakter.

---

## 📁 Struktur File

```
~/Documents/Games/
├── TruckTackle.cpp              ← Source code utama mod
└── TruckTackle_Panduan.md       ← File panduan ini

~/Documents/Games/Grand Theft Auto IV Complete Edition/GTAIV/
├── TruckTackle.asi              ← File mod yang dimuat game
├── TruckTackle.ini              ← Konfigurasi mod (bisa diedit tanpa kompilasi ulang)
└── plugins/
    ├── TruckTackle.asi          ← Salinan file mod
    └── TruckTackle.ini          ← Salinan konfigurasi
```

---

## ⚙️ Cara Edit Konfigurasi (TANPA Kompilasi Ulang)

Buka file `TruckTackle.ini` di folder game. Kamu bisa mengedit nilai-nilai berikut:

### `TriggerMode` — Mode Pemicu

| Nilai | Mode | Keterangan |
|-------|------|------------|
| `1` | **Punch Only** (Default) | Efek HANYA aktif saat meninju (klik kiri) |
| `2` | **Sprint Only** | Efek HANYA aktif saat lari kencang menabrak target |
| `3` | **Both** | Efek aktif saat meninju ATAU lari kencang menabrak |

```ini
TriggerMode = 1
```

### `MinRunningSpeed` — Kecepatan Lari Minimum (TriggerMode 2/3)

Kecepatan dalam unit game:
- `1.5` = jalan cepat / jogging ringan
- `3.5` = lari kencang (default)
- `5.0` = sprint penuh

```ini
MinRunningSpeed = 3.5
```

---

### Pengaturan NPC / Pejalan Kaki

| Parameter | Default | Keterangan |
|-----------|---------|------------|
| `AffectNPC` | `1` | `1` = aktif, `0` = nonaktif |
| `LaunchForce` | `38.0` | Kekuatan dorongan horizontal (seberapa jauh NPC terlempar) |
| `UpwardForce` | `9.5` | Kekuatan dorongan ke atas (seberapa tinggi NPC melayang) |
| `RagdollDuration` | `6000` | Durasi ragdoll dalam milidetik (6000 = 6 detik) |
| `ImpactDamage` | `40` | Damage yang diterima NPC saat terkena tinju |

> **Tips:** Naikkan `LaunchForce` ke `60.0` dan `UpwardForce` ke `15.0` buat efek yang lebih ekstrem!

---

### Pengaturan Kendaraan (Mobil, Truk, Motor, dll)

| Parameter | Default | Keterangan |
|-----------|---------|------------|
| `AffectVehicles` | `1` | `1` = aktif, `0` = nonaktif |
| `VehicleLaunchForce` | `75.0` | Kekuatan dorongan horizontal kendaraan |
| `VehicleUpwardForce` | `20.0` | Kekuatan dorongan ke atas kendaraan |
| `VehicleDamage` | `180` | Damage yang diterima bodi kendaraan |

> **Tips:** Set `VehicleLaunchForce = 120.0` dan `VehicleUpwardForce = 35.0` buat mobil terbang jauh banget!

---

### Pengaturan Feedback

| Parameter | Default | Keterangan |
|-----------|---------|------------|
| `ScreenShake` | `1` | `1` = efek getar kamera aktif, `0` = nonaktif |

---

## 🔧 Cara Edit Source Code & Kompilasi Ulang

Jika ingin mengubah logika/kode mod (bukan sekadar angka konfigurasi), edit file `TruckTackle.cpp` lalu kompilasi ulang.

### Prasyarat

Pastikan sudah terinstall cross-compiler MinGW 32-bit:

```bash
# Ubuntu/Debian/Mint
sudo apt install gcc-mingw-w64-i686 g++-mingw-w64-i686
```

### Perintah Kompilasi

```bash
i686-w64-mingw32-g++ \
  -shared -nostdlib -e _DllMain@12 \
  -o ~/Documents/Games/TruckTackle.asi \
  ~/Documents/Games/TruckTackle.cpp \
  -lkernel32 -O3 -s
```

> **⚠️ PENTING:** Jangan tambahkan flag `-lm`, `-lstdc++`, `-lpthread`, atau include header standar C++ seperti `<vector>`, `<map>`, `<cmath>` karena akan menyebabkan **Error 126** (dependency missing) saat game dimuat.

### Deploy ke Game Setelah Kompilasi

```bash
# Copy ke folder plugins GTA IV
cp ~/Documents/Games/TruckTackle.asi \
   ~/Documents/Games/"Grand Theft Auto IV Complete Edition"/GTAIV/plugins/TruckTackle.asi

# Copy juga ke root folder game (sebagai fallback)
cp ~/Documents/Games/TruckTackle.asi \
   ~/Documents/Games/"Grand Theft Auto IV Complete Edition"/GTAIV/TruckTackle.asi
```

---

## 🗺️ Bagian-Bagian Penting Source Code

### 1. Konstanta / Parameter Default (sekitar baris 300-an)

```cpp
static Config g_Config = {
    true,   // enabled
    1,      // triggerMode: 1=Punch, 2=Sprint, 3=Both
    3.5f,   // minRunningSpeed
    true,   // affectNPC
    38.0f,  // launchForce   <-- kekuatan lempar NPC
    9.5f,   // upwardForce   <-- kekuatan ke atas NPC
    6000,   // ragdollDuration
    40,     // impactDamage
    true,   // affectVehicles
    75.0f,  // vehLaunchForce  <-- kekuatan lempar kendaraan
    20.0f,  // vehUpwardForce  <-- kekuatan ke atas kendaraan
    180,    // vehDamage
    true    // screenShake
};
```

### 2. Deteksi Tinju (sekitar baris 430-an)

```cpp
// Deteksi saat meninju (klik kiri atau dalam mode combat)
bool isLeftClick = (MyGetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
bool inCombat = GTA::IsCharInMeleeCombat(playerPed) || 
                GTA::GetCharMeleeActionFlag0(playerPed) || 
                GTA::GetCharMeleeActionFlag1(playerPed);

// Tinju dianggap aktif selama 450ms setelah tombol ditekan
bool isPunchActive = (now - s_LastPunchTime < 450);
```

> Ubah angka `450` jadi lebih besar (misal `800`) jika mau window waktu tinju lebih lama.

### 3. Kekuatan Lempar NPC (sekitar baris 490-an)

```cpp
GTA::SwitchPedToRagdoll(targetPed, 0, g_Config.ragdollDuration, 1, 1, 1, 0);
GTA::SetCharVelocity(targetPed, vx, vy, vz);
GTA::ApplyForceToPed(targetPed, 3, vx * 1.3f, vy * 1.3f, vz * 1.3f, ...);
```

> Ganti multiplier `1.3f` jadi `2.0f` atau lebih besar untuk gaya dorong ekstra.

### 4. Kekuatan Lempar Kendaraan (sekitar baris 540-an)

```cpp
GTA::ApplyForceToCar(car, 3, forceX * 1.5f, forceY * 1.5f, forceZ * 1.5f, spinX, spinY, spinZ, ...);
```

> Ganti `1.5f` jadi `3.0f` untuk mobil terbang lebih jauh.

---

## ❓ FAQ

**Q: Apakah bisa ganti tombol dari klik kiri ke tombol lain?**

A: Bisa! Edit bagian ini di source code:
```cpp
bool isLeftClick = (MyGetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
// Ganti VK_LBUTTON dengan:
// VK_RBUTTON  = klik kanan
// VK_XBUTTON1 = tombol samping mouse 1
// 'E'         = tombol E keyboard
// VK_SPACE    = tombol spasi
```

**Q: Apakah muncul Error 126 lagi setelah kompilasi?**

A: Pastikan tidak menggunakan `-lm`, `-lstdc++`, dan tidak include `<vector>`, `<map>`, `<string>` atau header C++ standar lainnya.

**Q: Apakah efek tinju bisa beda dengan efek tabrakan lari?**

A: Saat ini memakai parameter yang sama. Untuk membedakan, tambahkan variabel config terpisah misalnya `PunchLaunchForce` vs `SprintLaunchForce`.

---

*Mod dibuat khusus untuk GTA IV Complete Edition (ScriptHook 0.5.1) di Linux/Wine/Lutris.*
