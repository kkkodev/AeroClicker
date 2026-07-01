package com.icysnex.ghosttap.hud;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.Cps;
import com.icysnex.ghosttap.core.HudAnchor;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;
import net.minecraft.client.gui.ScaledResolution;
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
        int lineHeight = fr.FONT_HEIGHT + 1;

        int w = 0;
        for (String line : lines)
            w = Math.max(w, fr.getStringWidth(line));
        // Trailing char spacing / lower font-cell row aren't visible, so trim a
        // pixel off the right and bottom to keep the padding even on all sides.
        int textW = w - 1;
        int textH = lines.size() * lineHeight - 2;

        ScaledResolution sr = new ScaledResolution(mc);
        int x = anchorX(textW, sr.getScaledWidth());
        int y = anchorY(textH, sr.getScaledHeight());

        if (ConfigHandler.hudBackground) {
            int pad = ConfigHandler.hudPadding;
            Gui.drawRect(x - pad, y - pad, x + textW + pad, y + textH + pad, ConfigHandler.hudBgColor);
        }

        int color = 0xFF000000 | (ConfigHandler.hudTextColor & 0xFFFFFF);
        for (int i = 0; i < lines.size(); i++)
            fr.drawStringWithShadow(lines.get(i), x, y + i * lineHeight, color);
    }

    // The background box extends `pad` beyond the text, so anchored positions
    // inset the text by the margin plus that padding to keep the box off the edge.
    private int edgePad() {
        return ConfigHandler.hudMargin + (ConfigHandler.hudBackground ? ConfigHandler.hudPadding : 0);
    }

    private int anchorX(int textW, int screenW) {
        HudAnchor a = ConfigHandler.hudAnchor;
        if (a == HudAnchor.MANUAL)
            return ConfigHandler.hudX;
        if (a == HudAnchor.TOP_RIGHT || a == HudAnchor.BOTTOM_RIGHT)
            return screenW - textW - edgePad();
        return edgePad();
    }

    private int anchorY(int textH, int screenH) {
        HudAnchor a = ConfigHandler.hudAnchor;
        if (a == HudAnchor.MANUAL)
            return ConfigHandler.hudY;
        if (a == HudAnchor.BOTTOM_LEFT || a == HudAnchor.BOTTOM_RIGHT)
            return screenH - textH - edgePad();
        return edgePad();
    }

    private List<String> buildLines() {
        List<String> lines = new ArrayList<>();

        String cps = cpsText();
        if (cps != null)
            lines.add(cps);

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
