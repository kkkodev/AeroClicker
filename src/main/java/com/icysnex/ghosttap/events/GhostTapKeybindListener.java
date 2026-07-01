package com.icysnex.ghosttap.events;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.gui.GuiGhostTap;
import net.minecraft.client.Minecraft;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.InputEvent;
import org.lwjgl.input.Keyboard;

public class GhostTapKeybindListener {

    // KeyInputEvent only fires while no GUI is open, so this handles the in-world
    // hotkeys only. The config screen manages its own key handling.
    @SubscribeEvent
    public void onKey(InputEvent.KeyInputEvent event) {
        if (!Keyboard.getEventKeyState())
            return;

        int key = Keyboard.getEventKey();

        if (key == ConfigHandler.toggleLeftKey) {
            Clicker.LEFT.toggle();
        } else if (key == ConfigHandler.toggleRightKey) {
            Clicker.RIGHT.toggle();
        } else if (key == ConfigHandler.openGuiKey) {
            Minecraft mc = Minecraft.getMinecraft();
            if (mc.currentScreen == null)
                mc.displayGuiScreen(new GuiGhostTap());
        }
    }
}
