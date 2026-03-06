package com.icysnex.ghosttap.core;

import java.util.concurrent.locks.LockSupport;

public class Clicker implements Runnable {

    static double CPS_MEAN = 12.0;
    static double CPS_STANDARDDEVIATION = 1.5;
    static double CPS_MIN = 8.0;
    static double CPS_MAX = 18.0;

    static double HOLD_MS_MEAN = 35;
    static double HOLD_MS_STANDARDDEVIATION = 8;
    static double HOLD_MS_MIN = 5;
    static double HOLD_MS_MAX = 65;

    static double RHYTHM_VOLATILITY = 0.6;
    static double RHYTHM_TENSION = 0.05;

    static double SKIP_CHANCE = 0.04;

    static double SPIKE_CHANCE = 0.05;
    static double SPIKE_MIN = 1;
    static double SPIKE_MAX = 4;


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
        long lastClickTime = System.nanoTime();

        while (true) {
            if (!enabled) {
                LockSupport.park();

                lastClickTime = System.nanoTime();
                currentHumanTrend = 0;
                continue;
            }

            long intervalNanos = calculateNextIntervalNanos();
            long targetPressTime = lastClickTime + intervalNanos;

            waitUntil(targetPressTime);

            if (!enabled)
                continue;

            if (Variance.chance(SKIP_CHANCE)) {
                lastClickTime = System.nanoTime();
                currentHumanTrend *= 0.5;
                continue;
            }

            InputMouse.downLeft();

            long holdNanos = calculateHoldNanos(intervalNanos);
            waitUntil(System.nanoTime() + holdNanos);

            InputMouse.upLeft();
            lastClickTime = targetPressTime;
        }
    }


    double currentHumanTrend = 0;

    void waitUntil(long targetNanoTime) {
        long remaining;

        while ((remaining = targetNanoTime - System.nanoTime()) > 0) {
            if (remaining > 1_500_000L) {
                LockSupport.parkNanos(1_000_000L);
            }
        }
    }

    long calculateNextIntervalNanos() {
        currentHumanTrend = Variance.trend(currentHumanTrend, RHYTHM_VOLATILITY, RHYTHM_TENSION);
        double cps = Variance.gaussian(CPS_MEAN + currentHumanTrend, CPS_STANDARDDEVIATION);

        if (Variance.chance(SPIKE_CHANCE)) {
            cps += Variance.range(SPIKE_MIN, SPIKE_MAX);
        }

        cps = Math.max(CPS_MIN, Math.min(CPS_MAX, cps));

        return (long) (1_000_000_000.0 / cps);
    }

    long calculateHoldNanos(long intervalNanos) {
        double hold = Variance.gaussian(HOLD_MS_MEAN, HOLD_MS_STANDARDDEVIATION);
        hold = Math.max(HOLD_MS_MIN, Math.min(HOLD_MS_MAX, hold));

        long holdTarget = (long) (hold * 1_000_000L);
        long maxPossibleHold = Math.max(1_000_000L, intervalNanos - 1_000_000L);

        return Math.min(holdTarget, maxPossibleHold);
    }
}