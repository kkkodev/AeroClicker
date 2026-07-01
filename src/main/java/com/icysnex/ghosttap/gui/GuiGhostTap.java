package com.icysnex.ghosttap.gui;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.HudAnchor;
import com.icysnex.ghosttap.core.analytics.Analytics;
import com.icysnex.ghosttap.core.analytics.Tracker;
import com.icysnex.ghosttap.utils.Notice;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.ScaledResolution;
import org.lwjgl.input.Keyboard;
import org.lwjgl.input.Mouse;
import org.lwjgl.opengl.GL11;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

// Custom dark-panel config screen. Four tabs (Left / Right / General /
// Analytics), each a scrollable list of section headers + widgets. Widget coords
// are recomputed every frame in walk(), so the mouse handlers can reuse the same
// layout.
public class GuiGhostTap extends GuiScreen {

    private static final int PANEL_W = 250;
    private static final int PANEL_H = 210;
    private static final int TITLE_H = 18;
    private static final int TAB_H = 16;
    private static final int SUBTAB_H = 14;
    private static final int SECTION_H = 20;
    private static final int BOTTOM_PAD = 8;

    // A main tab holds one or more sub-tabs; a sub-tab holds a list of rows.
    private static class SubTab {
        final String name;
        final List<Object> rows;
        SubTab(String name, List<Object> rows) { this.name = name; this.rows = rows; }
    }

    private static class Tab {
        final String name;
        final List<SubTab> subs;
        Tab(String name, List<SubTab> subs) { this.name = name; this.subs = subs; }
    }

    private final List<Tab> tabs = new ArrayList<>();
    private int activeTab = 0;
    private int activeSub = 0;
    private int scroll = 0;

    private GuiSlider activeSlider;
    private GuiKeybind activeKeybind;
    private GuiHexField activeField;
    private String hoverTooltip;

    // Row wrapper that only shows when the condition holds (walk/mouseClicked skip
    // it otherwise). Lets rows appear/disappear without rebuilding the tab.
    private static class Cond {
        final java.util.function.BooleanSupplier show;
        final Object row;
        Cond(java.util.function.BooleanSupplier show, Object row) { this.show = show; this.row = row; }
    }

    private Object cond(java.util.function.BooleanSupplier show, Object row) {
        return new Cond(show, row);
    }

    // Screen to return to on close: null when opened in-game, the mods list when
    // opened via the Forge config button.
    private final GuiScreen parent;

    public GuiGhostTap() {
        this(null);
    }

    public GuiGhostTap(GuiScreen parent) {
        this.parent = parent;
    }

    // Panel geometry, filled in initGui.
    private int left, top;
    private int contentX, contentW, contentTop, contentBottom, contentH;

    @Override
    public void initGui() {
        left = (width - PANEL_W) / 2;
        top = (height - PANEL_H) / 2;

        contentX = left + 12;
        contentW = PANEL_W - 24;

        tabs.clear();
        tabs.add(new Tab("General", single(buildGeneralRows())));
        tabs.add(buildClickerTab(Clicker.LEFT));
        tabs.add(buildClickerTab(Clicker.RIGHT));
        tabs.add(new Tab("HUD", single(buildHudRows())));
        tabs.add(new Tab("Analytics", single(buildAnalyticsRows())));

        layoutContent();
    }

    private List<SubTab> single(List<Object> rows) {
        List<SubTab> subs = new ArrayList<>();
        subs.add(new SubTab("", rows));
        return subs;
    }

    // Content region depends on whether a sub-tab bar is visible for this tab.
    private void layoutContent() {
        int subBar = hasSubTabs() ? SUBTAB_H : 0;
        contentTop = top + TITLE_H + TAB_H + subBar + 4;
        contentBottom = top + PANEL_H;
        contentH = contentBottom - contentTop;
    }

    private boolean hasSubTabs() {
        return tabs.get(activeTab).subs.size() > 1;
    }

    private Tab buildClickerTab(Clicker c) {
        boolean left = c == Clicker.LEFT;
        List<SubTab> subs = new ArrayList<>();
        subs.add(new SubTab("CPS", buildCpsRows(c)));
        subs.add(new SubTab("Fatigue", buildFatigueRows(c)));
        subs.add(new SubTab("Filters", buildFiltersRows(c, left)));
        return new Tab(left ? "Left" : "Right", subs);
    }

