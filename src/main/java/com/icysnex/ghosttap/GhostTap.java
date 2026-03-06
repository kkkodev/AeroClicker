package com.icysnex.ghosttap;

import com.icysnex.ghosttap.commands.ExampleCommand;
import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.events.ExampleKeybindListener;
import com.icysnex.ghosttap.hud.ClickerHUD;
import net.minecraftforge.client.ClientCommandHandler;
import net.minecraftforge.common.MinecraftForge;
import net.minecraftforge.fml.common.Mod;
import net.minecraftforge.fml.common.Mod.EventHandler;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;
import net.minecraftforge.fml.common.event.FMLPreInitializationEvent;

@Mod(modid = GhostTap.MODID, name = GhostTap.MODNAME, version = GhostTap.VERSION)
public class GhostTap {

    public static final String MODID = "ghosttap";
    public static final String MODNAME = "GhostTap";
    public static final String VERSION = "1.0";


    @EventHandler
    public void preInit(FMLPreInitializationEvent event) {
        ConfigHandler.loadConfig(event.getSuggestedConfigurationFile());
    }

    @EventHandler
    public void init(FMLInitializationEvent event) {
        ClientCommandHandler.instance.registerCommand(new ExampleCommand());

        MinecraftForge.EVENT_BUS.register(new ExampleKeybindListener());
        MinecraftForge.EVENT_BUS.register(new ClickerHUD());
    }
}
