package com.icysnex.ghosttap.core;

import com.icysnex.ghosttap.core.analytics.Tracker;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.locks.LockSupport;
import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

public class Clicker implements Runnable {

    // Two independent clickers, one per mouse button. Each runs its own daemon
    // thread and owns its own humanization parameters so left and right can be
    // tuned separately.
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

    // Persistent intent for Toggle mode (survives so gates can keep gating it).
    public volatile boolean toggledOn = false;
    // Mouse-mode gate: press the key to arm, then the real mouse button drives it.
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

    // Single source of the default tuning values, used at construction and by the
    // reset button.
    public void resetParams() {
        cpsMean = 12.0;
        cpsStandardDeviation = 1.5;
        cpsMin = 8.0;
        cpsMax = 18.0;
        cpsMinMaxFallout = 0.8;

        spikeChance = 0.04;
        spikeMin = 1;
        spikeMax = 3;

        stutterChance = 0.03;
        stutterMin = 4;
        stutterMax = 7;

        holdMsMean = 38;
        holdMsStandardDeviation = 6.5;
        holdMsMin = 18;
        holdMsMax = 75;

        holdMsHeavyChance = 0.015;
        holdMsHeavyMin = 15;
        holdMsHeavyMax = 35;

        rhythmVolatility = 0.5;
        rhythmTension = 0.04;
    }


    public interface ParamSink {
        void accept(String name, DoubleSupplier get, DoubleConsumer set);
    }

    // Single table of every tunable param, used to serialize and deserialize a
    // clicker without repeating the field list per operation.
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

    // Serialize all params to a shareable text blob.
    public String export() {
        StringBuilder sb = new StringBuilder("ghosttap-clicker\n");
        params((name, get, set) -> sb.append(name).append('=').append(get.getAsDouble()).append('\n'));
        return sb.toString();
    }

    // Apply params from an exported blob. Unknown keys are ignored; missing ones
    // keep their current value. Returns false if the text isn't a valid export.
    public boolean importFrom(String text) {
        if (text == null || !text.trim().startsWith("ghosttap-clicker"))
            return false;

        Map<String, Double> values = new HashMap<>();
        for (String line : text.split("\\r?\\n")) {
            int eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            try {
                values.put(line.substring(0, eq).trim(), Double.parseDouble(line.substring(eq + 1).trim()));
            } catch (NumberFormatException ignored) {
            }
        }

        if (values.isEmpty())
            return false;

        params((name, get, set) -> {
            Double v = values.get(name);
            if (v != null)
                set.accept(v);
        });
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

    // Full reset, used when the activation mode changes so nothing stays stuck on.
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
