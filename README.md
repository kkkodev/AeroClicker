<h1 align="center">AeroClicker</h1>

<p align="center">
  A humanized auto-clicker for Linux — Wayland and X11 — with a polished Dear ImGui interface.
</p>

<p align="center">
  <img src="assets/screenshot-light.png" width="49%" alt="AeroClicker — oxygen light theme">
  <img src="assets/screenshot-dark.png" width="49%" alt="AeroClicker — oxygen dark theme">
</p>

---

## What is AeroClicker?

AeroClicker is a standalone Linux auto-clicker with deep click humanization: Gaussian,
Poisson and uniform CPS distributions, random spikes and stutters, variable hold times,
rhythm drift, double-clicks and fatigue breaks. It runs natively on Wayland (and X11),
works with any application, and never touches the applications themselves.

Clicks are injected through `/dev/uinput`; keyboard and mouse input is read through
evdev. The GUI follows your KDE colour scheme (light or dark) and has its own
oxygen-styled widgets: gradients, 1px borders, hover states and a themed analytics
tooltip.

AeroClicker is a fork of [Aubergine-UX's Ghast Clicker](https://github.com/Aubergine-UX/Ghast-Clicker),
which in turn ports the humanization engine from [IcySnex/GhostTap](https://github.com/IcySnex/GhostTap).
See [Credits](#credits).

---

## Features

- **Two independent clickers** — separate left/right CPS, humanization, activation mode and keybind
- **Humanization** — Gaussian, Poisson and uniform timing; spike/stutter events; heavy holds; rhythm volatility and tension; start delay; fatigue breaks
- **Presets** — Normal, Jitter, Butterfly and Drag Click, ready to tweak
- **Activation modes** — Toggle, Hold, and Mouse (arm with a key, then hold your real mouse button)
- **Click styles** — optional double-clicks with randomized delay
- **Inventory pause** — a key that suspends clicking while your inventory is open
- **Hotbar slot filter** — only click while holding one of the selected slots
- **Macros** — optional (off by default): sequences of left clicks, right clicks, delays and hotbar slot switches, with looping and a keybind
- **Live analytics** — real-time CPS graphs, session averages/peaks, optional recording and CSV export
- **Profiles** — save and load complete configurations; the `config` profile is loaded automatically at startup
- **Theming** — follows the KDE colour scheme; the accent follows KDE by default, with selectable presets or a custom accent
- **Sound feedback** — a short beep when a clicker toggles (via `paplay`)
- **Headless mode** — run the clicker with no window

---

## Requirements

- Linux with `/dev/uinput` and `/dev/input/*` access
- C++17 compiler, CMake 3.16+
- GLFW 3 and OpenGL 3
- Optional: `paplay` (PipeWire/PulseAudio) for toggle sounds

### Install dependencies

```bash
# Arch
sudo pacman -S --needed base-devel cmake glfw

# Debian / Ubuntu
sudo apt install build-essential cmake libglfw3-dev libgl1-mesa-dev

# Fedora
sudo dnf install gcc-c++ cmake glfw-devel mesa-libGL-devel
```

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The binary is `build/aeroclicker`. To install it system-wide:

```bash
sudo cmake --install build          # installs /usr/local/bin/aeroclicker
sudo cp aeroclicker.desktop /usr/share/applications/
```

Dear ImGui 1.93.0 is vendored in `imgui-1.93.0/`, so no extra downloads are needed.

---

## Permissions

AeroClicker needs write access to `/dev/uinput` and read access to your input devices.
Pick **one** of these options.

### Option 1 — `input` group (recommended)

```bash
sudo usermod -aG input "$USER"
```

Log out and back in, then run `./build/aeroclicker`.

### Option 2 — run as root

```bash
sudo -E ./build/aeroclicker
```

When started as root, AeroClicker redirects `HOME` to `/tmp` to avoid icon-theme
crashes from GUI toolkits. That means profiles and settings are not shared with your
normal user account — prefer the `input` group when you can.

### Fix virtual-mouse sensitivity

Some desktops apply a different pointer acceleration curve to the virtual mouse. The
included udev rule gives it the same flat profile as a normal mouse:

```bash
sudo cp 99-aeroclicker.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Reconnect the device (or restart AeroClicker) for the rule to apply.

---

## Usage

```bash
./build/aeroclicker [options]
```

| Option | Description |
| --- | --- |
| `--lcps <n>` | Left CPS mean (default `13.0`) |
| `--rcps <n>` | Right CPS mean (default `15.0`) |
| `--lkey <code>` | Left toggle key as an evdev code (default `F6`) |
| `--rkey <code>` | Right toggle key (default `F7`) |
| `--invkey <code>` | Inventory key (default `E`) |
| `--toggle` | Toggle activation |
| `--hold` | Hold activation |
| `--mouse` | Mouse activation (default) |
| `--headless` | Run without the GUI |
| `--save` | Save the current configuration and exit |
| `-h`, `--help` | Show help |

Key codes are evdev codes (see `/usr/include/linux/input-event-codes.h`), not X11
keysyms. You can also pick keys interactively from the GUI.

### Activation modes

| Mode | Behaviour |
| --- | --- |
| **Toggle** | Press the key to start, press again to stop |
| **Hold** | Clicks while the key is held |
| **Mouse** | Press the key to arm, then hold your real mouse button to auto-click; real clicks are masked |

---

## Interface

| Tab | What it does |
| --- | --- |
| **Home** | Both clickers at a glance, with live CPS, click counts and on/off pills |
| **Left / Right** | Presets, speed, distribution, humanization, hold timing, spikes, breaks, keybind and activation mode |
| **Macros** | Build macro sequences and bind them to a key |
| **Settings** | Inventory key, hotbar slot filter, audio, profiles, accent preset |
| **Analytics** | CPS graphs, session statistics, recording and CSV export |

---

## Profiles and files

Everything lives in:

```
~/.config/aeroclicker/
├── config                 # last used profile
├── <profile-name>         # additional saved profiles
└── analytics_*.csv        # exports from the Analytics tab
```

On first start, if `~/.config/aeroclicker/` does not exist but the old
`~/.config/ghast-clicker/` does, AeroClicker copies the old directory over so your
profiles and keybinds survive the rename.

---

## Troubleshooting

**Clicks do not happen**
- Make sure you can open `/dev/uinput` (add yourself to the `input` group, then re-login).
- Check that `/dev/input/event*` is readable; `sudo` is a quick way to confirm it is a
  permission problem.
- If you use `--mouse` mode, the clicker grabs your physical mouse while armed — that is
  expected.

**The window looks blurry or oversized on Wayland**
- The GUI uses GLFW; on compositors that only expose XWayland it runs through XWayland.
  Set `GDK_SCALE`/`QT_SCALE` consistently if you mix HiDPI scales.

**The virtual mouse moves differently from my real mouse**
- Install `99-aeroclicker.rules` as shown above.

**No toggle sounds**
- Install `paplay` (PipeWire/PulseAudio). Sound is optional; AeroClicker silently
  skips it when `paplay` is unavailable.

**Wrong colours**
- AeroClicker reads the KDE colour scheme from `~/.config/kdeglobals` and falls back to
  its own palette. Change the system colour scheme (or the accent preset in Settings)
  to update the UI.

---

## Credits

AeroClicker would not exist without these projects and people:

- **[Aubergine-UX](https://github.com/Aubergine-UX)** — created the original
  [Ghast Clicker](https://github.com/Aubergine-UX/Ghast-Clicker) Linux/Wayland port that
  this project is forked from. This fork keeps their architecture, clicker engine,
  evdev/uinput code and interface design as its foundation.
- **[IcySnex](https://github.com/IcySnex)** — [GhostTap](https://github.com/IcySnex/GhostTap),
  the humanization engine and variance model that the click timing is based on.
- **[b1scoito](https://github.com/b1scoito)** / Rafael Félix — [b1scoito/clicker](https://github.com/b1scoito/clicker),
  the original Windows clicker whose code and ideas the project descends from.
- **[ocornut](https://github.com/ocornut)** and contributors — [Dear ImGui](https://github.com/ocornut/imgui),
  bundled here as version 1.93.0.
- **[GLFW](https://www.glfw.org/)** — windowing and input for the GUI.

Changes in this fork include the rename to AeroClicker, the Dear ImGui 1.93.0 upgrade,
the oxygen-style widget set (light and dark), live theming from KDE, the macros and
analytics tabs, and assorted fixes to the clicker and UI. Thanks to everyone whose
commits appear in the history.

---

## License

AeroClicker is licensed under the **GNU General Public License v3.0** — see
[LICENSE](LICENSE). As a fork of GPL-3.0 software, it stays under the same license.
