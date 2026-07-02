package com.icysnex.ghosttap.core.click;

import com.icysnex.ghosttap.core.input.InputMouse;
import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityLivingBase;
import net.minecraft.item.Item;
import net.minecraft.item.ItemAxe;
import net.minecraft.item.ItemBlock;
import net.minecraft.item.ItemHoe;
import net.minecraft.item.ItemShears;
import net.minecraft.item.ItemStack;
import net.minecraft.item.ItemSword;
import net.minecraft.item.ItemTool;
import net.minecraft.util.AxisAlignedBB;
import net.minecraft.util.MovingObjectPosition;
import net.minecraft.util.Vec3;
import net.minecraft.world.WorldSettings;

// Evaluates a clicker's gates against live client state.
public final class Gates {

    private Gates() {
    }

    public static boolean pass(ClickerGates g, byte button) {
        Minecraft mc = Minecraft.getMinecraft();
        EntityPlayerSP player = mc.thePlayer;
        if (player == null)
            return false;

        WorldSettings.GameType mode = mc.playerController.getCurrentGameType();
        switch (mode) {
            case SURVIVAL: if (!g.survival) return false; break;
            case CREATIVE: if (!g.creative) return false; break;
            case ADVENTURE: if (!g.adventure) return false; break;
            default: return false; // spectator etc.: no interaction
        }

        // In a GUI, the aim/item/slot gates are about in-world clicking and would
        // veto inventory clicking, so only the game-mode gate above applies.
        if (mc.currentScreen != null)
            return true;

        if (g.pauseWhileUsingItem && player.isUsingItem())
            return false;

        int slot = player.inventory.currentItem;
        if (slot < 0 || slot > 8 || !g.slots[slot])
            return false;

        if (!categoryAllowed(g, player.getHeldItem()))
            return false;

        if (button == InputMouse.BUTTON_LEFT && g.allowBlockBreak && aimingBlock(mc))
            return false;

        // Random reach each check so the engagement distance varies like a human's.
        if (g.entityOnly && !entityInReach(mc, player, Variance.range(g.reachMin, g.reachMax)))
            return false;

        return true;
    }

    // True if a living entity is intersected by the look ray within `reach` blocks.
    private static boolean entityInReach(Minecraft mc, EntityPlayerSP player, double reach) {
        Vec3 eyes = player.getPositionEyes(1.0F);
        Vec3 look = player.getLook(1.0F);
        Vec3 end = eyes.addVector(look.xCoord * reach, look.yCoord * reach, look.zCoord * reach);

        AxisAlignedBB region = player.getEntityBoundingBox()
                .addCoord(look.xCoord * reach, look.yCoord * reach, look.zCoord * reach)
                .expand(1.0, 1.0, 1.0);

        for (Entity e : mc.theWorld.getEntitiesWithinAABBExcludingEntity(player, region)) {
            if (!(e instanceof EntityLivingBase))
                continue;

            double border = e.getCollisionBorderSize();
            AxisAlignedBB box = e.getEntityBoundingBox().expand(border, border, border);
            if (box.isVecInside(eyes))
                return true;

            MovingObjectPosition hit = box.calculateIntercept(eyes, end);
            if (hit != null && eyes.distanceTo(hit.hitVec) <= reach)
                return true;
        }
        return false;
    }

    private static boolean categoryAllowed(ClickerGates g, ItemStack held) {
        if (g.weapons && g.tools && g.blocks && g.other)
            return true;

        if (held == null)
            return g.other;

        Item item = held.getItem();
        boolean isWeapon = item instanceof ItemSword || item instanceof ItemAxe;
        boolean isTool = item instanceof ItemTool || item instanceof ItemHoe || item instanceof ItemShears;
        boolean isBlock = item instanceof ItemBlock;

        if (isWeapon && g.weapons) return true;
        if (isTool && g.tools) return true;
        if (isBlock && g.blocks) return true;
        if (!isWeapon && !isTool && !isBlock && g.other) return true;

        return false;
    }

    private static boolean aimingBlock(Minecraft mc) {
        return mc.objectMouseOver != null && mc.objectMouseOver.typeOfHit == MovingObjectPosition.MovingObjectType.BLOCK;
    }
}
