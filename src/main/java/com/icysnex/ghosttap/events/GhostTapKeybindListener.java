package com.icysnex.ghosttap.events;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.InputMouse;
import com.icysnex.ghosttap.gui.GuiGhostTap;
import net.minecraft.client.Minecraft;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.TickEvent;
import org.lwjgl.input.Keyboard;

// Polled every client tick rather than driven by key events, so Hold mode sees
// releases reliably and Mouse mode can watch the real mouse button.
public class GhostTapKeybindListener {

    private final ButtonState left = new ButtonState();
    private final ButtonState right = new ButtonState();
    private boolean wasOpenDown = false;

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END)
            return;

        Minecraft mc = Minecraft.getMinecraft();
        boolean inWorld = mc.currentScreen == null && mc.inGameHasFocus;

        boolean openDown = inWorld && Keyboard.isKeyDown(ConfigHandler.openGuiKey);
        if (openDown && !wasOpenDown)
            mc.displayGuiScreen(new GuiGhostTap());
        wasOpenDown = openDown;

        process(Clicker.LEFT, ConfigHandler.toggleLeftKey, ConfigHandler.leftMode, InputMouse.BUTTON_LEFT, left, inWorld);
        process(Clicker.RIGHT, ConfigHandler.toggleRightKey, ConfigHandler.rightMode, InputMouse.BUTTON_RIGHT, right, inWorld);
    }

    private void process(Clicker clicker, int key, ActivationMode mode, byte button, ButtonState st, boolean inWorld) {
        boolean keyDown = inWorld && Keyboard.isKeyDown(key);

        switch (mode) {
            case TOGGLE:
                if (keyDown && !st.wasKeyDown)
                    clicker.toggle();
                break;

            case HOLD:
                clicker.setEnabled(keyDown);
                break;

            case MOUSE:
                if (keyDown && !st.wasKeyDown)
                    clicker.armed = !clicker.armed;

                boolean realDown = inWorld && InputMouse.real(button) == InputMouse.STATE_DOWN;
                clicker.setEnabled(clicker.armed && realDown);
                break;
        }

        // Suppress the real button whenever the spoofer owns it, so a physical
        // hold can't desync the polled state from the injected click events.
        boolean mask = mode == ActivationMode.MOUSE ? clicker.armed : clicker.isEnabled();
        InputMouse.setMask(button, mask);

        st.wasKeyDown = keyDown;
    }

    private static class ButtonState {
        boolean wasKeyDown;
    }
}
