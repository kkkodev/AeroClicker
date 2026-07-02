package com.icysnex.ghosttap.core;

import com.icysnex.ghosttap.core.analytics.Tracker;
import com.icysnex.ghosttap.utils.ConfigCodec;

import java.util.Map;
import java.util.concurrent.locks.LockSupport;
import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

// Two independent clickers, one per mouse button, each with its own thread,
// humanization params and gates.
public class Clicker implements Runnable {

    public static final Clicker LEFT = new Clicker(InputMouse.BUTTON_LEFT);
    public static final Clicker RIGHT = new Clicker(InputMouse.BUTTON_RIGHT);


    public double cpsMean;
    public double cpsStandardDeviation;
    public double cpsMin;
    public double cpsMax;
    public double cpsMinMaxFallout;

    public double spikeChance;
    public double spikeMin;
    public double spikeMax;

    public double stutterChance;
    public double stutterMin;
    public double stutterMax;

    public double holdMsMean;
    public double holdMsStandardDeviation;
    public double holdMsMin;
    public double holdMsMax;

    public double holdMsHeavyChance;
    public double holdMsHeavyMin;
    public double holdMsHeavyMax;

    public double rhythmVolatility;
    public double rhythmTension;


    public final Tracker tracker = new Tracker();
    public final ClickerGates gates = new ClickerGates();

    // Toggle-mode intent (persists so gates keep gating it); Mouse-mode arm gate.
    public volatile boolean toggledOn = false;
    public volatile boolean armed = false;

    private final byte button;
    private final Thread thread;

    private volatile boolean enabled = false;
    private double currentTrend = 0;


    private Clicker(byte button) {
        this.button = button;
        resetParams();

        String name = button == InputMouse.BUTTON_LEFT ? "Left" : "Right";
        thread = new Thread(this, "GhostTap-Clicker-" + name);
        thread.setDaemon(true);
        thread.start();
    }

    public void resetParams() {
        cpsMean = Defaults.CPS_MEAN;
        cpsStandardDeviation = Defaults.CPS_STD_DEV;
        cpsMin = Defaults.CPS_MIN;
        cpsMax = Defaults.CPS_MAX;
        cpsMinMaxFallout = Defaults.CPS_FALLOUT;

        spikeChance = Defaults.SPIKE_CHANCE;
        spikeMin = Defaults.SPIKE_MIN;
        spikeMax = Defaults.SPIKE_MAX;

        stutterChance = Defaults.STUTTER_CHANCE;
        stutterMin = Defaults.STUTTER_MIN;
        stutterMax = Defaults.STUTTER_MAX;

        holdMsMean = Defaults.HOLD_MEAN;
        holdMsStandardDeviation = Defaults.HOLD_STD_DEV;
        holdMsMin = Defaults.HOLD_MIN;
        holdMsMax = Defaults.HOLD_MAX;

        holdMsHeavyChance = Defaults.HEAVY_CHANCE;
        holdMsHeavyMin = Defaults.HEAVY_MIN;
        holdMsHeavyMax = Defaults.HEAVY_MAX;

        rhythmVolatility = Defaults.RHYTHM_VOLATILITY;
        rhythmTension = Defaults.RHYTHM_TENSION;
    }


    public interface ParamSink {
        void accept(String name, DoubleSupplier get, DoubleConsumer set);
    }

    // Table of every tunable param, so serialize/config never repeat the field list.
    public void params(ParamSink s) {
        s.accept("cpsMean", () -> cpsMean, v -> cpsMean = v);
        s.accept("cpsStandardDeviation", () -> cpsStandardDeviation, v -> cpsStandardDeviation = v);
        s.accept("cpsMin", () -> cpsMin, v -> cpsMin = v);
        s.accept("cpsMax", () -> cpsMax, v -> cpsMax = v);
        s.accept("cpsMinMaxFallout", () -> cpsMinMaxFallout, v -> cpsMinMaxFallout = v);
        s.accept("spikeChance", () -> spikeChance, v -> spikeChance = v);
        s.accept("spikeMin", () -> spikeMin, v -> spikeMin = v);
        s.accept("spikeMax", () -> spikeMax, v -> spikeMax = v);
        s.accept("stutterChance", () -> stutterChance, v -> stutterChance = v);
        s.accept("stutterMin", () -> stutterMin, v -> stutterMin = v);
        s.accept("stutterMax", () -> stutterMax, v -> stutterMax = v);
        s.accept("holdMsMean", () -> holdMsMean, v -> holdMsMean = v);
        s.accept("holdMsStandardDeviation", () -> holdMsStandardDeviation, v -> holdMsStandardDeviation = v);
        s.accept("holdMsMin", () -> holdMsMin, v -> holdMsMin = v);
        s.accept("holdMsMax", () -> holdMsMax, v -> holdMsMax = v);
        s.accept("holdMsHeavyChance", () -> holdMsHeavyChance, v -> holdMsHeavyChance = v);
        s.accept("holdMsHeavyMin", () -> holdMsHeavyMin, v -> holdMsHeavyMin = v);
        s.accept("holdMsHeavyMax", () -> holdMsHeavyMax, v -> holdMsHeavyMax = v);
        s.accept("rhythmVolatility", () -> rhythmVolatility, v -> rhythmVolatility = v);
        s.accept("rhythmTension", () -> rhythmTension, v -> rhythmTension = v);
    }

