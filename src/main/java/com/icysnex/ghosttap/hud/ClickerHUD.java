package com.icysnex.ghosttap.hud;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.Cps;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;
import net.minecraftforge.client.event.RenderGameOverlayEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

import java.util.ArrayList;
import java.util.List;

// Configurable overlay: a classic "CPS: L | R" counter and/or per-clicker status,
// drawn at a user-set position in a single chosen colour.
public class ClickerHUD {

    @SubscribeEvent
    public void onRenderGameOverlay(RenderGameOverlayEvent.Post event) {
        if (event.type != RenderGameOverlayEvent.ElementType.TEXT)
            return;
        if (!ConfigHandler.hudEnabled)
            return;

        Minecraft mc = Minecraft.getMinecraft();
        if (mc.thePlayer == null)
            return;

        List<String> lines = buildLines();
        if (lines.isEmpty())
            return;

        FontRenderer fr = mc.fontRendererObj;
        int x = ConfigHandler.hudX;
        int y = ConfigHandler.hudY;
        int lineHeight = fr.FONT_HEIGHT + 1;

        if (ConfigHandler.hudBackground) {
            int pad = ConfigHandler.hudPadding;
            int w = 0;
            for (String line : lines)
                w = Math.max(w, fr.getStringWidth(line));
            // -2 (not -1) so the box hugs the glyphs evenly top and bottom; the
            // font cell has an extra pixel below the visible text.
            Gui.drawRect(x - pad, y - pad, x + w + pad, y + lines.size() * lineHeight - 2 + pad, ConfigHandler.hudBgColor);
        }

        int color = 0xFF000000 | (ConfigHandler.hudTextColor & 0xFFFFFF);
        for (int i = 0; i < lines.size(); i++)
            fr.drawStringWithShadow(lines.get(i), x, y + i * lineHeight, color);
    }

    private List<String> buildLines() {
        List<String> lines = new ArrayList<>();

        if (ConfigHandler.hudShowCps) {
            String cps = cpsText();
            if (cps != null)
                lines.add(cps);
        }

        if (ConfigHandler.hudShowStatus) {
            lines.add(statusLine("Left", Clicker.LEFT, ConfigHandler.leftMode));
            lines.add(statusLine("Right", Clicker.RIGHT, ConfigHandler.rightMode));
        }

        return lines;
    }

    private String cpsText() {
        boolean left = ConfigHandler.hudCpsLeft;
        boolean right = ConfigHandler.hudCpsRight;

        if (left && right)
            return "CPS: " + Cps.left() + " | " + Cps.right();
        if (left)
            return "CPS: " + Cps.left();
        if (right)
            return "CPS: " + Cps.right();
        return null;
    }

    private String statusLine(String name, Clicker clicker, ActivationMode mode) {
        return name + ": " + (clicker.isEnabled() ? "ON" : "OFF") + " (" + mode.label + ")";
    }
}
