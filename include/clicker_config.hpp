#pragma once
#include <linux/input-event-codes.h>

enum class ActivationMode { TOGGLE, HOLD, MOUSE };
// Poisson was dropped (it sampled a click count per second rather than inter-arrival times,
// which quantized intervals and stalled for seconds at low rates). UNIFORM keeps the value 2
// it had when POISSON occupied 1, so existing config files load unchanged; 1 is retired.
enum class DistributionType { GAUSSIAN = 0, UNIFORM = 2 };
enum class MacroAction { LEFT_CLICK, RIGHT_CLICK, DELAY, SWITCH_SLOT };
enum class SprintResetMode { W_TAP, S_TAP, SHIFT_TAP };

// The defaults are the "Jitter" preset below, so a fresh config starts on it for both
// clickers instead of on a separate hand-tuned profile.
struct ClickerProfile {
    double cps_mean = 14.0;
    double cps_stddev = 1.8;
    double cps_min = 10.0;
    double cps_max = 18.0;
    double cps_minmax_fallout = 1.0;

    double spike_chance = 0.06;
    double spike_min = 1.5;
    double spike_max = 4.0;

    double stutter_chance = 0.05;
    double stutter_min = 3.0;
    double stutter_max = 6.0;

    // A mouse button is usually held longer than the bare minimum, and a hand's grip is less
    // consistent than a fixed ±5 ms; 29 ms / σ 6.7 read as too crisp at 14 CPS.
    double hold_ms_mean = 40.0;
    double hold_ms_stddev = 8.0;
    double hold_ms_min = 20.0;
    double hold_ms_max = 65.0;

    double hold_ms_heavy_chance = 0.02;
    double hold_ms_heavy_min = 12.0;
    double hold_ms_heavy_max = 30.0;

    // Slow drift on top of the per-click draw: the rate wanders over a session instead of
    // hovering tightly around one mean, which is the strongest timing tell of a simple script.
    double rhythm_volatility = 1.0;
    double rhythm_tension = 0.05;

    double start_delay_ms = 0.0;

    // A doublet adds a click instead of stealing rate (the next press is re-anchored from the
    // second click), so a low chance raises the effective CPS slightly rather than weakening it.
    double double_click_chance = 0.01;
    double double_click_delay_min = 25.0;
    double double_click_delay_max = 70.0;

    // Long enough to read as a hand reset (reposition, shake out the wrist), short enough not
    // to look like the clicker turned itself off. A full second was too long for that.
    bool breaks_enabled = true;
    double break_interval_min = 30.0;
    double break_interval_max = 75.0;
    double break_duration_min = 0.25;
    double break_duration_max = 0.6;

    int mouse_button = BTN_LEFT;
    DistributionType distribution = DistributionType::GAUSSIAN;

    // Slow wander of the hold time around its mean: grip and tension drift over a session, so
    // consecutive holds are correlated instead of being independent Gaussian draws. Kept last
    // so the preset initialisers above do not need two more columns.
    double hold_drift_ms = 2.0;
    double hold_drift_tension = 0.05;
};

constexpr double CPS_MEAN_LO = 1.0;
constexpr double CPS_MEAN_HI = 40.0;
constexpr double CPS_MIN_HI  = 40.0;
constexpr double CPS_MAX_HI  = 50.0;

struct ClickerPreset {
    const char* name;
    ClickerProfile profile;
};

inline const ClickerPreset LEFT_PRESETS[] = {
    {"Normal (8-10 CPS)", {
        9.0, 0.8, 7.0, 11.0, 0.5,
        0.02, 0.5, 1.5,
        0.02, 2.0, 4.0,
        45.0, 9.0, 22.0, 88.0,
        0.01, 10.0, 25.0,
        0.6, 0.03, 0.0, 0.01, 25.0, 70.0, true, 40.0, 95.0, 0.25, 0.6, BTN_LEFT
    }},
    {"Jitter (12-16 CPS)", {
        14.0, 1.8, 10.0, 18.0, 1.0,
        0.06, 1.5, 4.0,
        0.05, 3.0, 6.0,
        40.0, 8.0, 20.0, 65.0,
        0.02, 12.0, 30.0,
        1.0, 0.05, 0.0, 0.01, 25.0, 70.0, true, 30.0, 75.0, 0.25, 0.6, BTN_LEFT
    }},
    {"Butterfly (16-22 CPS)", {
        19.0, 2.2, 14.0, 24.0, 1.2,
        0.08, 2.0, 5.0,
        0.04, 5.0, 8.0,
        26.0, 6.0, 12.0, 50.0,
        0.02, 8.0, 20.0,
        1.2, 0.06, 0.0, 0.03, 20.0, 55.0, true, 30.0, 70.0, 0.25, 0.6, BTN_LEFT
    }},
    {"Drag Click (25+ CPS)", {
        28.0, 3.0, 22.0, 35.0, 1.5,
        0.10, 3.0, 6.0,
        0.03, 4.0, 7.0,
        17.0, 4.0, 9.0, 33.0,
        0.01, 5.0, 15.0,
        1.4, 0.08, 0.0, 0.05, 16.0, 45.0, true, 25.0, 60.0, 0.2, 0.5, BTN_LEFT
    }},
};

