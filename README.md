<p align="center">
  <img src="assets/aeroclicker.png" width="104" alt="AeroClicker">
</p>

<h1 align="center">AeroClicker</h1>

<p align="center">
  A humanized auto-clicker for Linux — Wayland and X11 — with a native KDE look.
</p>

<p align="center">
  <img src="assets/screenshot-light.png" width="49%" alt="AeroClicker, light theme">
  <img src="assets/screenshot-dark.png" width="49%" alt="AeroClicker, dark theme">
</p>

---

AeroClicker is a standalone Linux auto-clicker with deep click humanization: Gaussian and
uniform CPS distributions, random spikes and stutters, variable hold times, rhythm drift,
double-clicks and fatigue breaks. It runs natively on Wayland (and X11), works with any
application, and never touches the applications themselves.

Clicks are injected through `/dev/uinput`, and keyboard and mouse input is read through evdev.
The interface follows your KDE colour scheme (light or dark) and uses its own Oxygen-style
widget set: gradients, 1 px borders, hover states and themed analytics graphs.

AeroClicker is a fork of [Aubergine-UX's Ghast Clicker](https://github.com/Aubergine-UX/Ghast-Clicker),
which in turn ports the humanization engine from [IcySnex/GhostTap](https://github.com/IcySnex/GhostTap).
See [Credits](#credits).

---

## Contents

[Features](#features) · [Requirements](#requirements) · [Install](#install) ·
[Usage](#usage) · [Interface](#interface) · [Files](#files) ·
[Troubleshooting](#troubleshooting) · [Development](#development) · [Credits](#credits)

---

## Features

**Clicking**
- **Two independent clickers** — separate left/right CPS, humanization, activation mode and keybind
- **Humanization** — Gaussian and uniform timing; spike and stutter events; heavy holds; grip drift that wanders the hold time over a session; rhythm volatility and tension; MOUSE-mode start delay; fatigue breaks
- **Presets** — Normal, Jitter (the default), Butterfly and Drag Click, ready to tweak
- **Click styles** — optional double-clicks with a randomized delay; they add a click inside the current beat, so they never lower the average CPS

**Control**
- **Activation modes** — Toggle, Hold, and Mouse (arm with a key, then hold your real mouse button)
- **Slot activation** — the Minecraft hotbar slot switches each clicker on and off, with a per-clicker set of slots (default: slot 1 left, slot 9 right); it reads the game through the [bridge mod](#minecraft-slot-and-inventory-state)
- **Menu pause** — clicking stops while the game has any screen open (inventory, chat, chests, NPC menus) and resumes when it closes
- **Placeable-item gate** — the right clicker can require a block in hand, so right-click actions never fire on a sword, a pearl or an empty hand; it uses the same [bridge mod](#placeable-item-gate)
- **Macros** — optional, off by default: sequences of left clicks, right clicks, delays and hotbar slot switches, with looping and a keybind
- **Headless mode** — run the clicker with no window

**Interface**
- **Paused readout** — a switched-on clicker that is not clicking shows `Paused` and the reason (menu open, held item not a block, cursor over the app) with an amber dot, and its **L/R** pill turns amber, instead of a misleading green `ON`
- **Live analytics** — real-time CPS graphs, session averages and peaks, optional recording and CSV export
- **Profiles** — save and load complete configurations; the `config` profile is loaded automatically at startup
- **Theming** — follows the KDE colour scheme and accent; accents can also be fixed to Blue, Red, Green, Purple or Orange, or a custom colour
- **Sound feedback** — a short two-note chime when a clicker toggles, up or down (via `paplay`)

### Humanization defaults

The Jitter preset (the default) is tuned to look human without giving up rate. Double-clicks
are rare — 1 % of beats — and always land inside the current one, so they add clicks instead of
lengthening the gap. Fatigue breaks are short and infrequent: 0.15–0.4 s of rest every
40–90 s, and the break clock restarts whenever clicking is paused, so resuming never begins
with a break. Hold times follow the rate and drift slowly around their mean, as a real hand's
grip does, rather than being independent random draws. Every value is editable on the
**Left**/**Right** tabs, and **Normal**, **Butterfly** and **Drag Click** raise the double-click
chance as the click rate rises.

---

## Requirements

- Linux with access to `/dev/uinput` (write) and `/dev/input/*` (read) — see [Install](#install)
- C++17 compiler, CMake 3.16+
- GLFW 3 — 3.4 or newer for the native titlebar icon — and OpenGL 3
- Optional: `paplay` (PipeWire/PulseAudio) for toggle sounds

Dear ImGui 1.93.0 is vendored in `imgui-1.93.0/`, so no extra downloads are needed. GLFW older
than 3.4 still builds, but then the window gets no Wayland app id and your desktop will show a
generic titlebar icon instead of AeroClicker's.

---

## Install

### Dependencies

```bash
# Arch
sudo pacman -S --needed base-devel cmake glfw

# Debian / Ubuntu
sudo apt install build-essential cmake libglfw3-dev libgl1-mesa-dev

# Fedora
sudo dnf install gcc-c++ cmake glfw-devel mesa-libGL-devel

# Void Linux
sudo xbps-install -S base-devel cmake glfw-devel MesaLib-devel
# optional, for toggle sounds: sudo xbps-install -S pulseaudio-utils   (paplay)
```

### Build and install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$HOME/.local"   # per user, or without --prefix for /usr/local
```

The binary is also left at `build/aeroclicker` if you would rather not install it.

`cmake --install` writes three things under the prefix and touches nothing else:

| Path under the prefix | What it is |
| --- | --- |
| `bin/aeroclicker` | the binary |
| `share/applications/aeroclicker.desktop` | menu entry; `Exec=` is rewritten to the absolute path of the installed binary, and `Icon=aeroclicker` names the icon files |
| `share/icons/hicolor/{16,24,32,48,64,128,256,512}/apps/aeroclicker.png` | the app icon, one file per theme size |

Your settings are not part of the install: they live in `~/.config/aeroclicker/` and are created
on the first save. The udev rule below is not installed either — copy it yourself if you need it.
CMake records what it wrote in `build/install_manifest.txt`, so
`xargs rm -v < build/install_manifest.txt` removes an install again.

The desktop entry is written with the full path of the installed binary (`Exec=$HOME/.local/bin/aeroclicker`
for the command above, taken from the prefix actually used at install time), because a session
launcher does not have `~/.local/bin` on its `PATH`. Reinstall after changing the prefix.

### Permissions

AeroClicker needs write access to `/dev/uinput` and read access to your input devices. The
`input` group is the supported way to get both:

```bash
sudo usermod -aG input "$USER"
```

Log out and back in (a new terminal is not enough; the group list is read at login), then run
`aeroclicker` normally. If access is still missing, startup stops with `Cannot open /dev/uinput`
and the command to fix it, instead of crashing with an uncaught exception.

**Do not run it as root.** `sudo` is not needed for anything the app does, and it gives a
program that synthesizes mouse and keyboard input full system privileges, a root-owned window
in your session, and root's environment instead of yours. The app still tolerates `sudo` (it
keeps your real config directory and hands files back to your user), but there is no reason to
use it. If `sudo` is the only thing that works, fix the group membership instead.

### Fix virtual-mouse sensitivity

Some desktops apply a different pointer acceleration curve to the virtual mouse. The included
udev rule gives it the same flat profile as a normal mouse:

```bash
sudo cp 99-aeroclicker.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Reconnect the device (or restart AeroClicker) for the rule to apply.

### App icon

A Wayland window has no icon of its own: the compositor resolves it from the window's app id,
matched against an installed desktop entry. AeroClicker sets both to `aeroclicker` (the
`GLFW_WAYLAND_APP_ID` hint in `src/gui.cpp`, and `Icon=`/`StartupWMClass=` in the desktop
entry), so **the desktop entry and the icons must be installed** for the titlebar to show
anything but the compositor's placeholder. Running straight from `build/aeroclicker` is fine,
as long as they are installed. KDE picks them up immediately; if it does not, run
`kbuildsycoca6` or log out and back in.

---

## Usage

```bash
aeroclicker [options]
```

| Option | Description |
| --- | --- |
| `--lcps <n>` | Left CPS mean (default `13.0`) |
| `--rcps <n>` | Right CPS mean (default `15.0`) |
| `--lkey <code>` | Left hotkey as an evdev code (default `F6`) |
| `--rkey <code>` | Right hotkey (default `F7`) |
| `--toggle` | Toggle activation for both clickers |
| `--hold` | Hold activation for both clickers |
| `--mouse` | Mouse activation for both clickers (default) |
| `--headless` | Run without the GUI |
| `--save` | Save the configuration and exit |
| `-h`, `--help` | Show help |

Options apply on top of the saved `config` profile, for that run only. Key codes are evdev codes
(see `/usr/include/linux/input-event-codes.h`), not X11 keysyms; the GUI can also record keys
and mouse buttons interactively.

### Activation modes

| Mode | Behaviour |
| --- | --- |
| **Toggle** | Press the key to start, press again to stop |
| **Hold** | Clicks while the key is held |
| **Mouse** | Press the key to arm, then hold your real mouse button to auto-click; the real press is masked instead of reaching the application |

In Mouse mode AeroClicker takes exclusive access to the physical mouse so it can forward each
event exactly once — that is how the real press is masked. Other modes only read input
passively, and a hotkey bound to a mouse button works in every mode.

Input devices are picked up when they appear: unplugging and replugging the keyboard or the
mouse does not need a restart. If a mouse goes away while it was driving a clicker, that
clicker stops, since the button that was holding it can no longer be released.

### What “Paused” means

A clicker can be switched on and still be held back. Its card then shows `Paused`, the reason,
and an amber dot instead of a running CPS figure:

| Reason | When |
| --- | --- |
| `cursor over app` | the pointer is over AeroClicker's own window — the clicker stops rather than clicking the interface, and resumes when the pointer leaves |
| `menu is open` | the game reports an open screen — the inventory, chat, a chest, or an NPC menu |
| `held item is not a block` | the right clicker's placeable-item gate is on and the held item cannot be placed (see [Placeable-item gate](#placeable-item-gate)) |

The on/off state is kept, so closing the menu resumes clicking on its own. To force it, press the
clicker's hotkey (or its **L/R** pill): an explicit press un-pauses a paused clicker instead of
switching it off and lifts the menu pause until a menu opens again. The next hotbar slot change
re-applies the slot rules.

### Minecraft slot and inventory state

AeroClicker only sees keyboard and mouse events, which is not enough in Minecraft: the active
hotbar slot moves with the wheel, pick-block or a rebound key, an open inventory, chat, chest or
NPC menu looks exactly like a normal world, and the held item is invisible from outside. The
companion mod in `tools/mc-bridge` publishes those three facts, and the app uses them for slot
activation, the menu pause and the right clicker's placeable-item gate. Nothing falls back to key
guessing: without the mod, slot activation and the menu pause simply do nothing, and the gate
stays inert.

The mod is read-only: every 50 ms it writes the selected slot, whether any screen is open and
whether the held item is a block to `$XDG_RUNTIME_DIR/aeroclicker-mc.state` as a single line:

```
slot=<0-8, or -1 outside a world> screen=<0|1> ms=<epoch ms> block=<0|1|-1>
```

That is all it does. Nothing is sent to the server and no input is changed, so it is invisible to
anti-cheat in the same way as a HUD or scoreboard mod. When the file stops updating (the game
closed or crashed), the app treats the game as gone after 2.5 seconds: slot activation stops and
any clicker it switched on is switched off.

#### Installing the mod (optional)

Skip this if you do not want slot activation, the menu pause or the placeable-item gate. Nothing
is installed automatically: AeroClicker does not copy the jar anywhere, and `cmake --install` does
not touch your Minecraft folders. You choose the launcher and the instance, and put the jar in
that instance's `mods` folder yourself, like any other Fabric mod:

| Launcher | Where to find the instance |
| --- | --- |
| Official launcher | `~/.minecraft`, or the game directory of the installation you use |
| Prism Launcher / MultiMC | open the instance's folder from the launcher; the game directory and `mods` are inside it |
| ATLauncher | the instance's own folder |
| anything else | that launcher's instance folder |

The file is `tools/mc-bridge/aeroclicker-bridge-1.0.0.jar`; create the `mods` folder if it does
not exist yet. It covers Minecraft 26.1.2, 26.2 and 26.3 and needs Fabric Loader and Java 21 —
Fabric API is not required. Forge and NeoForge instances cannot load it. Instances without the
mod are unaffected: the app only reads the state file the mod writes.

Settings → Slot activation shows `Minecraft link: live` while the game is publishing state, and
highlights the slot the game currently has selected.

A prebuilt jar ships in the repository, so installing needs no toolchain. After changing the
mod's source, rebuild it with `./tools/mc-bridge/build.sh` (a JDK 21+ with `javac` and `jar`).
The script finds the Fabric loader jar in the usual Prism location; elsewhere, pass it with
`FABRIC_LOADER=/path/to/fabric-loader-*.jar`.

With the link live, slot activation applies as soon as the slot changes: picking a slot listed
under a clicker switches that clicker on, any other slot switches it off, and opening a menu
leaves it on but paused. A key or pill press still works normally and holds until the next slot
change. Settings → Slot activation is where the per-clicker slot lists live; the defaults are
slot 1 for the left clicker and slot 9 for the right one.

#### Placeable-item gate

The Right tab has **Only With a Placeable Item**. With it on, the right clicker only clicks while
the held item is a block that can be placed, and pauses with the reason `held item is not a
block` for anything else — a sword, a bucket, food, an empty hand. It is the right-click
equivalent of only bridging when you actually hold blocks, so a right click never throws a pearl,
starts drinking a potion or wears down a tool by accident.

The gate is driven by the same mod, so it needs a live link and a mod build that reports the held
item. Without either it stays inert instead of blocking every right click: an app build cannot
guess what is in your hand, and a silent no-op is safer than a clicker that never fires. The
state line's `block` field is `1` for a block, `0` for anything else, and `-1` for "this build
cannot tell".

A physical right press is only taken over when the clicker would actually act on it. While the
gate is blocking, the press and its release go to the game untouched, so you can still use a
bow, place water or open a door by hand without switching the gate off.

---

## Interface

| Tab | What it does |
| --- | --- |
| **Home** | Both clickers at a glance, with live CPS, click counts and on/off pills, plus the slot activation readout |
| **Settings** | Slot activation, menu pause, audio, profiles, accent |
| **Left / Right** | Presets, speed, distribution, humanization, hold timing, spikes, breaks, hotkey and activation mode; the Right tab adds the placeable-item gate |
| **Macros** | Build macro sequences and bind them to a key |
| **Analytics** | Measured CPS graphs (real press timing, not the target), session statistics, recording and CSV export |

---

## Files

Everything lives in:

```
~/.config/aeroclicker/
├── config                 # last used profile, loaded at startup
├── <profile-name>         # additional saved profiles
├── analytics_left_*.csv   # exports from the Analytics tab
└── analytics_right_*.csv
```

On first start, if `~/.config/aeroclicker/` does not exist but the old
`~/.config/ghast-clicker/` does, AeroClicker copies the old directory over so your profiles and
keybinds survive the rename.

---

## Troubleshooting

**Clicks do not happen**
- Make sure you can open `/dev/uinput` (add yourself to the `input` group, then re-login).
- Check that `/dev/input/event*` is readable — `groups` should list `input`; if it does not, the
  group change has not taken effect in this session yet.
- In Mouse mode the clicker only clicks while your real button is held after arming; with the
  pointer over AeroClicker it says `Paused` / `cursor over app` on purpose.
- A switched-on clicker showing `Paused` is being held back — see
  [What “Paused” means](#what-paused-means).

**Clicking the launcher says it cannot find aeroclicker**
- The entry predates the absolute `Exec=` path and names the binary by bare name, which a
  launcher cannot resolve for a per-user install. Run `cmake --install build --prefix "$HOME/.local"`
  again.

**The titlebar shows a generic icon**
- Install the desktop entry and icons (`cmake --install`), and make sure your GLFW is 3.4 or
  newer. Then restart AeroClicker.

**The virtual mouse moves differently from my real mouse**
- Install `99-aeroclicker.rules` as shown in [Install](#install).

**Slot activation does nothing in Minecraft**
- It needs the bridge mod: without it the app cannot know the selected slot or whether a menu is
  open, and it does not guess. Install it as described in
  [Minecraft slot and inventory state](#minecraft-slot-and-inventory-state).

**The right clicker stops with `held item is not a block`**
- That is the placeable-item gate doing its job: the held item cannot be placed. Hold a block, or
  switch **Only With a Placeable Item** off on the Right tab. The gate also reports this reason if
  the bridge mod is running an older build that does not export the held item; update the jar and
  restart Minecraft, since Fabric only loads mods at startup.

**No toggle sounds**
- Install `paplay` (PipeWire/PulseAudio). Sound is optional; AeroClicker silently skips it when
  `paplay` is unavailable.

**Wrong colours**
- AeroClicker reads the colour scheme from `~/.config/kdeglobals` and falls back to its own
  palette. Change the system scheme, or pick an accent in Settings.

**Blurry or oversized on a HiDPI screen**
- The UI follows the compositor's scale factor and rebuilds its font atlas when it changes.
  If it still looks off after changing scale or moving between monitors, restart AeroClicker.

---

## Development

A normal build and install need only a compiler, CMake, GLFW and OpenGL. Nothing in `tools/` is
needed to build the app, and all of its output is already in the tree:

| Path | What it does | Needed to build? |
| --- | --- | --- |
| `tools/mc-bridge/` | Fabric mod source, `build.sh` and the prebuilt jar. Needed only to rebuild the jar; users copy the jar themselves into the instance they choose. | No |

So neither Python nor the icon themes are needed for a normal build or install: `cmake --install`
copies the PNGs already in `assets/`, and the 16 px symbolic masks are committed in
`src/gui_icons_symbolic.inc`. The generated app icons and masks are checked in, so the generators
that produced them are not part of the repository — the PNGs are the source of truth now. Keep
`tools/mc-bridge/` if you want to offer the Minecraft mod.

---

## Credits

AeroClicker would not exist without these projects and people:

- **[Aubergine-UX](https://github.com/Aubergine-UX)** — created the original
  [Ghast Clicker](https://github.com/Aubergine-UX/Ghast-Clicker) Linux/Wayland port that this
  project is forked from. AeroClicker keeps that project's architecture, clicker engine and
  evdev/uinput input layer as its foundation; the interface here was rebuilt as the Oxygen-style
  KDE widget set described above.
- **[IcySnex](https://github.com/IcySnex)** — [GhostTap](https://github.com/IcySnex/GhostTap),
  the humanization engine and variance model the click timing is based on.
- **[b1scoito](https://github.com/b1scoito)** / Rafael Félix — [b1scoito/clicker](https://github.com/b1scoito/clicker),
  the original Windows clicker whose code and ideas the project descends from.
- **[ocornut](https://github.com/ocornut)** and contributors — [Dear ImGui](https://github.com/ocornut/imgui),
  bundled here as version 1.93.0.
- **[GLFW](https://www.glfw.org/)** — windowing and input for the GUI.

Changes in this fork include the rename to AeroClicker, the Dear ImGui 1.93.0 upgrade, a rebuilt
Oxygen-style interface (light and dark, live theming from KDE, new Home/Settings/Macros/Analytics
tabs), the pause/hotkey behaviour above, and assorted fixes to the clicker and the UI. Thanks to
everyone whose commits appear in the history.

---

## License

AeroClicker is licensed under the **GNU General Public License v3.0** — see
[LICENSE](LICENSE). As a fork of GPL-3.0 software, it stays under the same license.
