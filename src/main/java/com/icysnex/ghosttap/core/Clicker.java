package com.icysnex.ghosttap.core;

import com.icysnex.ghosttap.core.analytics.Tracker;

import java.util.concurrent.locks.LockSupport;

public class Clicker implements Runnable {

    // Two independent clickers, one per mouse button. Each runs its own daemon
    // thread and owns its own humanization parameters so left and right can be
    // tuned separately.
    public static final Clicker LEFT = new Clicker(InputMouse.BUTTON_LEFT);
    public static final Clicker RIGHT = new Clicker(InputMouse.BUTTON_RIGHT);


    public double cpsMean = 12.0;
    public double cpsStandardDeviation = 1.5;
    public double cpsMin = 8.0;
    public double cpsMax = 18.0;
    public double cpsMinMaxFallout = 0.8;

    public double spikeChance = 0.04;
    public double spikeMin = 1;
    public double spikeMax = 3;

    public double stutterChance = 0.03;
    public double stutterMin = 4;
    public double stutterMax = 7;

    public double holdMsMean = 38;
    public double holdMsStandardDeviation = 6.5;
    public double holdMsMin = 18;
    public double holdMsMax = 75;

    public double holdMsHeavyChance = 0.015;
    public double holdMsHeavyMin = 15;
    public double holdMsHeavyMax = 35;

    public double rhythmVolatility = 0.5;
    public double rhythmTension = 0.04;


    public final Tracker tracker = new Tracker();

    // Mouse-mode gate: press the key to arm, then the real mouse button drives it.
    public volatile boolean armed = false;

    private final byte button;
    private final Thread thread;

    private volatile boolean enabled = false;
    private double currentTrend = 0;


    private Clicker(byte button) {
        this.button = button;

        String name = button == InputMouse.BUTTON_LEFT ? "Left" : "Right";
        thread = new Thread(this, "GhostTap-Clicker-" + name);
        thread.setDaemon(true);
        thread.start();
    }


    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean value) {
        enabled = value;

        if (enabled) {
            LockSupport.unpark(thread);
        } else {
            if (InputMouse.spoofed(button) == InputMouse.STATE_DOWN)
                InputMouse.up(button);
        }
    }

    public void toggle() {
        setEnabled(!enabled);
    }

    // Full reset, used when the activation mode changes so nothing stays stuck on.
    public void deactivate() {
        setEnabled(false);
        armed = false;
        InputMouse.setMask(button, false);
    }


    @Override
    public void run() {
        long nextPressAnchor = System.nanoTime();

        while (true) {
            if (!enabled) {
                LockSupport.park();
                nextPressAnchor = System.nanoTime();
                currentTrend = 0;
                continue;
            }

            long intervalNanos = calculateNextIntervalNanos();
            waitUntil(nextPressAnchor);

            if (!enabled)
                continue;

            // --- CLICK START ---
            InputMouse.down(button);
            long thisClickStartTime = System.nanoTime();

            nextPressAnchor = thisClickStartTime + intervalNanos;

            long holdNanos = calculateHoldNanos(intervalNanos);
            waitUntil(thisClickStartTime + holdNanos);

            InputMouse.up(button);
            // --- CLICK END ---

            tracker.record(
                    1_000_000_000.0 / intervalNanos,
                    holdNanos,
                    intervalNanos,
                    currentTrend
            );
        }
    }


    void waitUntil(long targetNanoTime) {
        long remaining;

        while ((remaining = targetNanoTime - System.nanoTime()) > 0) {
            if (remaining > 1_500_000L) {
                LockSupport.parkNanos(1_000_000L);
            }
        }
    }

    long calculateNextIntervalNanos() {
        currentTrend = Variance.trend(currentTrend, rhythmVolatility, rhythmTension);
        double cps = Variance.gaussian(cpsMean + currentTrend, cpsStandardDeviation);

        if (Variance.chance(spikeChance)) {
            cps += Variance.range(spikeMin, spikeMax);
        }

        if (Variance.chance(stutterChance)) {
            cps -= Variance.range(stutterMin, stutterMax);
        }

        if (cps > cpsMax)
            cps = cpsMax + (Variance.range(-cpsMinMaxFallout, cpsMinMaxFallout));
        if (cps < cpsMin)
            cps = cpsMin + (Variance.range(-cpsMinMaxFallout, cpsMinMaxFallout));

        return (long)(1_000_000_000.0 / cps);
    }

    long calculateHoldNanos(long intervalNanos) {
        double speedFactor = 1.0 - (1_000_000_000.0 / intervalNanos / 25.0);
        double adjustedMean = holdMsMean * Math.max(0.8, speedFactor);

        double hold = Variance.gaussian(adjustedMean, holdMsStandardDeviation);

        if (Variance.chance(holdMsHeavyChance)) {
            hold += Variance.range(holdMsHeavyMin, holdMsHeavyMax);
        }

        hold = Math.max(holdMsMin, Math.min(holdMsMax, hold));

        long holdTarget = (long)(hold * 1_000_000L);
        long maxPossibleHold = (long)(intervalNanos * 0.85);

        return Math.min(holdTarget, maxPossibleHold);
    }
}
