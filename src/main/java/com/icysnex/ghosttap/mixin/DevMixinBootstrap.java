package com.icysnex.ghosttap.mixin;

import org.spongepowered.asm.launch.MixinBootstrap;
import org.spongepowered.asm.mixin.MixinEnvironment;
import org.spongepowered.asm.mixin.Mixins;

// Bootstraps Mixin in the dev environment only. In production the MixinTweaker
// (declared in the jar manifest) does this at the correct launch phase.
//
// This is a separate class on purpose: it references org.spongepowered.asm.*, so
// it must never be class-loaded in production. An early Mixin lookup from a
// coremod fails and gets cached in LaunchClassLoader's invalidClasses set, which
// then breaks every other mod that loads Mixin later (e.g. OneConfig).
final class DevMixinBootstrap {

    private DevMixinBootstrap() {
    }

    static void init() {
        MixinBootstrap.init();
        Mixins.addConfiguration("mixin.ghosttap.json");
        MixinEnvironment.getDefaultEnvironment().setSide(MixinEnvironment.Side.CLIENT);
    }
}
