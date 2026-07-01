package com.icysnex.ghosttap.config;

import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.ClickerGates;
import com.icysnex.ghosttap.core.HudAnchor;
import com.icysnex.ghosttap.core.analytics.Tracker;
import net.minecraftforge.common.config.Configuration;
import net.minecraftforge.common.config.Property;
import org.lwjgl.input.Keyboard;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;
import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

public class ConfigHandler {

    private static final String CAT_LEFT = "left_clicker";
    private static final String CAT_RIGHT = "right_clicker";
    private static final String CAT_KEYS = "keys";
    private static final String CAT_HUD = "hud";

    private static Configuration config;

    // Keybinds (LWJGL key codes). Rebindable from the GUI.
    public static int openGuiKey = Keyboard.KEY_RSHIFT;
    public static int toggleLeftKey = Keyboard.KEY_R;
    public static int toggleRightKey = Keyboard.KEY_H;

    // Per-clicker activation mode.
    public static ActivationMode leftMode = ActivationMode.TOGGLE;
    public static ActivationMode rightMode = ActivationMode.TOGGLE;

    // On-screen HUD.
    public static boolean hudEnabled = true;
    public static boolean hudCpsLeft = true;
    public static boolean hudCpsRight = true;
    public static boolean hudShowStatus = false;
    public static boolean hudBackground = true;
    public static int hudPadding = 3;
    public static int hudTextColor = 0xFFFFFFFF;
    public static int hudBgColor = 0x90000000;
    public static HudAnchor hudAnchor = HudAnchor.TOP_LEFT;
    public static int hudMargin = 4;
    public static int hudX = 4;
    public static int hudY = 4;


    public static void loadConfig(File file) {
        config = new Configuration(file);
        config.load();

        sync(false);
    }

    public static void resetHud() {
        hudEnabled = true;
        hudCpsLeft = true;
        hudCpsRight = true;
        hudShowStatus = false;
        hudBackground = true;
        hudPadding = 3;
        hudTextColor = 0xFFFFFFFF;
        hudBgColor = 0x90000000;
        hudAnchor = HudAnchor.TOP_LEFT;
        hudMargin = 4;
        hudX = 4;
        hudY = 4;
    }

    // HUD settings as a shareable base64 token.
    public static String exportHud() {
        StringBuilder sb = new StringBuilder("ghosttap-hud\n");
        sb.append("enabled=").append(hudEnabled ? 1 : 0).append('\n');
        sb.append("cpsLeft=").append(hudCpsLeft ? 1 : 0).append('\n');
        sb.append("cpsRight=").append(hudCpsRight ? 1 : 0).append('\n');
        sb.append("showStatus=").append(hudShowStatus ? 1 : 0).append('\n');
        sb.append("background=").append(hudBackground ? 1 : 0).append('\n');
        sb.append("padding=").append(hudPadding).append('\n');
        sb.append("textColor=").append(hudTextColor).append('\n');
        sb.append("bgColor=").append(hudBgColor).append('\n');
        sb.append("anchor=").append(hudAnchor.name()).append('\n');
        sb.append("margin=").append(hudMargin).append('\n');
        sb.append("x=").append(hudX).append('\n');
        sb.append("y=").append(hudY).append('\n');
        return Base64.getEncoder().encodeToString(sb.toString().getBytes(StandardCharsets.UTF_8));
    }

    public static boolean importHud(String token) {
        if (token == null)
            return false;

        String data;
        try {
            data = new String(Base64.getDecoder().decode(token.trim()), StandardCharsets.UTF_8);
        } catch (IllegalArgumentException e) {
            data = token;
        }
        if (!data.trim().startsWith("ghosttap-hud"))
            return false;

        Map<String, String> m = new HashMap<>();
        for (String line : data.split("\\r?\\n")) {
            int eq = line.indexOf('=');
            if (eq > 0)
                m.put(line.substring(0, eq).trim(), line.substring(eq + 1).trim());
        }

        hudEnabled = flag(m, "enabled", hudEnabled);
        hudCpsLeft = flag(m, "cpsLeft", hudCpsLeft);
        hudCpsRight = flag(m, "cpsRight", hudCpsRight);
        hudShowStatus = flag(m, "showStatus", hudShowStatus);
        hudBackground = flag(m, "background", hudBackground);
        hudPadding = num(m, "padding", hudPadding);
        hudTextColor = num(m, "textColor", hudTextColor);
        hudBgColor = num(m, "bgColor", hudBgColor);
        hudMargin = num(m, "margin", hudMargin);
        hudX = num(m, "x", hudX);
        hudY = num(m, "y", hudY);
        try {
            if (m.containsKey("anchor"))
                hudAnchor = HudAnchor.valueOf(m.get("anchor"));
        } catch (IllegalArgumentException ignored) {
        }
        return true;
    }

