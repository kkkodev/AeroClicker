package com.icysnex.ghosttap.utils;

import net.minecraft.command.ICommandSender;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

public class Chat {

    static String getPrefix(String sub) {
        return EnumChatFormatting.DARK_GRAY + "[" + EnumChatFormatting.WHITE + "GhostTap" + EnumChatFormatting.DARK_GRAY + "-" + sub + "] " + EnumChatFormatting.GRAY;
    }


    public static void message(ICommandSender sender, String sub, String text) {
        sender.addChatMessage(new ChatComponentText(getPrefix(sub) + text));
    }

    public static void error(ICommandSender sender, String sub, String text) {
        sender.addChatMessage(new ChatComponentText(getPrefix(sub) + EnumChatFormatting.RED + text));
    }

    public static void success(ICommandSender sender, String sub, String text) {
        sender.addChatMessage(new ChatComponentText(getPrefix(sub) + EnumChatFormatting.GREEN + text));
    }
}
