package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

import java.util.function.IntConsumer;
import java.util.function.IntSupplier;

// Segmented selector: label on the left, a row of pills on the right with the
// active option highlighted. Click a pill to select it.
public class GuiSegment {

    public static final int ROW_HEIGHT = 18;
    private static final int LABEL_W = 42;

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

    // Pills fill the width after the label. Boundaries are computed exactly so the
    // last pill reaches the right edge (no leftover gap from integer division).
    private int segX() {
        return x + LABEL_W;
    }

    private int edge(int i) {
        int segX = segX();
        return segX + (x + width - segX) * i / options.length;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        fr.drawString(label, x, y + 3, 0xFFB8B8B8);

        int active = getIndex.getAsInt();

        for (int i = 0; i < options.length; i++) {
            int px = edge(i);
            int pxEnd = edge(i + 1);
            boolean isActive = i == active;
            boolean hover = mouseX >= px && mouseX <= pxEnd && mouseY >= y && mouseY <= y + 14;

            // 1px gap between pills, but the last one fills flush to the edge.
            int right = i == options.length - 1 ? pxEnd : pxEnd - 1;
            int bg = isActive ? 0xFF5A9BD4 : (hover ? 0xFF3A3A3A : 0xFF262626);
            Gui.drawRect(px, y, right, y + 14, bg);

            int color = isActive ? 0xFFFFFFFF : 0xFFC8C8C8;
            int textX = px + (pxEnd - px - fr.getStringWidth(options[i])) / 2;
            fr.drawString(options[i], textX, y + 3, color);
        }
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        if (mouseY < y || mouseY > y + 14 || mouseX < segX() || mouseX > x + width)
            return false;

        for (int i = 0; i < options.length; i++) {
            if (mouseX >= edge(i) && mouseX < edge(i + 1)) {
                setIndex.accept(i);
                return true;
            }
        }
        return false;
    }
}
