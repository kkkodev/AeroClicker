package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

import java.util.function.DoubleConsumer;
import java.util.function.DoubleSupplier;

// Single-value horizontal slider. Reads and writes the backing value live
// through the supplied getter/setter (no cached copy), so external clamps stay
// reflected. Bounds are suppliers too, letting one slider constrain itself to
// another value (e.g. Mean bounded by the live Min/Max).
public class GuiSlider {

    public static final int ROW_HEIGHT = 24;

    private final String label;
    private final DoubleSupplier min;
    private final DoubleSupplier max;
    private final int decimals;
    private final boolean percent;
    private final DoubleSupplier getter;
    private final DoubleConsumer setter;

    public String tooltip;
    public boolean dragging;
    public int x, y, width;

    public GuiSlider(String label, double min, double max, int decimals, boolean percent, DoubleSupplier getter, DoubleConsumer setter) {
        this(label, () -> min, () -> max, decimals, percent, getter, setter);
    }

    public GuiSlider(String label, DoubleSupplier min, DoubleSupplier max, int decimals, boolean percent, DoubleSupplier getter, DoubleConsumer setter) {
        this.label = label;
        this.min = min;
        this.max = max;
        this.decimals = decimals;
        this.percent = percent;
        this.getter = getter;
        this.setter = setter;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        double lo = min.getAsDouble();
        double hi = max.getAsDouble();
        double value = clamp(getter.getAsDouble(), lo, hi);

        int trackTop = y + 13;
        int trackBottom = y + 17;

        fr.drawString(label, x, y, 0xFFB8B8B8);
        String text = format(value);
        fr.drawString(text, x + width - fr.getStringWidth(text), y, 0xFFFFFFFF);

        Gui.drawRect(x, trackTop, x + width, trackBottom, 0xFF2A2A2A);

        double t = hi > lo ? (value - lo) / (hi - lo) : 0;
        int knobX = x + (int) Math.round(t * width);

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
        double lo = min.getAsDouble();
        double hi = max.getAsDouble();
        double t = (mouseX - x) / (double) width;
        double raw = lo + t * (hi - lo);
        setter.accept(round(clamp(raw, lo, hi)));
    }

    private boolean contains(int mouseX, int mouseY) {
        return mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 22;
    }

    private double clamp(double v, double lo, double hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    private double round(double v) {
        // Percent values are stored as fractions, so keep two extra digits of
        // precision on the raw value behind the displayed percentage.
        double factor = Math.pow(10, percent ? decimals + 2 : decimals);
        return Math.round(v * factor) / factor;
    }

    private String format(double value) {
        if (percent)
            return String.format("%." + decimals + "f%%", value * 100);
        return String.format("%." + decimals + "f", value);
    }
}
