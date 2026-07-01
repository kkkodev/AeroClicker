package com.icysnex.ghosttap.gui;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.analytics.Analytics;
import com.icysnex.ghosttap.core.analytics.Tracker;
import com.icysnex.ghosttap.utils.Chat;
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
    private static final int SECTION_H = 20;
    private static final int BOTTOM_PAD = 8;

    private static final String[] TABS = {"Left", "Right", "General", "Analytics"};

    private final List<List<Object>> tabRows = new ArrayList<>();
    private int activeTab = 0;
    private int scroll = 0;

    private GuiSlider activeSlider;
    private GuiKeybind activeKeybind;
    private String hoverTooltip;

    // Panel geometry, filled in initGui.
    private int left, top;
    private int contentX, contentW, contentTop, contentBottom, contentH;

    @Override
    public void initGui() {
        left = (width - PANEL_W) / 2;
        top = (height - PANEL_H) / 2;

        contentX = left + 12;
        contentW = PANEL_W - 24;
        contentTop = top + TITLE_H + TAB_H + 4;
        // Viewport reaches the bottom edge (no fixed margin); the breathing gap is
        // added only as trailing scroll room (BOTTOM_PAD), so it shows when
        // scrolled to the end without shrinking the usable area.
        contentBottom = top + PANEL_H;
        contentH = contentBottom - contentTop;

        tabRows.clear();
        tabRows.add(buildClickerRows(Clicker.LEFT));
        tabRows.add(buildClickerRows(Clicker.RIGHT));
        tabRows.add(buildGeneralRows());
        tabRows.add(buildAnalyticsRows());
    }

    private List<Object> buildClickerRows(Clicker c) {
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

        r.add("Reset");
        r.add(button("Reset to defaults", "Restore this clicker's settings to their defaults.",
                c::resetParams));

        return r;
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

        r.add("Right clicker");
        r.add(toggle("Enabled", enabledTip,
                () -> active(Clicker.RIGHT, ConfigHandler.rightMode),
                v -> setActive(Clicker.RIGHT, ConfigHandler.rightMode, v)));
        r.add(segment("Mode", modeTip,
                () -> ConfigHandler.rightMode.ordinal(),
                i -> { ConfigHandler.rightMode = ActivationMode.values()[i]; Clicker.RIGHT.deactivate(); }));
        r.add(keybind("Key", "Right clicker key (toggle / hold / arm depending on mode).",
                () -> ConfigHandler.toggleRightKey, v -> ConfigHandler.toggleRightKey = v));

        r.add("Menu");
        r.add(keybind("Open menu", "Key that opens this config screen.",
                () -> ConfigHandler.openGuiKey, v -> ConfigHandler.openGuiKey = v));

        r.add("Config");
        r.add(button("Export to clipboard", "Copy all settings to the clipboard to share.",
                this::exportConfig));
        r.add(button("Import from clipboard", "Load settings from a clipboard config.",
                this::importConfig));

        return r;
    }

    private void exportConfig() {
        String data = ConfigHandler.exportString();
        if (data != null) {
            setClipboardString(data);
            Chat.message(mc.thePlayer, "Config", "Copied settings to clipboard.");
        } else {
            Chat.error(mc.thePlayer, "Config", "Export failed. Check console.");
        }
    }

    private void importConfig() {
        if (ConfigHandler.importString(getClipboardString()))
            Chat.message(mc.thePlayer, "Config", "Loaded settings from clipboard.");
        else
            Chat.error(mc.thePlayer, "Config", "Clipboard has no valid config.");
    }

    private List<Object> buildAnalyticsRows() {
        List<Object> r = new ArrayList<>();

        r.add("Tracking");
        r.add(toggle("Enabled", "Record click timing data for stats and CSV export.",
                () -> Tracker.enabled, v -> Tracker.enabled = v));

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
                () -> Analytics.export(mc.thePlayer)));
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
        int tabW = PANEL_W / TABS.length;
        int tabY = top + TITLE_H;

        for (int i = 0; i < TABS.length; i++) {
            int tx = left + i * tabW;
            boolean active = i == activeTab;
            boolean hover = mouseX >= tx && mouseX <= tx + tabW && mouseY >= tabY && mouseY <= tabY + TAB_H;

            int bg = active ? 0xFF2A2A2A : (hover ? 0xFF202020 : 0xFF161616);
            drawRect(tx, tabY, tx + tabW, tabY + TAB_H, bg);
            if (active)
                drawRect(tx, tabY + TAB_H - 2, tx + tabW, tabY + TAB_H, 0xFF5A9BD4);

            int color = active ? 0xFFFFFFFF : 0xFFA0A0A0;
            int textX = tx + (tabW - fontRendererObj.getStringWidth(TABS[i])) / 2;
            fontRendererObj.drawString(TABS[i], textX, tabY + 4, color);
        }
    }

    // Places every row (sets widget coords) and optionally draws. Returns the
    // total content height so scroll can be clamped.
    private int walk(List<Object> rows, boolean draw, int mouseX, int mouseY) {
        int cursor = contentTop - scroll;
        int start = cursor;

        for (Object row : rows) {
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
            }
        }

        return cursor - start;
    }

    private List<Object> currentRows() {
        return tabRows.get(activeTab);
    }


    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws java.io.IOException {
        super.mouseClicked(mouseX, mouseY, mouseButton);
        if (mouseButton != 0)
            return;

        // Close button
        if (mouseX >= left + PANEL_W - 16 && mouseX <= left + PANEL_W - 4 && mouseY >= top + 4 && mouseY <= top + 14) {
            mc.displayGuiScreen(null);
            return;
        }

        // Tabs
        int tabW = PANEL_W / TABS.length;
        int tabY = top + TITLE_H;
        if (mouseY >= tabY && mouseY <= tabY + TAB_H) {
            int i = (mouseX - left) / tabW;
            if (i >= 0 && i < TABS.length && mouseX >= left && mouseX <= left + PANEL_W) {
                switchTab(i);
                return;
            }
        }

        // Content widgets — only inside the visible clip region
        if (mouseY < contentTop || mouseY > contentBottom)
            return;

        clearListening();
        for (Object row : currentRows()) {
            if (row instanceof GuiSlider) {
                GuiSlider s = (GuiSlider) row;
                if (s.mouseClicked(mouseX, mouseY)) {
                    activeSlider = s;
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
            } else if (row instanceof GuiActionButton) {
                if (((GuiActionButton) row).mouseClicked(mouseX, mouseY))
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

        if (keyCode == Keyboard.KEY_ESCAPE || keyCode == ConfigHandler.openGuiKey) {
            mc.displayGuiScreen(null);
            return;
        }
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
        scroll = 0;
        clearListening();
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
