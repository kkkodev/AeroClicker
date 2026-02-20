package com.icysnex.ghosttap.mixin;

import org.lwjgl.input.Mouse;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

import java.nio.ByteBuffer;

@Mixin(Mouse.class)
public interface MixinLwjglInputMouseAccessor {

    @Accessor(value = "readBuffer", remap = false)
    static ByteBuffer getReadBuffer() {
        throw new IllegalStateException("Mixin injection for Mouse.readBuffer failed.");
    }
}
