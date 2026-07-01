package com.icysnex.ghosttap.config;

import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.analytics.Tracker;
import net.minecraftforge.common.config.Configuration;
import net.minecraftforge.common.config.Property;
import org.lwjgl.input.Keyboard;

import java.io.File;
import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

public class ConfigHandler {

    private static final String CAT_LEFT = "left_clicker";
    private static final String CAT_RIGHT = "right_clicker";
    private static final String CAT_KEYS = "keys";

    private static Configuration config;

    // Keybinds (LWJGL key codes). Rebindable from the GUI.
    public static int openGuiKey = Keyboard.KEY_RSHIFT;
    public static int toggleLeftKey = Keyboard.KEY_R;
    public static int toggleRightKey = Keyboard.KEY_H;


    public static void loadConfig(File file) {
        config = new Configuration(file);
        config.load();

        sync(false);
    }

    public static void saveConfig() {
        if (config == null)
            return;

        sync(true);

        if (config.hasChanged())
            config.save();
    }


    // One code path for both directions: save=false reads config into the live
    // fields, save=true writes the live fields back into the config.
    private static void sync(boolean save) {
        syncClicker(CAT_LEFT, Clicker.LEFT, save);
        syncClicker(CAT_RIGHT, Clicker.RIGHT, save);

        openGuiKey = key(CAT_KEYS, "openGui", openGuiKey, save);
        toggleLeftKey = key(CAT_KEYS, "toggleLeft", toggleLeftKey, save);
        toggleRightKey = key(CAT_KEYS, "toggleRight", toggleRightKey, save);

        Property analytics = config.get(CAT_KEYS, "analyticsEnabled", Tracker.enabled, "Record click analytics for tuning/export");
        if (save) analytics.set(Tracker.enabled);
        else Tracker.enabled = analytics.getBoolean();
    }

    private static void syncClicker(String cat, Clicker c, boolean save) {
        bind(cat, "cpsMean", () -> c.cpsMean, v -> c.cpsMean = v, save);
        bind(cat, "cpsStandardDeviation", () -> c.cpsStandardDeviation, v -> c.cpsStandardDeviation = v, save);
        bind(cat, "cpsMin", () -> c.cpsMin, v -> c.cpsMin = v, save);
        bind(cat, "cpsMax", () -> c.cpsMax, v -> c.cpsMax = v, save);
        bind(cat, "cpsMinMaxFallout", () -> c.cpsMinMaxFallout, v -> c.cpsMinMaxFallout = v, save);

        bind(cat, "spikeChance", () -> c.spikeChance, v -> c.spikeChance = v, save);
        bind(cat, "spikeMin", () -> c.spikeMin, v -> c.spikeMin = v, save);
        bind(cat, "spikeMax", () -> c.spikeMax, v -> c.spikeMax = v, save);

        bind(cat, "stutterChance", () -> c.stutterChance, v -> c.stutterChance = v, save);
        bind(cat, "stutterMin", () -> c.stutterMin, v -> c.stutterMin = v, save);
        bind(cat, "stutterMax", () -> c.stutterMax, v -> c.stutterMax = v, save);

        bind(cat, "holdMsMean", () -> c.holdMsMean, v -> c.holdMsMean = v, save);
        bind(cat, "holdMsStandardDeviation", () -> c.holdMsStandardDeviation, v -> c.holdMsStandardDeviation = v, save);
        bind(cat, "holdMsMin", () -> c.holdMsMin, v -> c.holdMsMin = v, save);
        bind(cat, "holdMsMax", () -> c.holdMsMax, v -> c.holdMsMax = v, save);

        bind(cat, "holdMsHeavyChance", () -> c.holdMsHeavyChance, v -> c.holdMsHeavyChance = v, save);
        bind(cat, "holdMsHeavyMin", () -> c.holdMsHeavyMin, v -> c.holdMsHeavyMin = v, save);
        bind(cat, "holdMsHeavyMax", () -> c.holdMsHeavyMax, v -> c.holdMsHeavyMax = v, save);

        bind(cat, "rhythmVolatility", () -> c.rhythmVolatility, v -> c.rhythmVolatility = v, save);
        bind(cat, "rhythmTension", () -> c.rhythmTension, v -> c.rhythmTension = v, save);
    }

    private static void bind(String cat, String key, DoubleSupplier get, DoubleConsumer set, boolean save) {
        Property p = config.get(cat, key, get.getAsDouble());
        if (save) p.set(get.getAsDouble());
        else set.accept(p.getDouble());
    }

    private static int key(String cat, String name, int current, boolean save) {
        Property p = config.get(cat, name, current, "LWJGL key code");
        if (save) {
            p.set(current);
            return current;
        }
        return p.getInt();
    }
}