    private static boolean flag(Map<String, String> m, String key, boolean current) {
        String v = m.get(key);
        if (v == null)
            return current;
        return v.equals("1") || v.equalsIgnoreCase("true");
    }

    private static int num(Map<String, String> m, String key, int current) {
        String v = m.get(key);
        if (v == null)
            return current;
        try {
            return Integer.parseInt(v);
        } catch (NumberFormatException e) {
            return current;
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

        syncGates(CAT_LEFT, Clicker.LEFT.gates, save);
        syncGates(CAT_RIGHT, Clicker.RIGHT.gates, save);

        openGuiKey = key(CAT_KEYS, "openGui", openGuiKey, save);
        toggleLeftKey = key(CAT_KEYS, "toggleLeft", toggleLeftKey, save);
        toggleRightKey = key(CAT_KEYS, "toggleRight", toggleRightKey, save);

        leftMode = mode(CAT_KEYS, "leftMode", leftMode, save);
        rightMode = mode(CAT_KEYS, "rightMode", rightMode, save);

        Property analytics = config.get(CAT_KEYS, "analyticsEnabled", Tracker.enabled, "Record click analytics for tuning/export");
        if (save) analytics.set(Tracker.enabled);
        else Tracker.enabled = analytics.getBoolean();

        hudEnabled = bool(CAT_HUD, "enabled", hudEnabled, save);
        hudCpsLeft = bool(CAT_HUD, "cpsLeft", hudCpsLeft, save);
        hudCpsRight = bool(CAT_HUD, "cpsRight", hudCpsRight, save);
        hudShowStatus = bool(CAT_HUD, "showStatus", hudShowStatus, save);
        hudBackground = bool(CAT_HUD, "background", hudBackground, save);
        hudPadding = integer(CAT_HUD, "padding", hudPadding, save);
        hudTextColor = integer(CAT_HUD, "textColor", hudTextColor, save);
        hudBgColor = integer(CAT_HUD, "bgColor", hudBgColor, save);

        Property anchor = config.get(CAT_HUD, "anchor", hudAnchor.name());
        if (save) anchor.set(hudAnchor.name());
        else try {
            hudAnchor = HudAnchor.valueOf(anchor.getString());
        } catch (IllegalArgumentException ignored) {
        }

        hudMargin = integer(CAT_HUD, "margin", hudMargin, save);
        hudX = integer(CAT_HUD, "x", hudX, save);
        hudY = integer(CAT_HUD, "y", hudY, save);
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

    private static void syncGates(String cat, ClickerGates g, boolean save) {
        g.allowBlockBreak = bool(cat, "allowBlockBreak", g.allowBlockBreak, save);
        g.allowInMenu = bool(cat, "allowInMenu", g.allowInMenu, save);
        g.pauseWhileUsingItem = bool(cat, "pauseWhileUsingItem", g.pauseWhileUsingItem, save);

        g.weapons = bool(cat, "catWeapons", g.weapons, save);
        g.tools = bool(cat, "catTools", g.tools, save);
        g.blocks = bool(cat, "catBlocks", g.blocks, save);
        g.other = bool(cat, "catOther", g.other, save);

        g.survival = bool(cat, "gmSurvival", g.survival, save);
        g.creative = bool(cat, "gmCreative", g.creative, save);
        g.adventure = bool(cat, "gmAdventure", g.adventure, save);

        for (int i = 0; i < g.slots.length; i++)
            g.slots[i] = bool(cat, "slot" + (i + 1), g.slots[i], save);
    }

    private static boolean bool(String cat, String key, boolean current, boolean save) {
        Property p = config.get(cat, key, current);
        if (save) {
            p.set(current);
            return current;
        }
        return p.getBoolean();
    }

    private static int integer(String cat, String key, int current, boolean save) {
        Property p = config.get(cat, key, current);
        if (save) {
            p.set(current);
            return current;
        }
        return p.getInt();
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
