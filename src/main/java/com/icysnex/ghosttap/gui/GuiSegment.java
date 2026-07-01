package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

import java.util.function.IntConsumer;
import java.util.function.IntSupplier;

// Segmented selector: label on the left, a row of pills on the right with the
// active option highlighted. Click a pill to select it.
public class GuiSegment {

    public static final int ROW_HEIGHT = 18;
    private static final int SEG_W = 150;

    final String label;
    private final String[] options;
    private final IntSupplier getIndex;
    private final IntConsumer setIndex;

    public String tooltip;
    public int x, y, width;

    public GuiSegment(String label, String[] options, IntSupplier getIndex, IntConsumer setIndex) {
        this.label = label;
        this.options = options;
        this.getIndex = getIndex;
        this.setIndex = setIndex;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        fr.drawString(label, x, y + 3, 0xFFB8B8B8);

        int segX = x + width - SEG_W;
        int pillW = SEG_W / options.length;
        int active = getIndex.getAsInt();

        for (int i = 0; i < options.length; i++) {
            int px = segX + i * pillW;
            boolean isActive = i == active;
            boolean hover = mouseX >= px && mouseX <= px + pillW && mouseY >= y && mouseY <= y + 14;

            int bg = isActive ? 0xFF5A9BD4 : (hover ? 0xFF3A3A3A : 0xFF262626);
            Gui.drawRect(px, y, px + pillW - 1, y + 14, bg);

            int color = isActive ? 0xFFFFFFFF : 0xFFC8C8C8;
            int textX = px + (pillW - fr.getStringWidth(options[i])) / 2;
            fr.drawString(options[i], textX, y + 3, color);
        }
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        int segX = x + width - SEG_W;
        int pillW = SEG_W / options.length;

        if (mouseY < y || mouseY > y + 14 || mouseX < segX || mouseX > segX + pillW * options.length)
            return false;

        int i = (mouseX - segX) / pillW;
        if (i >= 0 && i < options.length) {
            setIndex.accept(i);
            return true;
        }
        return false;
    }
}
