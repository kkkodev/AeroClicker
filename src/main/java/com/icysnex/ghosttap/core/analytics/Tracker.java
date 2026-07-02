package com.icysnex.ghosttap.core.analytics;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.stream.Stream;

// One tracker per clicker, so left and right keep separate history. The enabled
// flag stays global: a single master switch for all recording.
public class Tracker {

    public static boolean enabled = false;


    private final ConcurrentLinkedQueue<ClickData> history = new ConcurrentLinkedQueue<>();
    private final ConcurrentLinkedQueue<Long> window = new ConcurrentLinkedQueue<>();

    public void record(double targetCps, long hold, long interval, double trend) {
        if (!enabled)
            return;

        long now = System.currentTimeMillis();
        window.add(now);

        if (history.size() >= 2000)
            history.poll();

        history.add(new ClickData(now, targetCps, getCurrentCps(), hold, interval, trend));
    }

    // Physical click: no generated target/hold/trend, just timing.
    public void recordReal(long interval) {
        record(0, 0, interval, 0);
    }


    public void clear() {
        history.clear();
        window.clear();
    }

    public int size() {
        return history.size();
    }

    public Stream<ClickData> stream() {
        return history.stream();
    }

    public List<ClickData> copy() {
        return new ArrayList<>(history);
    }


    public double getCurrentCps() {
        long now = System.currentTimeMillis();

        window.removeIf(timestamp -> now - timestamp > 1000);
        return window.size();
    }

    public double getAverageCps() {
        return history.stream().mapToDouble(d -> d.actualCps).average().orElse(0.0);
    }
}
