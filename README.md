<p align="center" xmlns="http://www.w3.org/1999/html">
  <img src="assets/icon/icon-transparent.svg" width="128" alt="GhostTap icon">
</p>

<h1 align="center">
  GhostTap
</h1>
<h3 align="center">
  <a href="https://github.com/IcySnex/GhostTap/releases/latest">
    <img alt="GitHub Downloads (all assets, total)" src="https://img.shields.io/github/downloads/IcySnex/GhostTap/total?style=for-the-badge&label=Download%20&color=5a95ce">
  </a>
  <span> ˙ </span>
  <a href="https://www.curseforge.com/minecraft/mc-mods/ghosttap-clicker">
    <img alt="CurseForge Downloads" src="https://img.shields.io/curseforge/dt/1596026?style=for-the-badge&label=CurseForge&color=eb622b">
  </a>
  <span> ˙ </span>
  <a href="https://modrinth.com/mod/ghosttap-clicker">
    <img alt="Modrinth Downloads" src="https://img.shields.io/modrinth/dt/qzibnuGY?style=for-the-badge&label=Modrinth&color=1bd769">
  </a>
</h3>


<table>
  <tr>
    <td width="99999" align="center">
      <b>A lightweight internal auto‑clicker focused on customization and undetectability.</b>
      <br/>
      Independent clickers for Left/Right  - deep humanization - fully customizable with in-game UI - live HUD - click analysis
    </td>
  </tr>
</table>

<h3 align="center">
  <a href="https://minecraft.wiki/w/Java_Edition_1.8.9">
    <img alt="Static Badge" src="https://img.shields.io/badge/Minecraft-1.8.9-62B47A?style=for-the-badge">
  </a>
  <span> ˙ </span>
  <a href="https://files.minecraftforge.net/net/minecraftforge/forge/index_1.8.9.html">
    <img alt="Static Badge" src="https://img.shields.io/badge/Forge-11.15.1.2318-1E2D42?style=for-the-badge">
  </a>
</h3>

<p align="center">
  <img src="assets/hero.gif" width="720" alt="GhostTap in action">
</p>

---

## Features

- **Independent Left & Right Clickers:** Separate timing, filters, activation modes, and keybinds for each side.
- **3 Activation Modes:** Toggle, Hold, or Mouse mode (arms the clicker, but only clicks when you hold your physical mouse button).
- **Humanization:** Custom Gaussian CPS distributions, random spikes/stutters, variable hold times, and rhythmic pace drift.
- **Smart Filters (Gates):** Restrict clicking by hotbar slot, held item type, gamemode, in-game menus, entity reach, block-breaking (LMB), and block-placement (RMB).
- **Live HUD:** Customizable CPS counter and clicker status overlay.
- **Click Analytics:** Export precise timing data to CSV and plot the distributions using the included Python tool.
- **Config Sharing:** Import and export your settings instantly as base64 clipboard tokens.

---

## Why GhostTap?

Spam-clicking for hours in 1.8.9 PvP is exhausting, but most existing auto-clickers have major flaws:

* **External clickers** run as separate OS processes which means they eat up resources, lack cross-platform support, and (*most importantly*) can't read game state. That means they can't stop clicking in menus, pause while you break blocks, or whitelist specific hotbar slots. Plus, many are closed-source sketchware.
* **Internal clickers** are often poorly coded. They either fire at rigid, metronome-perfect intervals, lack basic featuers like a right-clicker, or rely on `java.awt.Robot` / direct network packets, which breaks other HUD mods like CPS displays and Keystrokes.

GhostTap is a fully open-source internal mod built to solve this with two goals:

1. **Look Human:** Every click interval and hold duration is pulled from tunable statistical models. With random stutters, heavy holds, and rhythmic drift, no two clicks look identical.
2. **A Low-footprint and controllable:** Input is spoofed at the **LWJGL layer**, not through fake OS events, so the game reads it through its normal mouse pipeline. Everything is managed through a clean in-game GUI - no config files required. 

---

## How It Works

GhostTap uses SpongePowered **Mixins** to hook into LWJGL's `org.lwjgl.input.Mouse` (`poll()` and `next()`), which the game uses to read hardware inputs every frame.

- When active, it injects synthetic press and release events directly into the mouse queue.
- It automatically masks your physical clicks so your manual clicking and the mod don't conflict.
- Because it avoids `java.awt.Robot` and network-level packet manipulation, all standard HUD mods (Keystrokes, CPS counters) still register the clicks properly.
- Clicks are handled on two dedicated worker threads that sleep when idle, keeping CPU overhead at zero. Filters are evaluated safely on the main thread once per client tick.

> [!WARNING]
> While GhostTap humanizes your click patterns perfectly to bypass server-side checks, it is still a coremod. Client-side anti-cheats that scan for loaded transformers or mixin modifications *will* see that the mod is present.

---

## The Configuration Menu

Press **Right Shift** in-game to open the menu. If you need to rebind this key, type `.ghosttap key <KEY>` in the game chat.

