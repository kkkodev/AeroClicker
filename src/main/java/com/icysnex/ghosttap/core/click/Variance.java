package com.icysnex.ghosttap.core.click;

import java.util.concurrent.ThreadLocalRandom;

public abstract class Variance {

    public static double gaussian(double mean, double standardDeviation) {
        ThreadLocalRandom random = ThreadLocalRandom.current();

        return mean + random.nextGaussian() * standardDeviation;
    }

    public static boolean chance(double probability) {
        ThreadLocalRandom random = ThreadLocalRandom.current();

        return random.nextDouble() < probability;
    }

    public static double range(double min, double max) {
        ThreadLocalRandom random = ThreadLocalRandom.current();

        return random.nextDouble(min, max);
    }


    public static double trend(double current, double volatility, double tension) {
        double move = range(-volatility, volatility);
        return (current + move) * (1.0 - tension);
    }

    public static double lerp(double start, double end, double amount) {
        return start + amount * (end - start);
    }
}
