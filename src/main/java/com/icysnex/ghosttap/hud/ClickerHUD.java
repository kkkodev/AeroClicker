package com.icysnex.ghosttap.hud;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.Cps;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.Gui;
import net.minecraft.util.EnumChatFormatting;
import net.minecraftforge.client.event.RenderGameOverlayEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

import java.util.ArrayList;
import java.util.List;

// Configurable overlay: a CPS counter and/or per-clicker status, drawn at a
// user-set position with an optional background.
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
            Gui.drawRect(x - pad, y - pad, x + w + pad, y + lines.size() * lineHeight - 1 + pad, 0x90000000);
        }

        for (int i = 0; i < lines.size(); i++)
            fr.drawStringWithShadow(lines.get(i), x, y + i * lineHeight, 0xFFFFFFFF);
    }

    private List<String> buildLines() {
        List<String> lines = new ArrayList<>();
        boolean status = ConfigHandler.hudShowStatus;
        boolean cps = ConfigHandler.hudShowCps;

        if (status) {
            lines.add(statusLine("Left", Clicker.LEFT, ConfigHandler.leftMode, cps, Cps.left()));
            lines.add(statusLine("Right", Clicker.RIGHT, ConfigHandler.rightMode, cps, Cps.right()));
        } else if (cps) {
            lines.add(EnumChatFormatting.AQUA + "CPS  "
                    + EnumChatFormatting.GRAY + "L " + EnumChatFormatting.WHITE + Cps.left() + "  "
                    + EnumChatFormatting.GRAY + "R " + EnumChatFormatting.WHITE + Cps.right());
        }

        return lines;
    }

    private String statusLine(String name, Clicker clicker, ActivationMode mode, boolean cps, int value) {
        String state = clicker.isEnabled()
                ? EnumChatFormatting.GREEN + "ON"
                : EnumChatFormatting.DARK_GRAY + "OFF";
        String line = EnumChatFormatting.WHITE + name + "  " + state + " " + EnumChatFormatting.DARK_GRAY + mode.label;
        if (cps)
            line += " " + EnumChatFormatting.AQUA + value;
        return line;
    }
}
