package com.icysnex.ghosttap.core;

import org.lwjgl.input.Keyboard;

// Every default value in one place. Referenced by the field initialisers, the
// reset buttons and the config loader so there is a single source of truth.
public final class Defaults {

    private Defaults() {
    }

    // Clicker tuning (per button)
    public static final double CPS_MEAN = 12.0;
    public static final double CPS_STD_DEV = 1.5;
    public static final double CPS_MIN = 8.0;
    public static final double CPS_MAX = 18.0;
    public static final double CPS_FALLOUT = 0.8;

    public static final double SPIKE_CHANCE = 0.04;
    public static final double SPIKE_MIN = 1;
    public static final double SPIKE_MAX = 3;

    public static final double STUTTER_CHANCE = 0.03;
    public static final double STUTTER_MIN = 4;
    public static final double STUTTER_MAX = 7;

    public static final double HOLD_MEAN = 38;
    public static final double HOLD_STD_DEV = 6.5;
    public static final double HOLD_MIN = 18;
    public static final double HOLD_MAX = 75;

    public static final double HEAVY_CHANCE = 0.015;
    public static final double HEAVY_MIN = 15;
    public static final double HEAVY_MAX = 35;

    public static final double RHYTHM_VOLATILITY = 0.5;
    public static final double RHYTHM_TENSION = 0.04;

    // Gates
    public static final boolean GATE_ITEM = true;      // weapons / tools / blocks / other
    public static final boolean GATE_GAMEMODE = true;  // survival / creative / adventure
    public static final boolean GATE_SLOT = true;
    public static final boolean ALLOW_BLOCK_BREAK = false;
    public static final boolean ALLOW_IN_MENU = false;
    public static final boolean PAUSE_ON_ITEM_USE = true;

    // Keys and modes
    public static final int KEY_OPEN = Keyboard.KEY_RSHIFT;
    public static final int KEY_LEFT = Keyboard.KEY_N;
    public static final int KEY_RIGHT = Keyboard.KEY_M;
    public static final ActivationMode MODE_LEFT = ActivationMode.MOUSE;
    public static final ActivationMode MODE_RIGHT = ActivationMode.MOUSE;
    public static final boolean ANALYTICS = false;

    // HUD
    public static final boolean HUD_ENABLED = true;
    public static final boolean HUD_CPS_LEFT = true;
    public static final boolean HUD_CPS_RIGHT = true;
    public static final boolean HUD_STATUS = false;
    public static final boolean HUD_BACKGROUND = true;
    public static final int HUD_PADDING = 5;
    public static final int HUD_TEXT_COLOR = 0xFFFFFFFF;
    public static final int HUD_BG_COLOR = 0x90000000;
    public static final HudAnchor HUD_ANCHOR = HudAnchor.TOP_LEFT;
    public static final int HUD_MARGIN = 4;
    public static final int HUD_X = 4;
    public static final int HUD_Y = 4;
}
