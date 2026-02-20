package com.icysnex.ghosttap.mixin;

import net.minecraftforge.fml.relauncher.IFMLLoadingPlugin;
import org.spongepowered.asm.launch.MixinBootstrap;
import org.spongepowered.asm.mixin.MixinEnvironment;
import org.spongepowered.asm.mixin.Mixins;

import java.lang.reflect.Field;
import java.util.Map;
import java.util.Set;

public class MixinLoader implements IFMLLoadingPlugin {
    public MixinLoader() {
        System.out.println("[GhostTap] Injecting Mixin with IFMLLoadingPlugin.");

        MixinBootstrap.init();
        Mixins.addConfiguration("mixin.ghosttap.json");

        try {
            Field field = net.minecraft.launchwrapper.LaunchClassLoader.class.getDeclaredField("classLoaderExceptions");
            field.setAccessible(true);
            Set<String> exceptions = (Set<String>) field.get(net.minecraft.launchwrapper.Launch.classLoader);
            exceptions.remove("org.lwjgl.");
        } catch (Exception e) {
            e.printStackTrace();
        }

        MixinEnvironment.getDefaultEnvironment().setSide(MixinEnvironment.Side.CLIENT);
    }


    @Override
    public String[] getASMTransformerClass() {
        return new String[0];
    }

    @Override
    public String getModContainerClass() {
        return null;
    }

    @Override
    public String getSetupClass() {
        return null;
    }

    @Override
    public void injectData(Map<String, Object> data) { }

    @Override
    public String getAccessTransformerClass() {
        return null;
    }
}