    private static final String TOKEN = "ghosttap-clicker";

    // Params + gates as a shareable clipboard token.
    public String export() {
        Map<String, String> m = ConfigCodec.map();
        params((name, get, set) -> ConfigCodec.put(m, name, get.getAsDouble()));

        ConfigCodec.put(m, "gate.weapons", gates.weapons);
        ConfigCodec.put(m, "gate.tools", gates.tools);
        ConfigCodec.put(m, "gate.blocks", gates.blocks);
        ConfigCodec.put(m, "gate.other", gates.other);
        ConfigCodec.put(m, "gate.allowBlockBreak", gates.allowBlockBreak);
        ConfigCodec.put(m, "gate.allowInMenu", gates.allowInMenu);
        ConfigCodec.put(m, "gate.pauseWhileUsingItem", gates.pauseWhileUsingItem);
        ConfigCodec.put(m, "gate.survival", gates.survival);
        ConfigCodec.put(m, "gate.creative", gates.creative);
        ConfigCodec.put(m, "gate.adventure", gates.adventure);
        for (int i = 0; i < gates.slots.length; i++)
            ConfigCodec.put(m, "gate.slot" + (i + 1), gates.slots[i]);

        return ConfigCodec.encode(TOKEN, m);
    }

    // Unknown keys are ignored, missing ones keep their current value.
    public boolean importFrom(String token) {
        Map<String, String> m = ConfigCodec.decode(TOKEN, token);
        if (m == null)
            return false;

        params((name, get, set) -> set.accept(ConfigCodec.number(m, name, get.getAsDouble())));

        gates.weapons = ConfigCodec.flag(m, "gate.weapons", gates.weapons);
        gates.tools = ConfigCodec.flag(m, "gate.tools", gates.tools);
        gates.blocks = ConfigCodec.flag(m, "gate.blocks", gates.blocks);
        gates.other = ConfigCodec.flag(m, "gate.other", gates.other);
        gates.allowBlockBreak = ConfigCodec.flag(m, "gate.allowBlockBreak", gates.allowBlockBreak);
        gates.allowInMenu = ConfigCodec.flag(m, "gate.allowInMenu", gates.allowInMenu);
        gates.pauseWhileUsingItem = ConfigCodec.flag(m, "gate.pauseWhileUsingItem", gates.pauseWhileUsingItem);
        gates.survival = ConfigCodec.flag(m, "gate.survival", gates.survival);
        gates.creative = ConfigCodec.flag(m, "gate.creative", gates.creative);
        gates.adventure = ConfigCodec.flag(m, "gate.adventure", gates.adventure);
        for (int i = 0; i < gates.slots.length; i++)
            gates.slots[i] = ConfigCodec.flag(m, "gate.slot" + (i + 1), gates.slots[i]);

        return true;
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

    // On-state for the mode (not whether it's mid-click): Mouse=armed, Toggle=toggled, Hold=live.
    public boolean isActive(ActivationMode mode) {
        switch (mode) {
            case MOUSE: return armed;
            case TOGGLE: return toggledOn;
            default: return enabled;
        }
    }

    public void setActive(ActivationMode mode, boolean value) {
        switch (mode) {
            case MOUSE: armed = value; break;
            case TOGGLE: toggledOn = value; break;
            default: setEnabled(value); break;
        }
    }

    // Clears all state; used on mode change so nothing stays stuck on.
    public void deactivate() {
        setEnabled(false);
        toggledOn = false;
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

            InputMouse.down(button);
            long thisClickStartTime = System.nanoTime();

            nextPressAnchor = thisClickStartTime + intervalNanos;

            long holdNanos = calculateHoldNanos(intervalNanos);
            waitUntil(thisClickStartTime + holdNanos);

            InputMouse.up(button);

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
