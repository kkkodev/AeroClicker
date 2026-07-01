package com.icysnex.ghosttap.gui;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.analytics.Analytics;
import com.icysnex.ghosttap.core.analytics.Tracker;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.ScaledResolution;
import org.lwjgl.input.Keyboard;
import org.lwjgl.input.Mouse;
import org.lwjgl.opengl.GL11;

import java.util.ArrayList;
import java.util.List;

// Custom dark-panel config screen. Three tabs (Left / Right / General), each a
// scrollable list of section headers + widgets. Widget coords are recomputed
// every frame in walk(), so the mouse handlers can reuse the same layout.
public class GuiGhostTap extends GuiScreen {

    private static final int PANEL_W = 250;
    private static final int PANEL_H = 210;
    private static final int TITLE_H = 18;
    private static final int TAB_H = 16;
    private static final int SECTION_H = 20;

    private static final String[] TABS = {"Left", "Right", "General", "Analytics"};

    private final List<List<Object>> tabRows = new ArrayList<>();
    private int activeTab = 0;
    private int scroll = 0;

    private GuiSlider activeSlider;
    private GuiKeybind activeKeybind;

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
        contentBottom = top + PANEL_H - 16;
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
        r.add(new GuiSlider("Mean", 1, 25, 1, c.cpsMean, v -> c.cpsMean = v));
        r.add(new GuiSlider("Std deviation", 0, 6, 2, c.cpsStandardDeviation, v -> c.cpsStandardDeviation = v));
        r.add(new GuiSlider("Min", 1, 25, 1, c.cpsMin, v -> c.cpsMin = v));
        r.add(new GuiSlider("Max", 1, 30, 1, c.cpsMax, v -> c.cpsMax = v));
        r.add(new GuiSlider("Min/Max fallout", 0, 3, 2, c.cpsMinMaxFallout, v -> c.cpsMinMaxFallout = v));

        r.add("Spike");
        r.add(new GuiSlider("Chance", 0, 0.5, 3, c.spikeChance, v -> c.spikeChance = v));
        r.add(new GuiSlider("Min", 0, 10, 1, c.spikeMin, v -> c.spikeMin = v));
        r.add(new GuiSlider("Max", 0, 10, 1, c.spikeMax, v -> c.spikeMax = v));

        r.add("Stutter");
        r.add(new GuiSlider("Chance", 0, 0.5, 3, c.stutterChance, v -> c.stutterChance = v));
        r.add(new GuiSlider("Min", 0, 15, 1, c.stutterMin, v -> c.stutterMin = v));
        r.add(new GuiSlider("Max", 0, 15, 1, c.stutterMax, v -> c.stutterMax = v));

        r.add("Hold (ms)");
        r.add(new GuiSlider("Mean", 5, 150, 1, c.holdMsMean, v -> c.holdMsMean = v));
        r.add(new GuiSlider("Std deviation", 0, 30, 1, c.holdMsStandardDeviation, v -> c.holdMsStandardDeviation = v));
        r.add(new GuiSlider("Min", 1, 150, 1, c.holdMsMin, v -> c.holdMsMin = v));
        r.add(new GuiSlider("Max", 1, 200, 1, c.holdMsMax, v -> c.holdMsMax = v));

        r.add("Heavy hold");
        r.add(new GuiSlider("Chance", 0, 0.2, 3, c.holdMsHeavyChance, v -> c.holdMsHeavyChance = v));
        r.add(new GuiSlider("Min", 0, 60, 1, c.holdMsHeavyMin, v -> c.holdMsHeavyMin = v));
        r.add(new GuiSlider("Max", 0, 80, 1, c.holdMsHeavyMax, v -> c.holdMsHeavyMax = v));

        r.add("Rhythm");
        r.add(new GuiSlider("Volatility", 0, 3, 2, c.rhythmVolatility, v -> c.rhythmVolatility = v));
        r.add(new GuiSlider("Tension", 0, 1, 3, c.rhythmTension, v -> c.rhythmTension = v));

        return r;
    }

    private List<Object> buildGeneralRows() {
        List<Object> r = new ArrayList<>();

        r.add("Keybinds");
        r.add(new GuiKeybind("Open menu", () -> ConfigHandler.openGuiKey, v -> ConfigHandler.openGuiKey = v));
        r.add(new GuiKeybind("Toggle left", () -> ConfigHandler.toggleLeftKey, v -> ConfigHandler.toggleLeftKey = v));
        r.add(new GuiKeybind("Toggle right", () -> ConfigHandler.toggleRightKey, v -> ConfigHandler.toggleRightKey = v));

        return r;
    }

    private List<Object> buildAnalyticsRows() {
        List<Object> r = new ArrayList<>();

        r.add("Tracking");
        r.add(new GuiToggle("Enabled", () -> Tracker.enabled, v -> Tracker.enabled = v));

        r.add("Left");
        r.add(new GuiStat("Current CPS", () -> String.format("%.0f", Clicker.LEFT.tracker.getCurrentCps())));
        r.add(new GuiStat("Recorded", () -> String.valueOf(Clicker.LEFT.tracker.size())));
        r.add(new GuiStat("Average CPS", () -> String.format("%.2f", Clicker.LEFT.tracker.getAverageCps())));

        r.add("Right");
        r.add(new GuiStat("Current CPS", () -> String.format("%.0f", Clicker.RIGHT.tracker.getCurrentCps())));
        r.add(new GuiStat("Recorded", () -> String.valueOf(Clicker.RIGHT.tracker.size())));
        r.add(new GuiStat("Average CPS", () -> String.format("%.2f", Clicker.RIGHT.tracker.getAverageCps())));

        r.add("Actions");
        r.add(new GuiActionButton("Export CSV", () -> Analytics.export(mc.thePlayer)));
        r.add(new GuiActionButton("Clear data", Analytics::clear));

        return r;
    }


    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        drawDefaultBackground();

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
                if (draw) s.draw(fontRendererObj, mouseX, mouseY);
                cursor += GuiSlider.ROW_HEIGHT;
            } else if (row instanceof GuiToggle) {
                GuiToggle t = (GuiToggle) row;
                t.x = contentX;
                t.y = cursor;
                t.width = contentW;
                if (draw) t.draw(fontRendererObj, mouseX, mouseY);
                cursor += GuiToggle.ROW_HEIGHT;
            } else if (row instanceof GuiKeybind) {
                GuiKeybind k = (GuiKeybind) row;
                k.x = contentX;
                k.y = cursor;
                k.width = contentW;
                if (draw) k.draw(fontRendererObj, mouseX, mouseY);
                cursor += GuiKeybind.ROW_HEIGHT;
            } else if (row instanceof GuiStat) {
                GuiStat s = (GuiStat) row;
                s.x = contentX;
                s.y = cursor;
                s.width = contentW;
                if (draw) s.draw(fontRendererObj, mouseX, mouseY);
                cursor += GuiStat.ROW_HEIGHT;
            } else if (row instanceof GuiActionButton) {
                GuiActionButton b = (GuiActionButton) row;
                b.x = contentX;
                b.y = cursor;
                b.width = contentW;
                if (draw) b.draw(fontRendererObj, mouseX, mouseY);
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
        int max = Math.max(0, total - contentH);
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
