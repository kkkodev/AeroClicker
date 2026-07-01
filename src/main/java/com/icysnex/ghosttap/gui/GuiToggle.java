package com.icysnex.ghosttap.gui;

import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;

import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

// Boolean row: label on the left, ON/OFF pill on the right. Reads live state
// through a supplier so external changes (e.g. hotkey toggles) stay in sync.
public class GuiToggle {

    public static final int ROW_HEIGHT = 18;

    private final String label;
    private final BooleanSupplier getter;
    private final Consumer<Boolean> onChange;

    public int x, y, width;

    public GuiToggle(String label, BooleanSupplier getter, Consumer<Boolean> onChange) {
        this.label = label;
        this.getter = getter;
        this.onChange = onChange;
    }

    public void draw(FontRenderer fr, int mouseX, int mouseY) {
        boolean on = getter.getAsBoolean();

        fr.drawString(label, x, y + 3, 0xFFB8B8B8);

        String text = on ? "ON" : "OFF";
        int pillW = 34;
        int pillX = x + width - pillW;
        int color = on ? 0xFF3FA34D : 0xFF7A2E2E;
        if (contains(mouseX, mouseY))
            color |= 0x00202020;

        Gui.drawRect(pillX, y, pillX + pillW, y + 14, color);
        int textX = pillX + (pillW - fr.getStringWidth(text)) / 2;
        fr.drawString(text, textX, y + 3, 0xFFFFFFFF);
    }

    public boolean mouseClicked(int mouseX, int mouseY) {
        if (!contains(mouseX, mouseY))
            return false;

        onChange.accept(!getter.getAsBoolean());
        return true;
    }

    private boolean contains(int mouseX, int mouseY) {
        int pillW = 34;
        int pillX = x + width - pillW;
        return mouseX >= pillX && mouseX <= pillX + pillW && mouseY >= y && mouseY <= y + 14;
    }
}
