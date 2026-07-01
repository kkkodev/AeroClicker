package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

// Single-value horizontal slider. Reads and writes the backing value live
// through the getter/setter (no cached copy). The knob rides a fixed absolute
// scale [absMin, absMax], but the value is confined to the live clamp bounds
// [clampLo, clampHi] — so e.g. the Mean knob physically stops at the current
// Min/Max positions instead of ranging the whole track.
public class GuiSlider {

    public static final int ROW_HEIGHT = 24;

    final String label;
    private final double absMin;
    private final double absMax;
    private final int decimals;
    private final boolean percent;
    private final DoubleSupplier getter;
    private final DoubleConsumer setter;
    private final DoubleSupplier clampLo;
    private final DoubleSupplier clampHi;

    public String tooltip;
    public boolean dragging;
    public int x, y, width;

    public GuiSlider(String label, double min, double max, int decimals, boolean percent, DoubleSupplier getter, DoubleConsumer setter) {
        this(label, min, max, decimals, percent, getter, setter, () -> min, () -> max);
    }

    public GuiSlider(String label, double min, double max, int decimals, boolean percent, DoubleSupplier getter, DoubleConsumer setter, DoubleSupplier clampLo, DoubleSupplier clampHi) {
        this.label = label;
        this.absMin = min;
        this.absMax = max;
        this.decimals = decimals;
        this.percent = percent;
        this.getter = getter;
        this.setter = setter;
        this.clampLo = clampLo;
        this.clampHi = clampHi;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        double value = clamp(getter.getAsDouble(), lo(), hi());

        int trackTop = y + 13;
        int trackBottom = y + 17;

        fr.drawString(label, x, y, 0xFFB8B8B8);
        String text = format(value);
        fr.drawString(text, x + width - fr.getStringWidth(text), y, 0xFFFFFFFF);

        Gui.drawRect(x, trackTop, x + width, trackBottom, 0xFF2A2A2A);

        double t = (value - absMin) / (absMax - absMin);
        int knobX = x + (int) Math.round(clamp(t, 0, 1) * width);

        Gui.drawRect(x, trackTop, knobX, trackBottom, 0xFF5A9BD4);

        boolean hover = dragging || contains(mouseX, mouseY);
        Gui.drawRect(knobX - 2, y + 10, knobX + 3, y + 20, hover ? 0xFFFFFFFF : 0xFFD0D0D0);
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        if (!contains(mouseX, mouseY))
            return false;

        dragging = true;
        updateFromMouse(mouseX);
        return true;
    }

    public void mouseDragged(int mouseX) {
        if (dragging)
            updateFromMouse(mouseX);
    }

    public void mouseReleased() {
        dragging = false;
    }

    private void updateFromMouse(int mouseX) {
        double t = (mouseX - x) / (double) width;
        double raw = absMin + t * (absMax - absMin);
        setter.accept(clamp(round(raw), lo(), hi()));
    }

    // Effective bounds: absolute scale intersected with the live clamp bounds.
    private double lo() {
        return Math.max(absMin, clampLo.getAsDouble());
    }

    private double hi() {
        return Math.min(absMax, clampHi.getAsDouble());
    }

    private boolean contains(int mouseX, int mouseY) {
        return mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 22;
    }

    private double clamp(double v, double lo, double hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    private double round(double v) {
        double factor = Math.pow(10, percent ? decimals + 2 : decimals);
        return Math.round(v * factor) / factor;
    }

    private String format(double value) {
        if (percent)
            return String.format("%." + decimals + "f%%", value * 100);
        return String.format("%." + decimals + "f", value);
    }
}
