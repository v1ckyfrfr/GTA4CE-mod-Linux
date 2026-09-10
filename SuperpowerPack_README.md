# Superpower Mod Pack — GTA IV Complete Edition

A single ASI plugin featuring an in-game menu to control a collection of chaos superpowers. Built specifically for Aru ScriptHook 0.5.1, GTA IV Complete Edition, and Wine/Lutris without additional MinGW runtime dependencies.

## Installation

Copy `SuperpowerPack.asi` to `GTAIV/plugins/` and `SuperpowerPack.ini` to your `GTAIV/` root folder. Press **F5** in-game to open the menu.

## Controls

| Control                        | Function                                                          |
| ------------------------------ | ----------------------------------------------------------------- |
| F5                             | Open/close the Superpower menu                                    |
| Up/Down Arrow                  | Navigate menu items                                               |
| Left/Right Arrow               | Change option value                                               |
| Enter                          | Confirm or trigger Mega Shockwave                                 |
| Escape/Backspace               | Save and close menu                                               |
| G (hold/release)               | Pull, hold, then throw target with telekinesis                    |
| H                              | Radial Force Shockwave                                            |
| J                              | Cycle Gravity Gun mode: Push/Pull/Launch/Grab                     |
| Right Click + aim crosshair    | Aim at target with the currently held firearm                     |
| K                              | Select target at crosshair and use Gravity Gun; hold in Pull mode |
| L                              | Throw the currently held target in Grab mode                      |
| Shift + Space                  | Super Jump                                                        |
| B while driving                | Vehicle Boost                                                     |
| Shoot with Pistol/Desert Eagle | Blast Pistol                                                      |
| Punch target                   | Super Punch                                                       |

## Superpowers

- **Invincibility:** Protects the player while active.
- **Super Punch:** Throws hit NPCs and vehicles far away.
- **Blast Pistol:** Pistol (ID 7) and Desert Eagle (ID 9) fire force blast shockwaves.
- **Gravity Gun:** Transforms the currently held firearm into a Gravity Gun with four modes to push, pull, launch, or grab targets.
- **Telekinesis:** Pulls NPCs or vehicles, suspends them in front of the player, and throws them.
- **Super Jump:** Grants a massive leap combined with forward momentum.
- **Vehicle Boost:** Provides a massive speed boost to the player's vehicle.
- **Force Shockwave:** Knocks away NPCs and vehicles surrounding the player.
- **Chaos Aura:** Automatically triggers a shockwave every 2.5 seconds.

The menu allows configuring the target filter (`NPC`, `VEHICLES`, or `BOTH`), as well as the power level (`FUN`, `CHAOS`, or `APOCALYPSE`). Settings are automatically saved to `SuperpowerPack.ini`.

### Gravity Gun Modes

- **Push:** Press K to push targets away.
- **Pull:** Hold K to pull targets closer; release K to let go.
- **Launch:** Press K to launch targets forward and into the air with massive force.
- **Grab:** Press K to grab or release a target. While suspended, press L to launch it.

To select a new target, hold a firearm, hold right-click to aim, place the crosshair directly on an NPC or vehicle, and press K. Modes can be selected via the F5 GUI or cycled directly using J. The Gravity Gun shares the same target selection filter and power level settings as the other powers.

Pulled or held vehicles use smooth kinematic position updates to remain stable and follow the crosshair without oscillating left and right. Vehicles revert to normal physics right before being released, pushed, or launched.

The Push and Launch directions use the camera/crosshair 3D vector in world coordinates rather than the player's facing direction or target local coordinates. Therefore, the orientation of the vehicle or NPC does not affect the throw direction. Launch applies significantly more force than Push; the vertical direction of both follows the crosshair angle.

During Pull or Grab, the hold point lies directly along the crosshair ray. Moving the crosshair left, right, up, or down moves the target accordingly.

Target detection covers scenario NPCs and various GTA IV NPC/vehicle categories. When the crosshair hits a driver or passenger and vehicle targeting is enabled, the Gravity Gun prioritizes the vehicle so the force is not absorbed by an NPC seated inside. NPCs outside vehicles have their ragdoll unlocked before being affected by powers.

The Gravity Gun utilizes dense ray-casting samples, and Telekinesis sweeps the frontal area using multiple trajectories. Both check NPC search flags `0/1` and vehicle categories `69/70/71`, including emergency services vehicles.

## Building on Linux

```bash
i686-w64-mingw32-g++ -shared -nostdlib -e _DllMain@12 \
  -o SuperpowerPack.asi SuperpowerPack.cpp -lkernel32 -O3 -s
```

Verify build dependencies:

```bash
i686-w64-mingw32-objdump -p SuperpowerPack.asi | grep "DLL Name:"
```

A correct build should only output `KERNEL32.dll`.
