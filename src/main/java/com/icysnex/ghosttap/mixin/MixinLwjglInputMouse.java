package com.icysnex.ghosttap.mixin;

import com.icysnex.ghosttap.core.MouseInput;
import org.lwjgl.input.Mouse;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(Mouse.class)
public class MixinLwjglInputMouse {
    @Shadow private static int eventButton;
    @Shadow private static boolean eventState;

    @Inject(method = "next", at = @At("HEAD"), cancellable = true, remap = false)
    private static void onNext(CallbackInfoReturnable<Boolean> cir) {
        if (MouseInput.injectNextEvent()) {
            eventButton = MouseInput.getButton();
            eventState = MouseInput.getState();
            MouseInput.consumeEvent();

            cir.setReturnValue(true);
            cir.cancel();
        }
    }

    @Inject(method = "isButtonDown", at = @At("HEAD"), cancellable = true, remap = false)
    private static void onIsButtonDown(int button, CallbackInfoReturnable<Boolean> cir) {
        if (MouseInput.getButton() == button && MouseInput.getState()) {
            cir.setReturnValue(true);
            cir.cancel();
        }
    }

    @Inject(method = "getEventButtonState", at = @At("HEAD"), cancellable = true, remap = false)
    private static void onGetEventButtonState(CallbackInfoReturnable<Boolean> cir) {
        if (MouseInput.injectNextEvent()) {
            cir.setReturnValue(MouseInput.getState());
            cir.cancel();
        }
    }

    @Inject(method = "getEventButton", at = @At("HEAD"), cancellable = true, remap = false)
    private static void onGetEventButton(CallbackInfoReturnable<Integer> cir) {
        if (MouseInput.injectNextEvent()) {
            cir.setReturnValue(MouseInput.getButton());
            cir.cancel();
        }
    }
}
