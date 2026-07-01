package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;

// Lays out several action buttons side by side across the row width.
public class GuiButtonRow {

    public static final int ROW_HEIGHT = 20;
    private static final int GAP = 4;

    final GuiActionButton[] buttons;
    public int x, y, width;

    public GuiButtonRow(GuiActionButton... buttons) {
        this.buttons = buttons;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        layout();
        for (GuiActionButton b : buttons)
            b.draw(fr, mouseX, mouseY);
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        for (GuiActionButton b : buttons) {
            if (b.mouseClicked(mouseX, mouseY))
                return true;
        }
        return false;
    }

    void layout() {
        int n = buttons.length;
        int each = (width - GAP * (n - 1)) / n;
        for (int i = 0; i < n; i++) {
            GuiActionButton b = buttons[i];
            b.x = x + i * (each + GAP);
            b.y = y;
            b.width = each;
        }
    }
}
