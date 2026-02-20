package com.icysnex.ghosttap.core;

import java.util.concurrent.ThreadLocalRandom;

public abstract class Variance {

    public static long getGaussian(double mean, double standardDeviation) {
        ThreadLocalRandom random = ThreadLocalRandom.current();

        double noise = (random.nextDouble() + random.nextDouble() + random.nextDouble() + random.nextDouble() - 2.0) / 2.0;
        return (long)(mean + (noise * standardDeviation));
    }

    public static boolean chance(double probability) {
        ThreadLocalRandom random = ThreadLocalRandom.current();

        return random.nextDouble() < probability;
    }
}