    private List<Object> buildCpsRows(Clicker c) {
        List<Object> r = new ArrayList<>();

        r.add("CPS");
        // Mean widens the range if dragged past Min/Max; Min/Max also push Mean
        // back in — the relationship is kept from both sides.
        r.add(slider("Mean", 1, 30, 1, false,
                "Average click speed. Drag past Min or Max to widen the range.",
                () -> c.cpsMean, v -> { c.cpsMean = v; if (v < c.cpsMin) c.cpsMin = v; if (v > c.cpsMax) c.cpsMax = v; }));
        r.add(slider("Std deviation", 0, 6, 2, false,
                "How much the speed randomly varies around the Mean.\nHigher = more human, less consistent.",
                () -> c.cpsStandardDeviation, v -> c.cpsStandardDeviation = v));
        r.add(slider("Min", 1, 30, 1, false,
                "Lowest allowed click speed. Pushes Max and Mean up if needed.",
                () -> c.cpsMin, v -> { c.cpsMin = v; if (c.cpsMax < v) c.cpsMax = v; if (c.cpsMean < v) c.cpsMean = v; }));
        r.add(slider("Max", 1, 30, 1, false,
                "Highest allowed click speed. Pulls Min and Mean down if needed.",
                () -> c.cpsMax, v -> { c.cpsMax = v; if (c.cpsMin > v) c.cpsMin = v; if (c.cpsMean > v) c.cpsMean = v; }));
        r.add(slider("Min/Max fallout", 0, 3, 2, false,
                "How far the speed may drift past Min/Max before being reeled back in.",
                () -> c.cpsMinMaxFallout, v -> c.cpsMinMaxFallout = v));

        r.add("Spike");
        r.add(slider("Chance", 0, 0.5, 1, true,
                "Chance per click of a short burst of extra speed.",
                () -> c.spikeChance, v -> c.spikeChance = v));
        r.add(slider("Min", 0, 10, 1, false,
                "Smallest speed boost a spike adds (in CPS).",
                () -> c.spikeMin, v -> { c.spikeMin = v; if (c.spikeMax < v) c.spikeMax = v; }));
        r.add(slider("Max", 0, 10, 1, false,
                "Largest speed boost a spike adds (in CPS).",
                () -> c.spikeMax, v -> { c.spikeMax = v; if (c.spikeMin > v) c.spikeMin = v; }));

        r.add("Stutter");
        r.add(slider("Chance", 0, 0.5, 1, true,
                "Chance per click of a short hitch that slows down.",
                () -> c.stutterChance, v -> c.stutterChance = v));
        r.add(slider("Min", 0, 15, 1, false,
                "Smallest speed drop a stutter causes (in CPS).",
                () -> c.stutterMin, v -> { c.stutterMin = v; if (c.stutterMax < v) c.stutterMax = v; }));
        r.add(slider("Max", 0, 15, 1, false,
                "Largest speed drop a stutter causes (in CPS).",
                () -> c.stutterMax, v -> { c.stutterMax = v; if (c.stutterMin > v) c.stutterMin = v; }));

        return r;
    }

    private List<Object> buildFatigueRows(Clicker c) {
        List<Object> r = new ArrayList<>();

        r.add("Hold (ms)");
        r.add(slider("Mean", 1, 200, 1, false,
                "Average hold time per click. Drag past Min or Max to widen the range.",
                () -> c.holdMsMean, v -> { c.holdMsMean = v; if (v < c.holdMsMin) c.holdMsMin = v; if (v > c.holdMsMax) c.holdMsMax = v; }));
        r.add(slider("Std deviation", 0, 30, 1, false,
                "How much the hold time randomly varies.",
                () -> c.holdMsStandardDeviation, v -> c.holdMsStandardDeviation = v));
        r.add(slider("Min", 1, 200, 1, false,
                "Shortest allowed hold time (ms).",
                () -> c.holdMsMin, v -> { c.holdMsMin = v; if (c.holdMsMax < v) c.holdMsMax = v; if (c.holdMsMean < v) c.holdMsMean = v; }));
        r.add(slider("Max", 1, 200, 1, false,
                "Longest allowed hold time (ms).",
                () -> c.holdMsMax, v -> { c.holdMsMax = v; if (c.holdMsMin > v) c.holdMsMin = v; if (c.holdMsMean > v) c.holdMsMean = v; }));

        r.add("Heavy hold");
        r.add(slider("Chance", 0, 0.2, 1, true,
                "Chance per click of an occasional extra-long hold.",
                () -> c.holdMsHeavyChance, v -> c.holdMsHeavyChance = v));
        r.add(slider("Min", 0, 80, 1, false,
                "Smallest extra time a heavy hold adds (ms).",
                () -> c.holdMsHeavyMin, v -> { c.holdMsHeavyMin = v; if (c.holdMsHeavyMax < v) c.holdMsHeavyMax = v; }));
        r.add(slider("Max", 0, 80, 1, false,
                "Largest extra time a heavy hold adds (ms).",
                () -> c.holdMsHeavyMax, v -> { c.holdMsHeavyMax = v; if (c.holdMsHeavyMin > v) c.holdMsHeavyMin = v; }));

        r.add("Rhythm");
        r.add(slider("Volatility", 0, 3, 2, false,
                "How quickly the click pace wanders over time.",
                () -> c.rhythmVolatility, v -> c.rhythmVolatility = v));
        r.add(slider("Tension", 0, 1, 3, false,
                "How strongly the pace is pulled back toward the Mean.\nHigher = steadier.",
                () -> c.rhythmTension, v -> c.rhythmTension = v));

        return r;
    }

