package com.icysnex.ghosttap.config;

import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.analytics.Tracker;
import net.minecraftforge.common.config.Configuration;
import net.minecraftforge.common.config.Property;
import org.lwjgl.input.Keyboard;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

public class ConfigHandler {

    private static final String CAT_LEFT = "left_clicker";
    private static final String CAT_RIGHT = "right_clicker";
    private static final String CAT_KEYS = "keys";

    private static Configuration config;
    private static File file;

    // Keybinds (LWJGL key codes). Rebindable from the GUI.
    public static int openGuiKey = Keyboard.KEY_RSHIFT;
    public static int toggleLeftKey = Keyboard.KEY_R;
    public static int toggleRightKey = Keyboard.KEY_H;

    // Per-clicker activation mode.
    public static ActivationMode leftMode = ActivationMode.TOGGLE;
    public static ActivationMode rightMode = ActivationMode.TOGGLE;


    public static void loadConfig(File configFile) {
        file = configFile;
        config = new Configuration(file);
        config.load();

        sync(false);
    }

    // Flush live settings to disk and return the whole config file as text, for
    // copying to the clipboard.
    public static String exportString() {
        saveConfig();
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    // Apply a config file pasted from the clipboard: write it, reload, and push
    // the values into the live fields. Runtime state is reset so nothing sticks.
    public static boolean importString(String text) {
        if (text == null || text.trim().isEmpty())
            return false;

        try {
            Files.write(file.toPath(), text.getBytes(StandardCharsets.UTF_8));
            config = new Configuration(file);
            config.load();
            sync(false);

            Clicker.LEFT.deactivate();
            Clicker.RIGHT.deactivate();
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
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

        leftMode = mode(CAT_KEYS, "leftMode", leftMode, save);
        rightMode = mode(CAT_KEYS, "rightMode", rightMode, save);

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

    private static ActivationMode mode(String cat, String name, ActivationMode current, boolean save) {
        Property p = config.get(cat, name, current.name(), "Activation mode: TOGGLE, HOLD or MOUSE");
        if (save) {
            p.set(current.name());
            return current;
        }
        try {
            return ActivationMode.valueOf(p.getString());
        } catch (IllegalArgumentException e) {
            return current;
        }
    }
}
