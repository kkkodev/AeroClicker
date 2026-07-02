package com.icysnex.ghosttap.mixin;

import com.icysnex.ghosttap.core.click.Clicker;
import com.icysnex.ghosttap.core.input.Cps;
import com.icysnex.ghosttap.core.input.InputMouse;
import net.minecraft.client.Minecraft;
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
    @Unique
    private static byte ghostTap$prevOutLeft = InputMouse.STATE_UP;
    @Unique
    private static byte ghostTap$prevOutRight = InputMouse.STATE_UP;
    @Unique
    private static long ghostTap$lastRealLeft = 0;
    @Unique
    private static long ghostTap$lastRealRight = 0;

    @Inject(method = "poll", at = @At("RETURN"), remap = false)
    private static void afterPoll(CallbackInfo ci) {
        // Clicks made while a screen is open (inventory, chat, menus) aren't
        // gameplay clicks, so they don't count towards CPS or analytics.
        final boolean count = Minecraft.getMinecraft().currentScreen == null;

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

        if (count && ghostTap$prevOutLeft == InputMouse.STATE_UP && combinedLeft == InputMouse.STATE_DOWN) {
            Cps.hit(InputMouse.BUTTON_LEFT);
            // Real (physical) click: the spoofer records its own clicks in run().
            if (InputMouse.spoofedLeft != InputMouse.STATE_DOWN) {
                long now = System.nanoTime();
                Clicker.LEFT.tracker.recordReal(ghostTap$lastRealLeft == 0 ? 0 : now - ghostTap$lastRealLeft);
                ghostTap$lastRealLeft = now;
            }
        }
        ghostTap$prevOutLeft = combinedLeft;
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

        if (count && ghostTap$prevOutRight == InputMouse.STATE_UP && combinedRight == InputMouse.STATE_DOWN) {
            Cps.hit(InputMouse.BUTTON_RIGHT);
            if (InputMouse.spoofedRight != InputMouse.STATE_DOWN) {
                long now = System.nanoTime();
                Clicker.RIGHT.tracker.recordReal(ghostTap$lastRealRight == 0 ? 0 : now - ghostTap$lastRealRight);
                ghostTap$lastRealRight = now;
            }
        }
        ghostTap$prevOutRight = combinedRight;
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