    private List<Object> buildFiltersRows(Clicker c, boolean left) {
        List<Object> r = new ArrayList<>();

        r.add("Hotbar slots");
        r.add(slots("Slots", "Only click when the selected hotbar slot is enabled.", c.gates.slots));

        r.add("Held item");
        r.add(toggle("Weapons", "Allow while holding a sword or axe.",
                () -> c.gates.weapons, v -> c.gates.weapons = v));
        r.add(toggle("Tools", "Allow while holding a pickaxe, shovel, hoe or shears.",
                () -> c.gates.tools, v -> c.gates.tools = v));
        r.add(toggle("Blocks", "Allow while holding a placeable block.",
                () -> c.gates.blocks, v -> c.gates.blocks = v));
        r.add(toggle("Other", "Allow while holding anything else, or an empty hand.",
                () -> c.gates.other, v -> c.gates.other = v));

        r.add("Rules");
        // Block breaking is a left-click concept only.
        if (left)
            r.add(toggle("Break blocks", "Pause the auto-clicker while aimed at a reachable block.\nOnly really makes sense if 'Mouse' mode is set.",
                    () -> c.gates.allowBlockBreak, v -> c.gates.allowBlockBreak = v));
        r.add(toggle("In menus", "Allow clicking while a screen (inventory, chat) is open.",
                () -> c.gates.allowInMenu, v -> c.gates.allowInMenu = v));
        r.add(toggle("Pause on item use", "Pause while eating, drawing a bow or blocking.",
                () -> c.gates.pauseWhileUsingItem, v -> c.gates.pauseWhileUsingItem = v));

        r.add("Game mode");
        r.add(toggle("Survival", "Allow in survival mode.",
                () -> c.gates.survival, v -> c.gates.survival = v));
        r.add(toggle("Creative", "Allow in creative mode.",
                () -> c.gates.creative, v -> c.gates.creative = v));
        r.add(toggle("Adventure", "Allow in adventure mode.",
                () -> c.gates.adventure, v -> c.gates.adventure = v));

        return r;
    }

    private GuiButtonRow configRow(Clicker c) {
        return new GuiButtonRow("Config",
                button("Reset", "Restore this clicker's settings to their defaults.", c::resetParams),
                button("Export", "Copy this clicker's settings to the clipboard.", () -> exportClicker(c)),
                button("Import", "Load this clicker's settings from the clipboard.", () -> importClicker(c)));
    }

    private void exportClicker(Clicker c) {
        setClipboardString(c.export());
        Notice.show("Copied clicker settings to clipboard.");
    }

    private void importClicker(Clicker c) {
        if (c.importFrom(getClipboardString()))
            Notice.show("Loaded clicker settings from clipboard.");
        else
            Notice.show("Clipboard has no valid clicker config.");
    }

    private List<Object> buildGeneralRows() {
        List<Object> r = new ArrayList<>();

        String modeTip = "Toggle: press key to switch on/off.\n"
                + "Hold: clicks only while key is held.\n"
                + "Mouse: press key to arm, then hold the real mouse button to click.";

        String enabledTip = "Turn the clicker on or off. In Hold mode this follows your key;\n"
                + "in Mouse mode it arms/disarms the mouse-hold gate.";

        r.add("Left clicker");
        r.add(toggle("Enabled", enabledTip,
                () -> active(Clicker.LEFT, ConfigHandler.leftMode),
                v -> setActive(Clicker.LEFT, ConfigHandler.leftMode, v)));
        r.add(segment("Mode", modeTip,
                () -> ConfigHandler.leftMode.ordinal(),
                i -> { ConfigHandler.leftMode = ActivationMode.values()[i]; Clicker.LEFT.deactivate(); }));
        r.add(keybind("Key", "Left clicker key (toggle / hold / arm depending on mode).",
                () -> ConfigHandler.toggleLeftKey, v -> ConfigHandler.toggleLeftKey = v));
        r.add(configRow(Clicker.LEFT));

        r.add("Right clicker");
        r.add(toggle("Enabled", enabledTip,
                () -> active(Clicker.RIGHT, ConfigHandler.rightMode),
                v -> setActive(Clicker.RIGHT, ConfigHandler.rightMode, v)));
        r.add(segment("Mode", modeTip,
                () -> ConfigHandler.rightMode.ordinal(),
                i -> { ConfigHandler.rightMode = ActivationMode.values()[i]; Clicker.RIGHT.deactivate(); }));
        r.add(keybind("Key", "Right clicker key (toggle / hold / arm depending on mode).",
                () -> ConfigHandler.toggleRightKey, v -> ConfigHandler.toggleRightKey = v));
        r.add(configRow(Clicker.RIGHT));

        r.add("HUD");
        r.add(toggle("Enabled", "Show the on-screen HUD (configure it on the HUD tab).",
                () -> ConfigHandler.hudEnabled, v -> ConfigHandler.hudEnabled = v));

        r.add("Analytics");
        r.add(toggle("Enabled", "Record click data for stats and export (Analytics tab).",
                () -> Tracker.enabled, v -> Tracker.enabled = v));

        r.add("Menu");
        r.add(keybind("Open menu", "Key that opens this config screen.",
                () -> ConfigHandler.openGuiKey, v -> ConfigHandler.openGuiKey = v));

        return r;
    }

