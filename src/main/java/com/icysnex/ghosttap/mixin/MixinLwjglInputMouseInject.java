package com.icysnex.ghosttap.mixin;

import com.icysnex.ghosttap.core.InputMouse;
import org.lwjgl.input.Mouse;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import java.nio.ByteBuffer;

@Mixin(Mouse.class)
public abstract class MixinLwjglInputMouseInject {

    @Shadow(remap = false)
    private static ByteBuffer buttons;

    @Unique
    private static byte ghostTap$prevRealLeft = InputMouse.STATE_UP;
    @Unique
    private static byte ghostTap$prevRealRight = InputMouse.STATE_UP;
    @Unique
    private static boolean ghostTap$prevMaskLeft = false;
    @Unique
    private static boolean ghostTap$prevMaskRight = false;

    @Inject(method = "poll", at = @At("RETURN"), remap = false)
    private static void afterPoll(CallbackInfo ci) {
        // Left
        final byte realLeft = buttons.get(0);
        InputMouse.realLeft = realLeft;

        // When the mask lifts while the button is still physically held, the game
        // never saw a down-edge for it (it only saw the spoofed clicks). Re-issue a
        // press event so held actions like mining start immediately.
        boolean maskLeft = InputMouse.maskLeft;
        if (ghostTap$prevMaskLeft && !maskLeft && realLeft == InputMouse.STATE_DOWN) {
            InputMouse.pendingEvents.add(new InputMouse.Event(InputMouse.BUTTON_LEFT, InputMouse.STATE_DOWN));
        }
        ghostTap$prevMaskLeft = maskLeft;

        byte currentSpoofLeft = InputMouse.spoofedLeft;
        if (InputMouse.pollLeftLatch.getAndSet(false)) {
            currentSpoofLeft = InputMouse.STATE_DOWN;
        }

        byte combinedLeft;
        if (maskLeft) {
            // Mouse mode: hide the real hold, output only the spoofed clicks.
            combinedLeft = currentSpoofLeft;
        } else {
            combinedLeft = (byte)(realLeft | currentSpoofLeft);

            if (ghostTap$prevRealLeft == InputMouse.STATE_DOWN && realLeft == InputMouse.STATE_UP) {
                combinedLeft = InputMouse.STATE_UP;

                if (InputMouse.spoofedLeft == InputMouse.STATE_DOWN)
                    InputMouse.upLeft();

                InputMouse.pollLeftLatch.set(false);
            }
        }

        if (buttons.get(0) != combinedLeft)
            buttons.put(0, combinedLeft);
        ghostTap$prevRealLeft = realLeft;

        // Right
        final byte realRight = buttons.get(1);
        InputMouse.realRight = realRight;

        boolean maskRight = InputMouse.maskRight;
        if (ghostTap$prevMaskRight && !maskRight && realRight == InputMouse.STATE_DOWN) {
            InputMouse.pendingEvents.add(new InputMouse.Event(InputMouse.BUTTON_RIGHT, InputMouse.STATE_DOWN));
        }
        ghostTap$prevMaskRight = maskRight;

        byte currentSpoofRight = InputMouse.spoofedRight;
        if (InputMouse.pollRightLatch.getAndSet(false)) {
            currentSpoofRight = InputMouse.STATE_DOWN;
        }

        byte combinedRight;
        if (maskRight) {
            combinedRight = currentSpoofRight;
        } else {
            combinedRight = (byte)(realRight | currentSpoofRight);

            if (ghostTap$prevRealRight == InputMouse.STATE_DOWN && realRight == InputMouse.STATE_UP) {
                combinedRight = InputMouse.STATE_UP;

                if (InputMouse.spoofedRight == InputMouse.STATE_DOWN)
                    InputMouse.upRight();

                InputMouse.pollRightLatch.set(false);
            }
        }

        if (buttons.get(1) != combinedRight)
            buttons.put(1, combinedRight);
        ghostTap$prevRealRight = realRight;
    }


    @Shadow(remap = false)
    private static ByteBuffer readBuffer;

    @Inject(method = "next", at = @At("HEAD"), remap = false)
    private static void onNext(CallbackInfoReturnable<Boolean> cir) {
        InputMouse.Event event = InputMouse.pendingEvents.poll();
        if (event == null)
            return;

        // Save old position
        int oldPos = readBuffer.position();

        // Write at end
        readBuffer.position(readBuffer.limit());
        readBuffer.limit(readBuffer.capacity());

        readBuffer.put(event.button);
        readBuffer.put(event.state);

        readBuffer.putInt(Mouse.getX());
        readBuffer.putInt(Mouse.getY());

        readBuffer.putInt(0);
        readBuffer.putLong(System.nanoTime());

        // restore for reading
        readBuffer.limit(readBuffer.position());
        readBuffer.position(oldPos);
    }
}