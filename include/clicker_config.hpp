#pragma once
#include <linux/input-event-codes.h>

enum class ActivationMode { TOGGLE, HOLD, MOUSE };
// Poisson was dropped (it sampled a click count per second rather than inter-arrival times,
// which quantized intervals and stalled for seconds at low rates). UNIFORM keeps the value 2
// it had when POISSON occupied 1, so existing config files load unchanged; 1 is retired.
enum class DistributionType { GAUSSIAN = 0, UNIFORM = 2 };
enum class MacroAction { LEFT_CLICK, RIGHT_CLICK, DELAY, SWITCH_SLOT };

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

    double hold_ms_mean = 32.0;
    double hold_ms_stddev = 5.0;
    double hold_ms_min = 15.0;
    double hold_ms_max = 60.0;

    double hold_ms_heavy_chance = 0.02;
    double hold_ms_heavy_min = 12.0;
    double hold_ms_heavy_max = 30.0;

    double rhythm_volatility = 0.7;
    double rhythm_tension = 0.05;

    double start_delay_ms = 0.0;

    // A doublet adds a click instead of stealing rate (the next press is re-anchored from the
    // second click), so a low chance raises the effective CPS slightly rather than weakening it.
    double double_click_chance = 0.01;
    double double_click_delay_min = 20.0;
    double double_click_delay_max = 55.0;

    // Rare and short by default: roughly half a second of rest every minute or so, not a
    // pause that reads as "the clicker stopped".
    bool breaks_enabled = true;
    double break_interval_min = 40.0;
    double break_interval_max = 90.0;
    double break_duration_min = 0.15;
    double break_duration_max = 0.4;

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
        45.0, 8.0, 25.0, 85.0,
        0.01, 10.0, 25.0,
        0.3, 0.03, 0.0, 0.01, 20.0, 55.0, true, 50.0, 120.0, 0.15, 0.4, BTN_LEFT
    }},
    {"Jitter (12-16 CPS)", {
        14.0, 1.8, 10.0, 18.0, 1.0,
        0.06, 1.5, 4.0,
        0.05, 3.0, 6.0,
        32.0, 5.0, 15.0, 60.0,
        0.02, 12.0, 30.0,
        0.7, 0.05, 0.0, 0.01, 20.0, 55.0, true, 40.0, 90.0, 0.15, 0.4, BTN_LEFT
    }},
    {"Butterfly (16-22 CPS)", {
        19.0, 2.2, 14.0, 24.0, 1.2,
        0.08, 2.0, 5.0,
        0.04, 5.0, 8.0,
        22.0, 4.0, 10.0, 45.0,
        0.02, 8.0, 20.0,
        1.0, 0.06, 0.0, 0.03, 15.0, 45.0, true, 30.0, 70.0, 0.15, 0.35, BTN_LEFT
    }},
    {"Drag Click (25+ CPS)", {
        28.0, 3.0, 22.0, 35.0, 1.5,
        0.10, 3.0, 6.0,
        0.03, 4.0, 7.0,
        15.0, 3.0, 8.0, 30.0,
        0.01, 5.0, 15.0,
        1.2, 0.08, 0.0, 0.05, 12.0, 35.0, true, 25.0, 60.0, 0.12, 0.3, BTN_LEFT
    }},
};

inline const ClickerPreset RIGHT_PRESETS[] = {
    {"Normal (8-10 CPS)", {
        9.0, 0.8, 7.0, 11.0, 0.5,
        0.02, 0.5, 1.5,
        0.02, 2.0, 4.0,
        45.0, 8.0, 25.0, 85.0,
        0.01, 10.0, 25.0,
        0.3, 0.03, 0.0, 0.01, 20.0, 55.0, true, 50.0, 120.0, 0.15, 0.4, BTN_RIGHT
    }},
    {"Jitter (12-16 CPS)", {
        14.0, 1.8, 10.0, 18.0, 1.0,
        0.06, 1.5, 4.0,
        0.05, 3.0, 6.0,
        32.0, 5.0, 15.0, 60.0,
        0.02, 12.0, 30.0,
        0.7, 0.05, 0.0, 0.01, 20.0, 55.0, true, 40.0, 90.0, 0.15, 0.4, BTN_RIGHT
    }},
    {"Butterfly (16-22 CPS)", {
        19.0, 2.2, 14.0, 24.0, 1.2,
        0.08, 2.0, 5.0,
        0.04, 5.0, 8.0,
        22.0, 4.0, 10.0, 45.0,
        0.02, 8.0, 20.0,
        1.0, 0.06, 0.0, 0.03, 15.0, 45.0, true, 30.0, 70.0, 0.15, 0.35, BTN_RIGHT
    }},
    {"Drag Click (25+ CPS)", {
        28.0, 3.0, 22.0, 35.0, 1.5,
        0.10, 3.0, 6.0,
        0.03, 4.0, 7.0,
        15.0, 3.0, 8.0, 30.0,
        0.01, 5.0, 15.0,
        1.2, 0.08, 0.0, 0.05, 12.0, 35.0, true, 25.0, 60.0, 0.12, 0.3, BTN_RIGHT
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

struct Config {
    ClickerProfile left;
    ClickerProfile right;

    ActivationMode left_mode = ActivationMode::MOUSE;
    ActivationMode right_mode = ActivationMode::MOUSE;

    int key_left = KEY_F6;
    int key_right = KEY_F7;

    int accent_preset = 0;   // 0 = Follow KDE (Colors:Selection), 1 Blue, 2 Red, 3 Green, 4 Purple, 5 Orange, 6 Custom
    float accent_r = 0.24f;
    float accent_g = 0.49f;
    float accent_b = 0.78f;

    // Slot activation: while one of a clicker's slots is the selected hotbar slot, that clicker
    // is switched on; any other slot switches it off. The slot comes from the bridge mod, so
    // this only acts while the game is publishing state. Defaults: slot 1 left, slot 9 right.
    bool slot_auto_enabled = true;
    bool left_slots[9]  = {true,  false, false, false, false, false, false, false, false};
    bool right_slots[9] = {false, false, false, false, false, false, false, false, true};
    // Screens include the inventory, chat, chests and NPC menus: clicking them is never wanted.
    bool pause_in_menu = true;
    // Right clicker only: click while the held item is a block. The bridge mod reports the
    // held item, so this is inert without it (and with an older mod build).
    bool right_placeable_only = false;

    bool audio_enabled = true;
    bool macros_enabled = false;
    Macro macros[MAX_MACROS] = {};

    Config() {
        left.mouse_button = BTN_LEFT;
        right.mouse_button = BTN_RIGHT;
    }
};
