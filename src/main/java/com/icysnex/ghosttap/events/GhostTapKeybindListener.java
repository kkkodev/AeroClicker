package com.icysnex.ghosttap.events;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.core.ActivationMode;
import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.Gates;
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
        boolean screenOpen = mc.currentScreen != null;

        boolean openDown = !screenOpen && Keyboard.isKeyDown(ConfigHandler.openGuiKey);
        if (openDown && !wasOpenDown)
            mc.displayGuiScreen(new GuiGhostTap());
        wasOpenDown = openDown;

        process(Clicker.LEFT, ConfigHandler.toggleLeftKey, ConfigHandler.leftMode, InputMouse.BUTTON_LEFT, left, screenOpen);
        process(Clicker.RIGHT, ConfigHandler.toggleRightKey, ConfigHandler.rightMode, InputMouse.BUTTON_RIGHT, right, screenOpen);
    }

    private void process(Clicker clicker, int key, ActivationMode mode, byte button, ButtonState st, boolean screenOpen) {
        // A clicker only acts when in the world, unless it's allowed in menus.
        boolean context = !screenOpen || clicker.gates.allowInMenu;
        boolean keyDown = context && Keyboard.isKeyDown(key);

        // "Intent" = does the user want it clicking, before context gates apply.
        boolean intent = false;
        switch (mode) {
            case TOGGLE:
                if (keyDown && !st.wasKeyDown)
                    clicker.toggledOn = !clicker.toggledOn;
                intent = clicker.toggledOn;
                break;

            case HOLD:
                intent = keyDown;
                break;

            case MOUSE:
                if (keyDown && !st.wasKeyDown)
                    clicker.armed = !clicker.armed;
                boolean realDown = context && InputMouse.real(button) == InputMouse.STATE_DOWN;
                intent = clicker.armed && realDown;
                break;
        }
        st.wasKeyDown = keyDown;

        boolean enabled = intent && context && Gates.pass(clicker.gates, button);
        clicker.setEnabled(enabled);

        // Mask suppresses the real button.
        //  - Mouse mode: the physical button is the trigger, so it's owned the
        //    whole time we're armed. That keeps the hold from leaking through when
        //    a gate (e.g. block-break) stops clicking — otherwise a held button
        //    would still mine the block.
        //  - Toggle/Hold: the physical button isn't the trigger, so only mask
        //    while actually clicking to avoid desyncing the polled state.
        boolean mask = mode == ActivationMode.MOUSE ? clicker.armed : enabled;
        InputMouse.setMask(button, mask);
    }

    private static class ButtonState {
        boolean wasKeyDown;
    }
}