inline const ClickerPreset RIGHT_PRESETS[] = {
    {"Normal (8-10 CPS)", {
        9.0, 0.8, 7.0, 11.0, 0.5,
        0.02, 0.5, 1.5,
        0.02, 2.0, 4.0,
        45.0, 9.0, 22.0, 88.0,
        0.01, 10.0, 25.0,
        0.6, 0.03, 0.0, 0.01, 25.0, 70.0, true, 40.0, 95.0, 0.25, 0.6, BTN_RIGHT
    }},
    {"Jitter (12-16 CPS)", {
        14.0, 1.8, 10.0, 18.0, 1.0,
        0.06, 1.5, 4.0,
        0.05, 3.0, 6.0,
        40.0, 8.0, 20.0, 65.0,
        0.02, 12.0, 30.0,
        1.0, 0.05, 0.0, 0.01, 25.0, 70.0, true, 30.0, 75.0, 0.25, 0.6, BTN_RIGHT
    }},
    {"Butterfly (16-22 CPS)", {
        19.0, 2.2, 14.0, 24.0, 1.2,
        0.08, 2.0, 5.0,
        0.04, 5.0, 8.0,
        26.0, 6.0, 12.0, 50.0,
        0.02, 8.0, 20.0,
        1.2, 0.06, 0.0, 0.03, 20.0, 55.0, true, 30.0, 70.0, 0.25, 0.6, BTN_RIGHT
    }},
    {"Drag Click (25+ CPS)", {
        28.0, 3.0, 22.0, 35.0, 1.5,
        0.10, 3.0, 6.0,
        0.03, 4.0, 7.0,
        17.0, 4.0, 9.0, 33.0,
        0.01, 5.0, 15.0,
        1.4, 0.08, 0.0, 0.05, 16.0, 45.0, true, 25.0, 60.0, 0.2, 0.5, BTN_RIGHT
    }},
};

constexpr int PRESET_COUNT = sizeof(LEFT_PRESETS) / sizeof(LEFT_PRESETS[0]);

struct MacroStep {
    MacroAction action = MacroAction::LEFT_CLICK;
    int count = 1;
    double delay_ms = 100.0;
    int slot = 1;
};

constexpr int MAX_MACROS = 4;
constexpr int MAX_MACRO_STEPS = 16;

struct Macro {
    char name[32] = "";
    MacroStep steps[MAX_MACRO_STEPS] = {};
    int step_count = 0;
    bool loop = false;
    int key_bind = 0;
};

// Both combat modules are driven by the bridge mod, not by our own clicks: Jump Reset fires
// when the game reports that the local player took damage, Sprint Reset when it reports that
// the player attacked a living entity. That keeps them quiet outside real combat. The defaults
// are deliberately human-paced and randomized; a jump reset 15 ms after a hit looks nothing
// like a person reacting, while ~100 ms with a broad spread reads as a fast reaction.
struct JumpResetProfile {
    // 90 % keeps the reset reliable in a real fight while the occasional missed reaction is
    // what a person looks like. Sprint reset keeps its own default.
    double chance = 0.90;
    // A trained player reacts to a hit in roughly 90-130 ms, and strafing is as normal as
    // running straight. 100 ms with a right-skewed 25 ms spread (clamped to 60-170) is fast
    // enough to cut knockback without sitting on an impossible floor.
    double delay_ms_mean = 100.0;
    double delay_ms_stddev = 25.0;
    double delay_ms_min = 60.0;
    double delay_ms_max = 170.0;

    double hold_ms_mean = 70.0;
    double hold_ms_stddev = 8.0;
    double hold_ms_min = 50.0;
    double hold_ms_max = 95.0;

