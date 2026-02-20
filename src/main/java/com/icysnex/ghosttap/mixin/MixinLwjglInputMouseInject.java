package com.icysnex.ghosttap.mixin;

import com.icysnex.ghosttap.core.InputMouse;
import org.lwjgl.input.Mouse;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.nio.ByteBuffer;

@Mixin(Mouse.class)
public abstract class MixinLwjglInputMouseInject {

    @Shadow(remap = false)
    private static ByteBuffer buttons;

    private static byte prevRealLeft = InputMouse.BUTTON_UP;
    private static byte prevRealRight = InputMouse.BUTTON_UP;

    @Inject(method = "poll", at = @At("RETURN"), remap = false)
    private static void afterPoll(CallbackInfo ci) {
        // 1. Process LEFT Click (Index 0)
        final byte realLeft = buttons.get(0);

        byte combinedLeft = (byte)(realLeft | InputMouse.spoofedLeft);
        if (prevRealLeft == InputMouse.BUTTON_DOWN && realLeft == InputMouse.BUTTON_UP) {
            combinedLeft = InputMouse.BUTTON_UP;

            if (InputMouse.spoofedLeft == InputMouse.BUTTON_DOWN)
                InputMouse.upLeft();
        }

        if (realLeft != combinedLeft)
            buttons.put(0, combinedLeft);
        prevRealLeft = realLeft;

        // 2. Process RIGHT Click (Index 1)
        final byte realRight = buttons.get(1);

        byte combinedRight = (byte)(realRight | InputMouse.spoofedRight);
        if (prevRealRight == InputMouse.BUTTON_DOWN && realRight == InputMouse.BUTTON_UP) {
            combinedRight = InputMouse.BUTTON_UP;

            if (InputMouse.spoofedRight == InputMouse.BUTTON_DOWN)
                InputMouse.upRight();
        }

        if (realRight != combinedRight)
            buttons.put(1, combinedRight);
        prevRealRight = realRight;

    }
}

