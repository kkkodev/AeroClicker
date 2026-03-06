package com.icysnex.ghosttap.core.analytics;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.stream.Stream;

public class Tracker {

    public static boolean enabled = true;


    static final ConcurrentLinkedQueue<ClickData> history = new ConcurrentLinkedQueue<>();
     static final ConcurrentLinkedQueue<Long> window = new ConcurrentLinkedQueue<>();

    public static void record(double targetCps, long hold, long interval, double trend) {
        if (!enabled)
            return;

        long now = System.currentTimeMillis();
        window.add(now);

        if (history.size() >= 2000)
            history.poll();

        history.add(new ClickData(now, targetCps, getCurrentCps(), hold, interval, trend));
    }


    public static void clear() {
        history.clear();
    }

    public static int size() {
        return history.size();
    }

    public static Stream<ClickData> stream() {
        return history.stream();
    }

    public static List<ClickData> copy() {
        return new ArrayList<>(history);
    }


    public static double getCurrentCps() {
        long now = System.currentTimeMillis();

        window.removeIf(timestamp -> now - timestamp > 1000);
        return window.size();
    }
}