    private List<Object> buildHudRows() {
        List<Object> r = new ArrayList<>();

        r.add("Display");
        r.add(toggle("Left CPS", "Show the left clicker's clicks per second.",
                () -> ConfigHandler.hudCpsLeft, v -> ConfigHandler.hudCpsLeft = v));
        r.add(toggle("Right CPS", "Show the right clicker's clicks per second.",
                () -> ConfigHandler.hudCpsRight, v -> ConfigHandler.hudCpsRight = v));
        r.add(toggle("Clicker status", "Show each clicker's on/off state and mode.",
                () -> ConfigHandler.hudShowStatus, v -> ConfigHandler.hudShowStatus = v));

        r.add("Formatting");
        r.add(hexField("Text colour", false, () -> ConfigHandler.hudTextColor, v -> ConfigHandler.hudTextColor = v));
        r.add(toggle("Background", "Draw a box behind the HUD for readability.",
                () -> ConfigHandler.hudBackground, v -> ConfigHandler.hudBackground = v));
        r.add(cond(() -> ConfigHandler.hudBackground, hexField("Background colour", true, () -> ConfigHandler.hudBgColor, v -> ConfigHandler.hudBgColor = v)));
        r.add(cond(() -> ConfigHandler.hudBackground, slider("Padding", 0, 12, 0, false, "Space between the text and the box edge.",
                () -> ConfigHandler.hudPadding, v -> ConfigHandler.hudPadding = (int) v)));

        r.add("Position");
        r.add(anchorSegment());
        ScaledResolution sr = new ScaledResolution(mc);
        int sw = sr.getScaledWidth();
        int sh = sr.getScaledHeight();
        r.add(cond(GuiGhostTap::manualPos, slider("X", 0, sw, 0, false, "Horizontal position (pixels from the left).",
                () -> ConfigHandler.hudX, v -> ConfigHandler.hudX = (int) v)));
        r.add(cond(GuiGhostTap::manualPos, slider("Y", 0, sh, 0, false, "Vertical position (pixels from the top).",
                () -> ConfigHandler.hudY, v -> ConfigHandler.hudY = (int) v)));
        r.add(cond(() -> !manualPos(), slider("Edge gap", 0, 40, 0, false, "Distance from the screen edge when anchored to a corner.",
                () -> ConfigHandler.hudMargin, v -> ConfigHandler.hudMargin = (int) v)));

        r.add("Config");
        r.add(new GuiButtonRow("Config",
                button("Export", "Copy HUD settings to the clipboard.", this::exportHud),
                button("Import", "Load HUD settings from the clipboard.", this::importHud)));

        return r;
    }

    private void exportHud() {
        setClipboardString(ConfigHandler.exportHud());
        Notice.show("Copied HUD settings to clipboard.");
    }

    private void importHud() {
        if (ConfigHandler.importHud(getClipboardString()))
            Notice.show("Loaded HUD settings from clipboard.");
        else
            Notice.show("Clipboard has no valid HUD config.");
    }

    private static boolean manualPos() {
        return ConfigHandler.hudAnchor == HudAnchor.MANUAL;
    }

    private GuiSegment anchorSegment() {
        HudAnchor[] anchors = HudAnchor.values();
        String[] names = new String[anchors.length];
        for (int i = 0; i < anchors.length; i++)
            names[i] = anchors[i].label;

        GuiSegment s = new GuiSegment("Anchor", names,
                () -> ConfigHandler.hudAnchor.ordinal(),
                i -> ConfigHandler.hudAnchor = HudAnchor.values()[i]);
        s.tooltip = "Corner anchors stick to the screen edges on resize; Man = free X/Y.";
        return s;
    }

    private GuiHexField hexField(String label, boolean alpha, java.util.function.IntSupplier get, java.util.function.IntConsumer set) {
        GuiHexField f = new GuiHexField(label, alpha, get, set);
        f.tooltip = alpha ? "Hex colour #AARRGGBB (alpha first)." : "Hex colour #RRGGBB.";
        return f;
    }

