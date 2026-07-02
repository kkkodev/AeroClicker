package com.icysnex.ghosttap.core;

import java.util.Arrays;

// Per-clicker conditions deciding whether clicking is allowed right now. Pure
// data; evaluated on the main thread by Gates.
public class ClickerGates {

    public final boolean[] slots = new boolean[9];

    // Allowed held-item categories (all true = any item).
    public boolean weapons = Defaults.GATE_ITEM;
    public boolean tools = Defaults.GATE_ITEM;
    public boolean blocks = Defaults.GATE_ITEM;
    public boolean other = Defaults.GATE_ITEM;

    public boolean allowBlockBreak = Defaults.ALLOW_BLOCK_BREAK;
    public boolean allowInMenu = Defaults.ALLOW_IN_MENU;
    public boolean pauseWhileUsingItem = Defaults.PAUSE_ON_ITEM_USE;

    // Allowed game modes.
    public boolean survival = Defaults.GATE_GAMEMODE;
    public boolean creative = Defaults.GATE_GAMEMODE;
    public boolean adventure = Defaults.GATE_GAMEMODE;

    public ClickerGates() {
        Arrays.fill(slots, Defaults.GATE_SLOT);
    }
}
