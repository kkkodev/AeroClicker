package com.icysnex.ghosttap.mixin;

import com.icysnex.ghosttap.core.Clicker;
import net.minecraft.client.Minecraft;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(Minecraft.class)
public abstract class MixinMinecraft {

    @Inject(method = "runTick", at = @At("HEAD"), remap = false)
    private void onTick(CallbackInfo ci) {
        Clicker.tick();
    }
}