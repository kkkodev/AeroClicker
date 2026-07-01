package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;

// Lays out several action buttons side by side. An optional left label lines the
// row up with the other titled rows.
public class GuiButtonRow {

    public static final int ROW_HEIGHT = 20;
    private static final int GAP = 4;
    private static final int LABEL_W = 44;

    final String label;
    final GuiActionButton[] buttons;
    public String tooltip;
    public int x, y, width;

    public GuiButtonRow(String label, GuiActionButton... buttons) {
        this.label = label;
        this.buttons = buttons;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        layout();
        if (label != null)
            fr.drawString(label, x, y + 3, 0xFFB8B8B8);
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
        int startX = label != null ? x + LABEL_W : x;
        int avail = x + width - startX;
        int n = buttons.length;
        int each = (avail - GAP * (n - 1)) / n;
        for (int i = 0; i < n; i++) {
            GuiActionButton b = buttons[i];
            b.x = startX + i * (each + GAP);
            b.y = y;
            b.width = each;
        }
    }
}
