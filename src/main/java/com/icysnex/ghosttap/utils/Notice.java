package com.icysnex.ghosttap.utils;

// Transient status message shown by the config screen. Replaces chat feedback so
// actions work even with no world/player (e.g. opened from the Forge mods menu).
public final class Notice {

    private static volatile String message = "";
    private static volatile long until = 0;

    private Notice() {
    }

    public static void show(String text) {
        message = text;
        until = System.currentTimeMillis() + 2500;
    }

    public static String current() {
        return System.currentTimeMillis() < until ? message : null;
    }
}
