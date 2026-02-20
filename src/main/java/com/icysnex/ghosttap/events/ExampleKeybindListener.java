package com.icysnex.ghosttap.events;

import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.InputMouse;
import net.minecraft.client.settings.KeyBinding;
import net.minecraftforge.fml.client.registry.ClientRegistry;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.InputEvent;
import org.lwjgl.input.Keyboard;

public class ExampleKeybindListener {

    private final KeyBinding onKeybind = new KeyBinding("on", Keyboard.KEY_J, "ExampleMod");
    private final KeyBinding offKeybind = new KeyBinding("off", Keyboard.KEY_K, "ExampleMod");

    public ExampleKeybindListener() {
        ClientRegistry.registerKeyBinding(onKeybind);
        ClientRegistry.registerKeyBinding(offKeybind);
    }


    private boolean wasOnPressed = false;
    private boolean wasOffPressed = false;

    @SubscribeEvent
    public void onKeyPress(InputEvent.KeyInputEvent event) {
        // Check ON key
        boolean isOnPressed = onKeybind.isPressed();
        if (isOnPressed && !wasOnPressed) {
//            InputMouse.downLeft();
            Clicker.setEnabled(true);
        }
        wasOnPressed = isOnPressed;

        // Check OFF key
        boolean isOffPressed = offKeybind.isPressed();
        if (isOffPressed && !wasOffPressed) {
//            InputMouse.upLeft();
            Clicker.setEnabled(false);
        }
        wasOffPressed = isOffPressed;
    }
}
