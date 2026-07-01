package com.icysnex.ghosttap.core;

import java.util.Arrays;

// Per-clicker conditions that decide whether clicking is allowed in the current
// context. Pure data; evaluated on the main thread by Gates.
public class ClickerGates {

    // Only click when the selected hotbar slot (0-8) is whitelisted.
    public final boolean[] slots = new boolean[9];

    // Held-item categories that are allowed. All true = any item.
    public boolean weapons = true;
    public boolean tools = true;
    public boolean blocks = true;
    public boolean other = true;

    // Allow clicking while aimed at a block (mining). Off = combat-focused.
    public boolean allowBlockBreak = true;
    // Allow clicking while a screen (inventory, chat, ...) is open.
    public boolean allowInMenu = false;
    // Pause while the player is using an item (eating, bow, blocking).
    public boolean pauseWhileUsingItem = true;

    // Game modes the clicker is allowed to run in.
    public boolean survival = true;
    public boolean creative = true;
    public boolean adventure = true;

    public ClickerGates() {
        Arrays.fill(slots, true);
    }
}
