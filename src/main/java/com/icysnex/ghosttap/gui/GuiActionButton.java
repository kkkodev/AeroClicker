package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

// Simple full-width clickable button that runs a supplied action.
public class GuiActionButton {

    public static final int ROW_HEIGHT = 20;

    private final String text;
    private final Runnable action;

    public String tooltip;
    public int x, y, width;

    public GuiActionButton(String text, Runnable action) {
        this.text = text;
        this.action = action;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        boolean hover = contains(mouseX, mouseY);
        Gui.drawRect(x, y, x + width, y + 16, hover ? 0xFF3A3A3A : 0xFF262626);
        int color = hover ? 0xFFFFFFFF : 0xFFC8C8C8;
        int textX = x + (width - fr.getStringWidth(text)) / 2;
        fr.drawString(text, textX, y + 4, color);
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        if (!contains(mouseX, mouseY))
            return false;

        action.run();
        return true;
    }

    private boolean contains(int mouseX, int mouseY) {
        return mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 16;
    }
}
