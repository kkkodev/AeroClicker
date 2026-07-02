package com.icysnex.ghosttap.core.click;

import com.icysnex.ghosttap.core.Defaults;

import java.util.Arrays;

// Per-clicker conditions deciding whether clicking is allowed right now. Pure
// data; evaluated on the main thread by Gates.
public class ClickerGates {

    public final boolean[] slots = new boolean[9];

    // Allowed held-item categories (all true = any item).
    public boolean weapons;
    public boolean tools;
    public boolean blocks;
    public boolean other;

    public boolean allowBlockBreak;
    public boolean allowInMenu;
    public boolean pauseWhileUsingItem;

    // Allowed game modes.
    public boolean survival;
    public boolean creative;
    public boolean adventure;

    public ClickerGates(Defaults.Profile d) {
        weapons = d.weapons;
        tools = d.tools;
        blocks = d.blocks;
        other = d.other;
        allowBlockBreak = d.allowBlockBreak;
        allowInMenu = d.allowInMenu;
        pauseWhileUsingItem = d.pauseWhileUsingItem;
        survival = d.survival;
        creative = d.creative;
        adventure = d.adventure;
        Arrays.fill(slots, d.slot);
    }
}