<p align="center">
  <img src="assets/screenshots/gui-general.png" alt="General tab">
</p>

### General Settings
Controls global toggles, activation styles, analytics, and an option to hide the mod from Forge server queries.

| Setting | Description |
| :--- | :--- |
| **Enabled** | Master switch for the clicker. |
| **Mode** | Choose between Toggle, Hold, or Mouse mode. |
| **Start Delay** | *(Mouse mode only)* Delay before clicking starts so quick taps pass through normally. |
| **Key** | Keybind for the clicker (supports keyboard and mouse side buttons). |
| **Config** | Reset, export, or import profiles via clipboard tokens. |

#### Activation Modes
* **Toggle:** Tap the key once to turn the clicker on; tap again to turn it off.
* **Hold:** Clicks only while the key is actively held down.
* **Mouse:** Press the key to **arm** the clicker. It will only start clicking when you hold down your **real** mouse button. Perfect for natural PvP or bridging.

> Settings automatically save to your local config folder at `.minecraft/config/ghosttap.cfg`.

---

### Left & Right Clicker Customization

Each clicker features three sub-tabs for deep tuning:

<p align="center">
  <img src="assets/screenshots/gui-left-cps.png" alt="Left/Right CPS tab">
</p>

* **CPS:** Tune the core speed using Mean, Std Deviation, Min/Max limits, and "fallout" (how far the rate can drift before resetting). You can also configure random **Spikes** (bursts of speed) and **Stutters** (sudden slowdowns).

<p align="center">
  <img src="assets/screenshots/gui-left-fatigue.png" alt="Left/Right Fatigue tab">
</p>

* **Fatigue:** Adjust individual click physics. Set standard click **Hold** times (in milliseconds), trigger random **Heavy Holds** (simulating a tired finger), and adjust **Rhythm** (how drastically the pace drifts over time).

<p align="center">
  <img src="assets/screenshots/gui-left-filters.png" alt="Left Filters tab">
  <img src="assets/screenshots/gui-right-filters.png" alt="Right Filters tab">
</p>

* **Filters:** Fine-tune when the clicker is allowed to fire:
  * **Hotbar slots:** Whitelist specific slots (1–9).
  * **Held item:** Filter by Weapons, Tools, Blocks, or Other items.
  * **Rules:** Break Blocks (pauses LMB while mining), Placeable Only (fires RMB only when looking at a valid block collision), In Menus, Pause on Item Use, and Entity Only (with custom reach limits).
  * **Game mode:** Whitelist Survival, Creative, or Adventure.

---

### HUD Options

<p align="center">
  <img src="assets/screenshots/hud.png" width="400" alt="HUD">
  <img src="assets/screenshots/gui-hud.png" alt="HUD tab">
</p>

* **Left / Right CPS:** Toggle text readouts for active clicker speeds.
* **Clicker Status:** Displays current activation state (`ON/OFF`) and firing state (`FIRE/IDLE`).
* **Visuals:** Fully customizable colors (Hex), background opacity, margins, and anchoring to snap the HUD to any screen corner.

---

### Analytics & Plotting

<p align="center">
  <img src="assets/screenshots/gui-analytics.png" alt="Analytics tab">
</p>

When enabled, GhostTap logs every single click (both real and generated) with precise timestamps, target vs. actual CPS, hold durations, and intervals. 

Exporting saves a `.csv` file directly to your desktop. You can run the Python script located in the `tools/` directory to plot your click distributions and verify how human your settings actually look.

<p align="center">
  <img src="assets/plots/combined.png" alt="Analytics plots">
</p>

---

## Fair Use & Detectability

GhostTap is an open-source personal research project. Keep the following in mind:
* **Input:** The generated timing patterns and LWJGL injection layer make the clicks nearly impossible to differentiate from human input via server-side checks. BUT:
* **Coremod:** Because it edits code at runtime, client-side anti-cheats checking for byte code manipulation or Mixin hooks will flag the presence of a mod, regardless of your settings - especially since it's a **Forge Mod** lol.
* **Efficiency :** Filters like *Placeable only* make you mechanically flawless (100% valid placements, zero wasted clicks). That is efficient, but a perfectly clean pattern at a low click-rate can look non-human to an observer or a CPS meter. Tune with that trade-off in mind.

*Use this tool responsibly and respect the rules of the servers you play on.*

---

## Installation

1. Install **Minecraft 1.8.9** with **Forge** (`11.15.1.2318` or compatible).
2. Drop `GhostTap-x.x.x.jar` into your `.minecraft/mods/` folder.
3. Launch, and press **Right Shift** in-game to open the menu.

---

## Building from source

Requires **JDK 8**. The toolchain (ForgeGradle 2.1) will not run on newer JDKs.

```sh
JAVA_HOME=<path-to-jdk8> ./gradlew build
```

The built jar lands in `build/libs/GhostTap-x.x.x.jar`.

---

## License

Licensed under the **GNU General Public License v3.0** - see [LICENSE](LICENSE).
