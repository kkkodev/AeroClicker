package com.icysnex.ghosttap.commands;

import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.analytics.Analytics;
import com.icysnex.ghosttap.core.analytics.Tracker;
import com.icysnex.ghosttap.utils.Chat;
import net.minecraft.command.CommandBase;
import net.minecraft.command.ICommandSender;
import net.minecraft.util.BlockPos;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

import java.util.List;

public class GhostTapAnalyticsCommand extends CommandBase {

    @Override
    public String getCommandName() {
        return "gta";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/" + getCommandName() + " <toggle | stats | export | clear>";
    }

    @Override
    public boolean canCommandSenderUseCommand(ICommandSender sender) {
        return true;
    }

    @Override
    public void processCommand(ICommandSender sender, String[] args) {
        if (args.length == 0) {
            sender.addChatMessage(new ChatComponentText(EnumChatFormatting.RED + "Usage: " + getCommandUsage(sender)));
            return;
        }

        String sub = args[0].toLowerCase();
        switch (sub) {
            case "toggle":
                Tracker.enabled = !Tracker.enabled;

                String state = Tracker.enabled ? EnumChatFormatting.GREEN + "ENABLED" : EnumChatFormatting.RED + "DISABLED";
                Chat.message(sender, "Analytics", "Tracker is now " + state);
                break;

            case "stats":
                if (Analytics.totalSize() <= 0) {
                    Chat.error(sender, "Analytics", "No data collected. Run '/gta toggle' and start clicking.");
                    return;
                }

                Chat.message(sender, "Analytics", statLine("Left", Clicker.LEFT.tracker));
                Chat.message(sender, "Analytics", statLine("Right", Clicker.RIGHT.tracker));
                break;

            case "export":
                Analytics.export(sender);
                break;

            case "clear":
                Analytics.clear();
                Chat.message(sender, "Analytics", "Cleared all tracking data.");
                break;

            default:
                Chat.error(sender, "Analytics", "Unknown argument. Use toggle, stats, export, clear.");
                break;
        }
    }

    private static String statLine(String label, Tracker t) {
        return label + ": " + t.size() + " clicks, avg " + String.format("%.2f", t.getAverageCps()) + " CPS.";
    }

    @Override
    public List<String> addTabCompletionOptions(ICommandSender sender, String[] args, BlockPos pos) {
        return getListOfStringsMatchingLastWord(args, "toggle", "export", "clear", "stats");
    }
}
