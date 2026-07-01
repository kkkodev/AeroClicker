package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;

import java.util.function.Supplier;

// Read-only row: label left, live value right. Value is pulled from the supplier
// every frame so displayed stats stay current while the screen is open.
public class GuiStat {

    public static final int ROW_HEIGHT = 13;

    final String label;
    private final Supplier<String> value;

    public String tooltip;
    public int x, y, width;

    public GuiStat(String label, Supplier<String> value) {
        this.label = label;
        this.value = value;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        fr.drawString(label, x, y, 0xFFB8B8B8);
        String text = value.get();
        fr.drawString(text, x + width - fr.getStringWidth(text), y, 0xFFFFFFFF);
    }
}