    private List<Object> buildAnalyticsRows() {
        List<Object> r = new ArrayList<>();

        r.add("Left");
        r.add(stat("Recorded", "Left clicks recorded this session.",
                () -> String.valueOf(Clicker.LEFT.tracker.size())));
        r.add(stat("Average CPS", "Average recorded left click speed.",
                () -> String.format("%.2f", Clicker.LEFT.tracker.getAverageCps())));

        r.add("Right");
        r.add(stat("Recorded", "Right clicks recorded this session.",
                () -> String.valueOf(Clicker.RIGHT.tracker.size())));
        r.add(stat("Average CPS", "Average recorded right click speed.",
                () -> String.format("%.2f", Clicker.RIGHT.tracker.getAverageCps())));

        r.add("Actions");
        r.add(button("Export CSV", "Save recorded data to a CSV file in the config folder.",
                Analytics::export));
        r.add(button("Clear data", "Delete all recorded data.", Analytics::clear));

        return r;
    }


    // The "Enabled" toggle targets the arm gate in Mouse mode (so you can drop
    // back to a normal mouse) and the plain on/off state otherwise.
    private static boolean active(Clicker c, ActivationMode mode) {
        return mode == ActivationMode.MOUSE ? c.armed : c.isEnabled();
    }

    private static void setActive(Clicker c, ActivationMode mode, boolean value) {
        if (mode == ActivationMode.MOUSE)
            c.armed = value;
        else
            c.setEnabled(value);
    }


    // --- Row builder helpers (attach tooltips without cluttering call sites) ---

    private GuiSlider slider(String label, double min, double max, int dec, boolean pct, String tip, DoubleSupplier get, DoubleConsumer set) {
        GuiSlider s = new GuiSlider(label, min, max, dec, pct, get, set);
        s.tooltip = tip;
        return s;
    }

    private GuiToggle toggle(String label, String tip, java.util.function.BooleanSupplier get, java.util.function.Consumer<Boolean> set) {
        GuiToggle t = new GuiToggle(label, get, set);
        t.tooltip = tip;
        return t;
    }

    private GuiKeybind keybind(String label, String tip, java.util.function.IntSupplier get, java.util.function.IntConsumer set) {
        GuiKeybind k = new GuiKeybind(label, get, set);
        k.tooltip = tip;
        return k;
    }

    private GuiSegment segment(String label, String tip, java.util.function.IntSupplier get, java.util.function.IntConsumer set) {
        ActivationMode[] modes = ActivationMode.values();
        String[] names = new String[modes.length];
        for (int i = 0; i < modes.length; i++)
            names[i] = modes[i].label;

        GuiSegment s = new GuiSegment(label, names, get, set);
        s.tooltip = tip;
        return s;
    }

    private GuiStat stat(String label, String tip, java.util.function.Supplier<String> value) {
        GuiStat s = new GuiStat(label, value);
        s.tooltip = tip;
        return s;
    }

    private GuiActionButton button(String label, String tip, Runnable action) {
        GuiActionButton b = new GuiActionButton(label, action);
        b.tooltip = tip;
        return b;
    }

    private GuiSlots slots(String label, String tip, boolean[] arr) {
        GuiSlots s = new GuiSlots(label, arr);
        s.tooltip = tip;
        return s;
    }


    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        drawDefaultBackground();
        hoverTooltip = null;

        // Panel background
        drawRect(left, top, left + PANEL_W, top + PANEL_H, 0xF0121212);

        // Title bar
        drawRect(left, top, left + PANEL_W, top + TITLE_H, 0xFF1B1B1B);
        fontRendererObj.drawString("GhostTap", left + 8, top + 5, 0xFFFFFFFF);
        boolean closeHover = mouseX >= left + PANEL_W - 16 && mouseX <= left + PANEL_W - 4 && mouseY >= top + 4 && mouseY <= top + 14;
        fontRendererObj.drawString("x", left + PANEL_W - 12, top + 5, closeHover ? 0xFFFF5555 : 0xFFB8B8B8);

        drawTabs(mouseX, mouseY);

        // Clip content to the scroll region and draw it
        ScaledResolution sr = new ScaledResolution(mc);
        int sf = sr.getScaleFactor();
        GL11.glEnable(GL11.GL_SCISSOR_TEST);
        GL11.glScissor(contentX * sf, mc.displayHeight - contentBottom * sf, contentW * sf, contentH * sf);

        if (activeSlider != null)
            activeSlider.mouseDragged(mouseX);
        walk(currentRows(), true, mouseX, mouseY);

        GL11.glDisable(GL11.GL_SCISSOR_TEST);

        super.drawScreen(mouseX, mouseY, partialTicks);

