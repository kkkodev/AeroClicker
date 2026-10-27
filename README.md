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

AeroClicker is a standalone Linux auto-clicker with deep click humanization: Gaussian, Poisson
and uniform CPS distributions, random spikes and stutters, variable hold times, rhythm drift,
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
[Troubleshooting](#troubleshooting) · [Credits](#credits)

---

## Features

**Clicking**
- **Two independent clickers** — separate left/right CPS, humanization, activation mode and keybind
- **Humanization** — Gaussian, Poisson and uniform timing; spike and stutter events; heavy holds; rhythm volatility and tension; start delay; fatigue breaks
- **Presets** — Normal, Jitter, Butterfly and Drag Click, ready to tweak
- **Click styles** — optional double-clicks with a randomized delay

**Control**
- **Activation modes** — Toggle, Hold, and Mouse (arm with a key, then hold your real mouse button)
- **Inventory pause** — a key that suspends clicking while your inventory is open; it pauses instead of switching the clickers off, so they resume when it closes
- **Hotbar slot filter** — only click while holding one of the selected slots; selecting a filtered slot pauses the clickers and the card says why
- **Macros** — optional, off by default: sequences of left clicks, right clicks, delays and hotbar slot switches, with looping and a keybind
- **Headless mode** — run the clicker with no window

**Interface**
- **Paused readout** — a switched-on clicker that is not clicking shows `Paused` and the reason (inventory open, slot not allowed, cursor over the app) with an amber dot, instead of a misleading `ON`
- **Live analytics** — real-time CPS graphs, session averages and peaks, optional recording and CSV export
- **Profiles** — save and load complete configurations; the `config` profile is loaded automatically at startup
- **Theming** — follows the KDE colour scheme and accent; accents can also be fixed to Blue, Red, Green, Purple or Orange, or a custom colour
- **Sound feedback** — a short two-note chime when a clicker toggles, up or down (via `paplay`)

---

## Requirements

- Linux with access to `/dev/uinput` (write) and `/dev/input/*` (read) — see [Install](#install)
- C++17 compiler, CMake 3.16+
- GLFW 3 — 3.4 or newer for the native titlebar icon — and OpenGL 3
- Optional: `paplay` (PipeWire/PulseAudio) for toggle sounds
- Optional: Python 3 with Pillow, only to regenerate the app icon

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
```

### Build and install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$HOME/.local"   # per user, or without --prefix for /usr/local
```

The binary is also left at `build/aeroclicker` if you would rather not install it. Installing
adds the binary to `bin`, `aeroclicker.desktop` to `share/applications`, and the app icon to
`share/icons/hicolor/<size>/apps`.

The desktop entry is written with the full path of the installed binary (`Exec=$HOME/.local/bin/aeroclicker`
for the command above, taken from the prefix actually used at install time), because a session
launcher does not have `~/.local/bin` on its `PATH`. Reinstall after changing the prefix.

### Permissions

AeroClicker needs write access to `/dev/uinput` and read access to your input devices. Pick
**one** of these options.

**Option 1 — `input` group (recommended)**

```bash
sudo usermod -aG input "$USER"
```

Log out and back in, then run `aeroclicker`.

**Option 2 — run as root**

```bash
sudo -E ./build/aeroclicker
```

When started as root, AeroClicker redirects `HOME` to `/tmp` so GUI toolkits cannot trip over
your icon theme. The configuration does not follow it: it still lives in your real
`~/.config/aeroclicker`, and each save is handed back to your user account, so profiles are
shared with a normal run. Prefer the `input` group anyway.

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

The icon files in `assets/icons/app/` are committed, so the build needs nothing extra. After
changing `assets/aeroclicker.png`, regenerate them (this trims the source's transparent padding
so the artwork fills each size):

```bash
python3 tools/gen_app_icon.py                   # needs Pillow
cmake --install build --prefix "$HOME/.local"   # refresh the installed copies
```

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
| `--invkey <code>` | Inventory key (default `E`) |
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
| `inventory is open` | after the inventory key (default `E`) |
| `slot not allowed` | the slot filter is on and the active hotbar slot (keys `1`–`9`) is not one of the allowed ones |

The on/off state is kept, so closing the inventory or switching back to an allowed slot resumes
clicking on its own. To force it, press the clicker's hotkey (or its **L/R** pill): an explicit
press un-pauses a paused clicker instead of switching it off, clears the inventory state, and
overrides the slot filter until the next slot change.

---

## Interface

| Tab | What it does |
| --- | --- |
| **Home** | Both clickers at a glance, with live CPS, click counts and on/off pills |
| **Left / Right** | Presets, speed, distribution, humanization, hold timing, spikes, breaks, hotkey and activation mode |
| **Macros** | Build macro sequences and bind them to a key |
| **Settings** | Inventory key, hotbar slot filter, audio, profiles, accent |
| **Analytics** | CPS graphs, session statistics, recording and CSV export |

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
- Check that `/dev/input/event*` is readable; running with `sudo` is a quick way to confirm it
  is a permission problem.
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

## Credits

AeroClicker would not exist without these projects and people:

- **[Aubergine-UX](https://github.com/Aubergine-UX)** — created the original
  [Ghast Clicker](https://github.com/Aubergine-UX/Ghast-Clicker) Linux/Wayland port that this
  project is forked from. This fork keeps their architecture, clicker engine, evdev/uinput code
  and interface design as its foundation.
- **[IcySnex](https://github.com/IcySnex)** — [GhostTap](https://github.com/IcySnex/GhostTap),
  the humanization engine and variance model the click timing is based on.
- **[b1scoito](https://github.com/b1scoito)** / Rafael Félix — [b1scoito/clicker](https://github.com/b1scoito/clicker),
  the original Windows clicker whose code and ideas the project descends from.
- **[ocornut](https://github.com/ocornut)** and contributors — [Dear ImGui](https://github.com/ocornut/imgui),
  bundled here as version 1.93.0.
- **[GLFW](https://www.glfw.org/)** — windowing and input for the GUI.

Changes in this fork include the rename to AeroClicker, the Dear ImGui 1.93.0 upgrade, the
Oxygen-style widget set (light and dark), live theming from KDE, the macros and analytics tabs,
the pause/hotkey behaviour above, and assorted fixes to the clicker and the UI. Thanks to
everyone whose commits appear in the history.

---

## License

AeroClicker is licensed under the **GNU General Public License v3.0** — see
[LICENSE](LICENSE). As a fork of GPL-3.0 software, it stays under the same license.
