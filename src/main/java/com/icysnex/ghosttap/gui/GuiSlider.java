package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

import java.util.function.DoubleConsumer;

// Single-value horizontal slider. Label sits on the left of its row, current
// value on the right, track underneath. Writes changes straight back through the
// supplied consumer so the screen stays a thin binding layer.
public class GuiSlider {

    public static final int ROW_HEIGHT = 24;

    private final String label;
    private final double min;
    private final double max;
    private final int decimals;
    private final DoubleConsumer onChange;

    private double value;
    public boolean dragging;

    public int x, y, width;

    public GuiSlider(String label, double min, double max, int decimals, double initial, DoubleConsumer onChange) {
        this.label = label;
        this.min = min;
        this.max = max;
        this.decimals = decimals;
        this.onChange = onChange;
        this.value = clamp(initial);
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        int trackTop = y + 13;
        int trackBottom = y + 17;

        fr.drawString(label, x, y, 0xFFB8B8B8);
        String text = format();
        fr.drawString(text, x + width - fr.getStringWidth(text), y, 0xFFFFFFFF);

        // Track background
        Gui.drawRect(x, trackTop, x + width, trackBottom, 0xFF2A2A2A);

        double t = (value - min) / (max - min);
        int knobX = x + (int) Math.round(t * width);

        // Filled portion + knob
        Gui.drawRect(x, trackTop, knobX, trackBottom, 0xFF5A9BD4);

        boolean hover = dragging || contains(mouseX, mouseY);
        int knobColor = hover ? 0xFFFFFFFF : 0xFFD0D0D0;
        Gui.drawRect(knobX - 2, y + 10, knobX + 3, y + 20, knobColor);
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
        double raw = min + t * (max - min);
        value = round(clamp(raw));
        onChange.accept(value);
    }

    // Hit area covers the whole row height so grabbing is forgiving.
    private boolean contains(int mouseX, int mouseY) {
        return mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 22;
    }

    private double clamp(double v) {
        return Math.max(min, Math.min(max, v));
    }

    private double round(double v) {
        double factor = Math.pow(10, decimals);
        return Math.round(v * factor) / factor;
    }

    private String format() {
        return String.format("%." + decimals + "f", value);
    }
}