        // Border last so it frames the whole window cleanly, on top of the title
        // bar, tabs and content.
        drawBorder(left, top, left + PANEL_W, top + PANEL_H, 0xFF5A9BD4);

        String notice = Notice.current();
        if (notice != null) {
            int w = fontRendererObj.getStringWidth(notice);
            int nx = left + (PANEL_W - w) / 2;
            int ny = top + PANEL_H + 6;
            drawRect(nx - 5, ny - 4, nx + w + 5, ny + 10, 0xF0121212);
            drawBorder(nx - 5, ny - 4, nx + w + 5, ny + 10, 0xFF5A9BD4);
            fontRendererObj.drawStringWithShadow(notice, nx, ny, 0xFFFFFFFF);
        }

        // Tooltip on top of everything. Suppressed while actively dragging or
        // rebinding so it doesn't get in the way.
        if (hoverTooltip != null && activeSlider == null && activeKeybind == null)
            drawHoveringText(Arrays.asList(hoverTooltip.split("\n")), mouseX, mouseY);
    }

    // Hover only the title text of a labelled widget, so the tooltip shows when
    // pointing at the name rather than anywhere on the row.
    private void captureHoverTitle(int mouseX, int mouseY, int titleTop, String label, String tip) {
        int titleWidth = fontRendererObj.getStringWidth(label);
        captureHover(mouseX, mouseY, contentX, titleTop, titleWidth, 9, tip);
    }

    // Records a tooltip if the mouse is inside the given rect, clipped to the
    // visible content region. Drawn later, after the scissor is disabled.
    private void captureHover(int mouseX, int mouseY, int rx, int ry, int rw, int rh, String tip) {
        if (tip == null || tip.isEmpty())
            return;
        if (mouseY < contentTop || mouseY > contentBottom)
            return;
        if (mouseX >= rx && mouseX <= rx + rw && mouseY >= ry && mouseY <= ry + rh)
            hoverTooltip = tip;
    }

    private void drawTabs(int mouseX, int mouseY) {
        int tabY = top + TITLE_H;
        int tabW = PANEL_W / tabs.size();

        for (int i = 0; i < tabs.size(); i++) {
            int tx = left + i * tabW;
            boolean active = i == activeTab;
            boolean hover = mouseX >= tx && mouseX <= tx + tabW && mouseY >= tabY && mouseY <= tabY + TAB_H;

            int bg = active ? 0xFF2A2A2A : (hover ? 0xFF202020 : 0xFF161616);
            drawRect(tx, tabY, tx + tabW, tabY + TAB_H, bg);
            if (active)
                drawRect(tx, tabY + TAB_H - 2, tx + tabW, tabY + TAB_H, 0xFF5A9BD4);

            int color = active ? 0xFFFFFFFF : 0xFFA0A0A0;
            int textX = tx + (tabW - fontRendererObj.getStringWidth(tabs.get(i).name)) / 2;
            fontRendererObj.drawString(tabs.get(i).name, textX, tabY + 4, color);
        }

        if (!hasSubTabs())
            return;

        List<SubTab> subs = tabs.get(activeTab).subs;
        int subY = tabY + TAB_H;
        int subW = PANEL_W / subs.size();

        for (int i = 0; i < subs.size(); i++) {
            int sx = left + i * subW;
            boolean active = i == activeSub;
            boolean hover = mouseX >= sx && mouseX <= sx + subW && mouseY >= subY && mouseY <= subY + SUBTAB_H;

            int bg = active ? 0xFF202020 : (hover ? 0xFF1A1A1A : 0xFF121212);
            drawRect(sx, subY, sx + subW, subY + SUBTAB_H, bg);
            if (active)
                drawRect(sx, subY + SUBTAB_H - 1, sx + subW, subY + SUBTAB_H, 0xFF5A9BD4);

            int color = active ? 0xFFDDDDDD : 0xFF888888;
            int textX = sx + (subW - fontRendererObj.getStringWidth(subs.get(i).name)) / 2;
            fontRendererObj.drawString(subs.get(i).name, textX, subY + 3, color);
        }
    }

    // Places every row (sets widget coords) and optionally draws. Returns the
    // total content height so scroll can be clamped.
    private int walk(List<Object> rows, boolean draw, int mouseX, int mouseY) {
        int cursor = contentTop - scroll;
        int start = cursor;

        for (Object entry : rows) {
            Object row = entry;
            if (row instanceof Cond) {
                Cond c = (Cond) row;
                if (!c.show.getAsBoolean())
                    continue;
                row = c.row;
            }

            if (row instanceof String) {
                if (draw && cursor + SECTION_H >= contentTop && cursor <= contentBottom) {
                    fontRendererObj.drawString((String) row, contentX, cursor + 6, 0xFF5A9BD4);
                    drawRect(contentX, cursor + 16, contentX + contentW, cursor + 17, 0xFF333333);
                }
                cursor += SECTION_H;
            } else if (row instanceof GuiSlider) {
                GuiSlider s = (GuiSlider) row;
                s.x = contentX;
                s.y = cursor;
                s.width = contentW;
                if (draw) {
                    s.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor, s.label, s.tooltip);
                }
                cursor += GuiSlider.ROW_HEIGHT;
            } else if (row instanceof GuiToggle) {
                GuiToggle t = (GuiToggle) row;
                t.x = contentX;
                t.y = cursor;
                t.width = contentW;
                if (draw) {
                    t.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor + 3, t.label, t.tooltip);
                }
                cursor += GuiToggle.ROW_HEIGHT;
            } else if (row instanceof GuiKeybind) {
                GuiKeybind k = (GuiKeybind) row;
                k.x = contentX;
                k.y = cursor;
                k.width = contentW;
                if (draw) {
                    k.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor + 3, k.label, k.tooltip);
                }
                cursor += GuiKeybind.ROW_HEIGHT;
            } else if (row instanceof GuiSegment) {
                GuiSegment sg = (GuiSegment) row;
                sg.x = contentX;
                sg.y = cursor;
                sg.width = contentW;
                if (draw) {
                    sg.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor + 3, sg.label, sg.tooltip);
                }
                cursor += GuiSegment.ROW_HEIGHT;
            } else if (row instanceof GuiHexField) {
                GuiHexField f = (GuiHexField) row;
                f.x = contentX;
                f.y = cursor;
                f.width = contentW;
                if (draw) {
                    f.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor + 3, f.label, f.tooltip);
                }
                cursor += GuiHexField.ROW_HEIGHT;
            } else if (row instanceof GuiSlots) {
                GuiSlots sl = (GuiSlots) row;
                sl.x = contentX;
                sl.y = cursor;
                sl.width = contentW;
                if (draw) {
                    sl.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor + 3, sl.label, sl.tooltip);
                }
                cursor += GuiSlots.ROW_HEIGHT;
            } else if (row instanceof GuiStat) {
                GuiStat s = (GuiStat) row;
                s.x = contentX;
                s.y = cursor;
                s.width = contentW;
                if (draw) {
                    s.draw(fontRendererObj, mouseX, mouseY);
                    captureHoverTitle(mouseX, mouseY, cursor, s.label, s.tooltip);
                }
                cursor += GuiStat.ROW_HEIGHT;
            } else if (row instanceof GuiActionButton) {
                GuiActionButton b = (GuiActionButton) row;
                b.x = contentX;
                b.y = cursor;
                b.width = contentW;
                if (draw) {
                    b.draw(fontRendererObj, mouseX, mouseY);
                    // Buttons have centered text and no left title, so hover the
                    // whole button.
                    captureHover(mouseX, mouseY, contentX, cursor, contentW, 16, b.tooltip);
                }
                cursor += GuiActionButton.ROW_HEIGHT;
            } else if (row instanceof GuiButtonRow) {
                GuiButtonRow br = (GuiButtonRow) row;
                br.x = contentX;
                br.y = cursor;
                br.width = contentW;
                if (draw) {
                    br.draw(fontRendererObj, mouseX, mouseY);
                    if (br.label != null)
                        captureHoverTitle(mouseX, mouseY, cursor + 3, br.label, br.tooltip);
                    for (GuiActionButton b : br.buttons)
                        captureHover(mouseX, mouseY, b.x, cursor, b.width, 16, b.tooltip);
                }
                cursor += GuiButtonRow.ROW_HEIGHT;
            }
        }

        return cursor - start;
    }

    private List<Object> currentRows() {
        return tabs.get(activeTab).subs.get(activeSub).rows;
    }


    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws java.io.IOException {
        // Binding a mouse button: middle / side buttons only (left & right stay
        // free for the UI and are meaningless as clicker hotkeys anyway).
        if (activeKeybind != null && mouseButton >= 2) {
            activeKeybind.mousePressed(mouseButton);
            activeKeybind = null;
            return;
        }

        super.mouseClicked(mouseX, mouseY, mouseButton);
        if (mouseButton != 0)
            return;

        // Close button
        if (mouseX >= left + PANEL_W - 16 && mouseX <= left + PANEL_W - 4 && mouseY >= top + 4 && mouseY <= top + 14) {
            close();
            return;
        }

        boolean inPanelX = mouseX >= left && mouseX <= left + PANEL_W;

        // Main tabs
        int tabY = top + TITLE_H;
        if (inPanelX && mouseY >= tabY && mouseY <= tabY + TAB_H) {
            int i = (mouseX - left) / (PANEL_W / tabs.size());
            if (i >= 0 && i < tabs.size()) {
                switchTab(i);
                return;
            }
        }

        // Sub tabs
        if (hasSubTabs()) {
            List<SubTab> subs = tabs.get(activeTab).subs;
            int subY = tabY + TAB_H;
            if (inPanelX && mouseY >= subY && mouseY <= subY + SUBTAB_H) {
                int i = (mouseX - left) / (PANEL_W / subs.size());
                if (i >= 0 && i < subs.size()) {
                    switchSub(i);
                    return;
                }
            }
        }

        // Content widgets — only inside the visible clip region
        if (mouseY < contentTop || mouseY > contentBottom)
            return;

        clearListening();
        clearFocus();
        for (Object entry : currentRows()) {
            Object row = entry;
            if (row instanceof Cond) {
                Cond c = (Cond) row;
                if (!c.show.getAsBoolean())
                    continue;
                row = c.row;
            }

            if (row instanceof GuiSlider) {
                GuiSlider s = (GuiSlider) row;
                if (s.mouseClicked(mouseX, mouseY)) {
                    activeSlider = s;
                    return;
                }
            } else if (row instanceof GuiHexField) {
                GuiHexField f = (GuiHexField) row;
                if (f.mouseClicked(mouseX, mouseY)) {
                    activeField = f;
                    return;
                }
            } else if (row instanceof GuiToggle) {
                if (((GuiToggle) row).mouseClicked(mouseX, mouseY))
                    return;
            } else if (row instanceof GuiKeybind) {
                GuiKeybind k = (GuiKeybind) row;
                if (k.mouseClicked(mouseX, mouseY)) {
                    activeKeybind = k;
                    return;
                }
            } else if (row instanceof GuiSegment) {
                if (((GuiSegment) row).mouseClicked(mouseX, mouseY))
                    return;
            } else if (row instanceof GuiSlots) {
                if (((GuiSlots) row).mouseClicked(mouseX, mouseY))
                    return;
            } else if (row instanceof GuiActionButton) {
                if (((GuiActionButton) row).mouseClicked(mouseX, mouseY))
                    return;
            } else if (row instanceof GuiButtonRow) {
                if (((GuiButtonRow) row).mouseClicked(mouseX, mouseY))
                    return;
            }
        }
    }

    @Override
    protected void mouseReleased(int mouseX, int mouseY, int state) {
        super.mouseReleased(mouseX, mouseY, state);
        if (activeSlider != null) {
            activeSlider.mouseReleased();
            activeSlider = null;
        }
    }

    @Override
    public void handleMouseInput() throws java.io.IOException {
        super.handleMouseInput();

        int wheel = Mouse.getEventDWheel();
        if (wheel == 0)
            return;

        scroll -= Integer.signum(wheel) * 14;
        clampScroll();
    }

    private void clampScroll() {
        int total = walk(currentRows(), false, 0, 0);
        // Extra padding so the last row can clear the bottom edge instead of
        // sitting flush against it.
        int max = Math.max(0, total - contentH + BOTTOM_PAD);
        scroll = Math.max(0, Math.min(max, scroll));
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) throws java.io.IOException {
        // Capturing a new keybind takes priority over everything else.
        if (activeKeybind != null) {
            activeKeybind.keyPressed(keyCode);
            activeKeybind = null;
            return;
        }

        // Typing into a hex field consumes keys (Escape just defocuses).
        if (activeField != null) {
            if (keyCode == Keyboard.KEY_ESCAPE)
                clearFocus();
            else
                activeField.keyTyped(typedChar, keyCode);
            return;
        }

        if (keyCode == Keyboard.KEY_ESCAPE || keyCode == ConfigHandler.openGuiKey) {
            close();
            return;
        }
    }

    private void clearFocus() {
        if (activeField != null) {
            activeField.focused = false;
            activeField = null;
        }
    }

    private void close() {
        mc.displayGuiScreen(parent);
    }

    @Override
    public void onGuiClosed() {
        ConfigHandler.saveConfig();
    }

    @Override
    public boolean doesGuiPauseGame() {
        return false;
    }


    private void switchTab(int i) {
        activeTab = i;
        activeSub = 0;
        scroll = 0;
        clearListening();
        clearFocus();
        activeSlider = null;
        layoutContent();
    }

    private void switchSub(int i) {
        activeSub = i;
        scroll = 0;
        clearListening();
        clearFocus();
        activeSlider = null;
    }

    private void clearListening() {
        if (activeKeybind != null) {
            activeKeybind.listening = false;
            activeKeybind = null;
        }
    }

    private void drawBorder(int x1, int y1, int x2, int y2, int color) {
        drawRect(x1, y1, x2, y1 + 1, color);
        drawRect(x1, y2 - 1, x2, y2, color);
        drawRect(x1, y1, x1 + 1, y2, color);
        drawRect(x2 - 1, y1, x2, y2, color);
    }
}
