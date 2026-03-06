package com.icysnex.ghosttap.core;

import com.icysnex.ghosttap.core.analytics.Tracker;

import java.util.concurrent.locks.LockSupport;

public class Clicker implements Runnable {

    public static double CPS_MEAN = 12.0;
    public static double CPS_STANDARDDEVIATION = 1.5;
    public static double CPS_MIN = 8.0;
    public static double CPS_MAX = 18.0;
    public static double CPS_MINMAX_FALLOUT = 0.8;

    public static double SPIKE_CHANCE = 0.04;
    public static double SPIKE_MIN = 1;
    public static double SPIKE_MAX = 3;

    public static double STUTTER_CHANCE = 0.03;
    public static double STUTTER_MIN = 4;
    public static double STUTTER_MAX = 7;

    public static double HOLD_MS_MEAN = 38;
    public static double HOLD_MS_STANDARDDEVIATION = 6.5;
    public static double HOLD_MS_MIN = 18;
    public static double HOLD_MS_MAX = 75;

    public static double HOLD_MS_HEAVY_CHANCE = 0.015;
    public static double HOLD_MS_HEAVY_MIN = 15;
    public static double HOLD_MS_HEAVY_MAX = 35;

    public static double RHYTHM_VOLATILITY = 0.5;
    public static double RHYTHM_TENSION = 0.04;


    static Thread thread;

    static {
        thread = new Thread(new Clicker(), "GhostTap-Clicker-Thread");

        thread.setDaemon(true);
        thread.start();
    }


    static volatile boolean enabled = false;

    public static void setEnabled(boolean value) {
        enabled = value;

        if (enabled) {
            LockSupport.unpark(thread);
        } else {
            if (InputMouse.spoofedLeft == InputMouse.STATE_DOWN)
                InputMouse.upLeft();
        }
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
            InputMouse.downLeft();
            long thisClickStartTime = System.nanoTime();

            nextPressAnchor = thisClickStartTime + intervalNanos;

            long holdNanos = calculateHoldNanos(intervalNanos);
            waitUntil(thisClickStartTime + holdNanos);

            InputMouse.upLeft();
            // --- CLICK END ---

            Tracker.record(
                    1_000_000_000.0 / intervalNanos,
                    holdNanos,
                    intervalNanos,
                    currentTrend
            );
        }
    }


    double currentTrend = 0;

    void waitUntil(long targetNanoTime) {
        long remaining;

        while ((remaining = targetNanoTime - System.nanoTime()) > 0) {
            if (remaining > 1_500_000L) {
                LockSupport.parkNanos(1_000_000L);
            }
        }
    }

    long calculateNextIntervalNanos() {
        currentTrend = Variance.trend(currentTrend, RHYTHM_VOLATILITY, RHYTHM_TENSION);
        double cps = Variance.gaussian(CPS_MEAN + currentTrend, CPS_STANDARDDEVIATION);

        if (Variance.chance(SPIKE_CHANCE)) {
            cps += Variance.range(SPIKE_MIN, SPIKE_MAX);
        }

        if (Variance.chance(STUTTER_CHANCE)) {
            cps -= Variance.range(STUTTER_MIN, STUTTER_MAX);
        }

        if (cps > CPS_MAX)
            cps = CPS_MAX + (Variance.range(-CPS_MINMAX_FALLOUT, CPS_MINMAX_FALLOUT));
        if (cps < CPS_MIN)
            cps = CPS_MIN + (Variance.range(-CPS_MINMAX_FALLOUT, CPS_MINMAX_FALLOUT));

        return (long)(1_000_000_000.0 / cps);
    }

    long calculateHoldNanos(long intervalNanos) {
        double speedFactor = 1.0 - (1_000_000_000.0 / intervalNanos / 25.0);
        double adjustedMean = HOLD_MS_MEAN * Math.max(0.8, speedFactor);

        double hold = Variance.gaussian(adjustedMean, HOLD_MS_STANDARDDEVIATION);

        if (Variance.chance(HOLD_MS_HEAVY_CHANCE)) {
            hold += Variance.range(HOLD_MS_HEAVY_MIN, HOLD_MS_HEAVY_MAX);
        }

        hold = Math.max(HOLD_MS_MIN, Math.min(HOLD_MS_MAX, hold));

        long holdTarget = (long)(hold * 1_000_000L);
        long maxPossibleHold = (long)(intervalNanos * 0.85);

        return Math.min(holdTarget, maxPossibleHold);
    }
}