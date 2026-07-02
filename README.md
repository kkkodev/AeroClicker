# GhostTap

A lightweight internal auto-clicker for **Minecraft 1.8.9 (Forge)**, focused on
customization and undetectability. Input is spoofed at the LWJGL layer through a
Mixin, so no fake key events are fired.

## Features

- **Two independent clickers** (left & right), each fully tunable.
- **Humanized timing** — gaussian CPS, random spikes & stutters, a hold-time
  model, and rhythm drift, so the pattern never looks robotic.
- **Three activation modes**
  - *Toggle* — press the key to switch on/off.
  - *Hold* — clicks only while the key is held.
  - *Mouse* — press the key to arm, then hold the real mouse button to click
    (with an optional start delay so quick taps stay single clicks).
- **Filters / gates** — hotbar-slot whitelist, held-item categories, break
  blocks, entity-only with a random reach window, game mode, pause while using
  an item, and allow-in-menus.
- **Configurable HUD** — CPS counter and per-clicker status (armed / firing),
  custom colours, background, and screen-corner anchors.
- **Analytics** — per-click tracking with CSV export (to your Desktop) and a
  python plotting tool under `tools/`.
- **Config sharing** — per-clicker and HUD settings export to base64 clipboard
  tokens; import to apply.
- **Config screen** opened with a keybind, and via the Forge mods "Config" button.

## Install

1. Minecraft **1.8.9** with **Forge** (`11.15.1.2318` or compatible).
2. Drop `GhostTap-1.0.jar` into your `mods/` folder.
3. Launch.

## Usage

- **Open the menu:** Right-Shift (rebindable in the General tab).
- If that key is taken, type `.ghosttap` in chat — it's intercepted before
  sending (never shown or sent to the server) and opens the menu.
  `.ghosttap key <name>` rebinds the menu key.
- **Left / Right tabs** — CPS, Fatigue (hold/heavy/rhythm), and Filters per clicker.
- **General tab** — enable, mode, key and config per clicker, plus HUD and
  Analytics toggles.

## Building

Requires **JDK 8** — the old ForgeGradle (2.1) won't run on newer JDKs.

```sh
JAVA_HOME=<path-to-jdk8> ./gradlew build
```

Output: `build/libs/GhostTap-1.0.jar`.

## License

GPLv3 — see [LICENSE](LICENSE).
