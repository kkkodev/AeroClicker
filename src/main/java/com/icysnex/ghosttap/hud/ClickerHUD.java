package com.icysnex.ghosttap.hud;

import net.minecraft.client.Minecraft;
import net.minecraftforge.client.event.RenderGameOverlayEvent;
import net.minecraftforge.event.entity.player.AttackEntityEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.TickEvent;
import java.util.ArrayList;

public class ClickerHUD {

    private final ArrayList<Long> clicks = new ArrayList<>();

    @SubscribeEvent
    public void onAttack(AttackEntityEvent event) {
        if (event.entityPlayer.worldObj.isRemote && event.entityPlayer == Minecraft.getMinecraft().thePlayer) {
            registerClick();
        }
    }

    private void registerClick() {
        long now = System.currentTimeMillis();

        if (clicks.isEmpty() || now - clicks.get(clicks.size() - 1) > 5) {
            clicks.add(now);
        }
    }

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.START) return;

        long now = System.currentTimeMillis();
        clicks.removeIf(time -> now - time > 1000);
    }

    @SubscribeEvent
    public void onRenderGameOverlay(RenderGameOverlayEvent.Post event) {
        if (event.type == RenderGameOverlayEvent.ElementType.TEXT) {
            int cps = clicks.size();
            Minecraft.getMinecraft().fontRendererObj.drawStringWithShadow("CPS: " + cps, 5, 5, 0xFFFFFF);
        }
    }
}