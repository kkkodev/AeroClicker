package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;
import org.lwjgl.input.Keyboard;

import java.util.function.IntConsumer;
import java.util.function.IntSupplier;

// Key rebinder row: click the button to arm it, then the next key press is
// captured by the screen and written back through onChange.
public class GuiKeybind {

    public static final int ROW_HEIGHT = 18;

    final String label;
    private final IntSupplier getter;
    private final IntConsumer onChange;

    public String tooltip;
    public boolean listening;
    public int x, y, width;

    public GuiKeybind(String label, IntSupplier getter, IntConsumer onChange) {
        this.label = label;
        this.getter = getter;
        this.onChange = onChange;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        fr.drawString(label, x, y + 3, 0xFFB8B8B8);

        int boxW = 70;
        int boxX = x + width - boxW;
        int color = listening ? 0xFF5A9BD4 : (contains(mouseX, mouseY) ? 0xFF3A3A3A : 0xFF2A2A2A);
        Gui.drawRect(boxX, y, boxX + boxW, y + 14, color);

        String text = listening ? "> press <" : Keyboard.getKeyName(getter.getAsInt());
        if (text == null)
            text = "NONE";
        int textX = boxX + (boxW - fr.getStringWidth(text)) / 2;
        fr.drawString(text, textX, y + 3, 0xFFFFFFFF);
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        if (!contains(mouseX, mouseY))
            return false;

        listening = true;
        return true;
    }

    // Called by the screen while armed. Escape cancels without rebinding.
    public void keyPressed(int keyCode) {
        if (keyCode != Keyboard.KEY_ESCAPE)
            onChange.accept(keyCode);
        listening = false;
    }

    private boolean contains(int mouseX, int mouseY) {
        int boxW = 70;
        int boxX = x + width - boxW;
        return mouseX >= boxX && mouseX <= boxX + boxW && mouseY >= y && mouseY <= y + 14;
    }
}
