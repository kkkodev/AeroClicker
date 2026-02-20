package com.icysnex.ghosttap.core;

public abstract class Clicker {
    private static final double DURATION_MEAN = 150;
    private static final double DURATION_STANDARD_DEVIATION = 15;

    // Auto-clicker settings
    private static boolean active = false;
    private static double targetCPS = 10.0;
    private static long nextClickTime = -1;

    private static long nextReleaseTimeLeft = -1;

    public static void setEnabled(boolean enabled) {
        active = enabled;
        if (!enabled) nextClickTime = -1;
    }

    public static void tick() {
        final long currentTimeMillis = System.currentTimeMillis();

        // 1. Handle Releases (Existing logic)
        if (nextReleaseTimeLeft != -1 && currentTimeMillis >= nextReleaseTimeLeft) {
            InputMouse.upLeft();
            nextReleaseTimeLeft = -1;
        }

        // 2. Handle Auto-Clicking
        if (active) {
            if (nextClickTime == -1 || currentTimeMillis >= nextClickTime) {
                left();
                // Calculate interval in ms: (1000 / CPS)
                // We add some Gaussian variance here too so it's not "perfectly" 100ms
                long delay = (long) (1000.0 / targetCPS);
                nextClickTime = currentTimeMillis + delay;
            }
        }
    }

    public static void left() {
        // Prevent clicking if the mouse is already held down
        if (nextReleaseTimeLeft != -1)
            return;

        long holdTime = (long)Variance.getGaussian(DURATION_MEAN, DURATION_STANDARD_DEVIATION);

        InputMouse.downLeft();
        nextReleaseTimeLeft = System.currentTimeMillis() + holdTime;
    }
}