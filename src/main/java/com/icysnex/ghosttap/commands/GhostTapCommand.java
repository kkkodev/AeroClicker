package com.icysnex.ghosttap.commands;

import com.icysnex.ghosttap.config.ConfigHandler;
import com.icysnex.ghosttap.utils.Chat;
import net.minecraft.command.CommandBase;
import net.minecraft.command.ICommandSender;
import net.minecraft.util.BlockPos;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;
import org.lwjgl.input.Keyboard;

import java.util.List;

// Fallback for rebinding the menu key from chat, in case the current key is taken
// by another mod and the menu can't be opened to rebind it in the UI.
public class GhostTapCommand extends CommandBase {

    @Override
    public String getCommandName() {
        return "ghosttap";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/" + getCommandName() + " key <keyName>";
    }

    @Override
    public boolean canCommandSenderUseCommand(ICommandSender sender) {
        return true;
    }

    @Override
    public void processCommand(ICommandSender sender, String[] args) {
        if (args.length >= 2 && args[0].equalsIgnoreCase("key")) {
            int code = Keyboard.getKeyIndex(args[1].toUpperCase());
            if (code == Keyboard.KEY_NONE) {
                Chat.error(sender, "Menu", "Unknown key: " + args[1]);
                return;
            }

            ConfigHandler.openGuiKey = code;
            ConfigHandler.saveConfig();
            Chat.success(sender, "Menu", "Menu key set to " + Keyboard.getKeyName(code) + ".");
            return;
        }

        sender.addChatMessage(new ChatComponentText(EnumChatFormatting.RED + "Usage: " + getCommandUsage(sender)));
    }

    @Override
    public List<String> addTabCompletionOptions(ICommandSender sender, String[] args, BlockPos pos) {
        if (args.length == 1)
            return getListOfStringsMatchingLastWord(args, "key");
        return null;
    }
}
