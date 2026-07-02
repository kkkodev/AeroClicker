package com.icysnex.ghosttap.core.input;

import java.util.concurrent.ConcurrentLinkedQueue;

// Counts clicks per second as the game actually sees them (real + spoofed),
// recorded by the mixin on each button down-edge. Always on, cheap.
public final class Cps {

    private static final ConcurrentLinkedQueue<Long> LEFT = new ConcurrentLinkedQueue<>();
    private static final ConcurrentLinkedQueue<Long> RIGHT = new ConcurrentLinkedQueue<>();

    private Cps() {
    }

    public static void hit(byte button) {
        (button == InputMouse.BUTTON_LEFT ? LEFT : RIGHT).add(System.currentTimeMillis());
    }

    public static int left() {
        return count(LEFT);
    }

    public static int right() {
        return count(RIGHT);
    }

    private static int count(ConcurrentLinkedQueue<Long> window) {
        long now = System.currentTimeMillis();
        window.removeIf(timestamp -> now - timestamp > 1000);
        return window.size();
    }
}