    double cooldown_ms = 500.0;
    // "Only while moving forward" is the strict W check; this one accepts any WASD direction,
    // which is what a player strafing or backing up actually does. Both may be on: forward then
    // wins because it is the narrower filter.
    bool forward_only = false;
    bool moving_only = true;
    bool allow_in_menu = false;
    bool use_slot_filter = true;
    // Pressing Space in water makes the player swim upward, which is both useless and a clear
    // tell, so Vape-style clients stop the reset while in water. On by default.
    bool disable_in_water = true;
    // A jump only does anything from the ground; pressing it mid-air just leaks the module.
    bool require_on_ground = true;
};

struct SprintResetProfile {
    SprintResetMode mode = SprintResetMode::W_TAP;
    double chance = 0.90;
    // A random delay keeps the tap from landing on the exact tick of every hit; 15-65 ms still
    // breaks sprint before the next swing without looking frame-perfect.
    double delay_ms_min = 15.0;
    double delay_ms_max = 65.0;

    double duration_ticks_mean = 1.25;
    double duration_ticks_stddev = 0.40;
    double duration_ticks_min = 1.0;
    double duration_ticks_max = 2.0;
    // Whole ticks alone make every W-Tap exactly 50 or 100 ms. A little per-tap millisecond
    // jitter keeps the release off the same sample boundary without shortening the tap below
    // the tick count the game needs to see.
    double tap_jitter_ms = 12.0;

    double cooldown_ms = 400.0;
    bool forward_only = true;
    bool use_slot_filter = true;
};

struct Config {
    ClickerProfile left;
    ClickerProfile right;

    ActivationMode left_mode = ActivationMode::MOUSE;
    ActivationMode right_mode = ActivationMode::MOUSE;

    int key_left = KEY_F6;
    int key_right = KEY_F7;
    int key_jump_reset = KEY_F9;
    int key_sprint_reset = KEY_F10;

    int accent_preset = 0;   // 0 = Follow KDE (Colors:Selection), 1 Blue, 2 Red, 3 Green, 4 Purple, 5 Orange, 6 Custom
    float accent_r = 0.24f;
    float accent_g = 0.49f;
    float accent_b = 0.78f;

    // Slot activation: while one of a clicker's slots is the selected hotbar slot, that clicker
    // is switched on; any other slot switches it off. The slot comes from the bridge mod, so
    // this only acts while the game is publishing state. The mapping defaults to slot 1 left and
    // slot 9 right, but following the hotbar is off until the user enables it.
    bool slot_auto_enabled = false;
    bool left_slots[9]  = {true,  false, false, false, false, false, false, false, false};
    bool right_slots[9] = {false, false, false, false, false, false, false, false, true};
    // Screens include the inventory, chat, chests and NPC menus: clicking them is never wanted.
    bool pause_in_menu = true;
    // Right clicker: click only while the held item is a block. On by default, so a right
    // clicker cannot throw a pearl or drink a potion by accident. The bridge mod reports the
    // held item, so this is inert without it (and with an older mod build).
    bool right_placeable_only = true;
    // Beds are placeable blocks, but a right clicker holding one is usually a mistake (bed
    // PvP, accidental placement). On by default so the right gate skips them.
    bool right_gate_no_bed = true;
    // Right clicker: only click while the crosshair is on a block. On by default, so it stays
    // quiet while aiming at air or an entity. This is the build/bridge filter; it is separate
    // from the held-item gate and needs a mod build that reports `look`.
    bool right_look_block = true;
    // Left clicker, GhostTap-style gate. Value 1 = a sword must be held, 2 = a target (mob,
    // player or fireball) must be in the crosshair, 3 = both. 0 is off. Inert without the mod.
    // Default: only click at targets, which is what keeps a clicker from firing at blocks.
    int left_gate = 2;
    // Whether a fireball in the crosshair counts as a target for the gate above. On by default:
    // a fireball flying at you is worth clicking at just like a player.
    bool left_gate_fireball = true;

    // Combat modules: off until the user turns them on. The profiles above hold their timing.
    bool jump_reset_enabled = false;
    bool sprint_reset_enabled = false;
    JumpResetProfile jump_reset;
    SprintResetProfile sprint_reset;

    bool audio_enabled = true;
    bool macros_enabled = false;
    Macro macros[MAX_MACROS] = {};

    Config() {
        left.mouse_button = BTN_LEFT;
        right.mouse_button = BTN_RIGHT;
    }
};
