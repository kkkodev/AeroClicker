package com.icysnex.ghosttap.commands;

import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.core.analytics.ClickData;
import com.icysnex.ghosttap.core.analytics.Tracker;
import com.icysnex.ghosttap.utils.Chat;
import com.icysnex.ghosttap.utils.FileIO;
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
        return "/" + getCommandName() + " <debug | export | clear>";
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

            case "clear":
                Tracker.clear();

                Chat.message(sender, "Analytics", "Cleared all tracking data.");
                break;

            case "stats":
                if (Tracker.size() <= 0) {
                    Chat.error(sender, "Analytics", "No data collected. Run '/gta toggle' and start clicking.");
                    return;
                }

                double avgCps = Tracker.stream().mapToDouble(d -> d.actualCps).average().orElse(0.0);
                Chat.message(sender, "Analytics", "Recorded " + Tracker.size() + " clicks. Average Speed: " + String.format( "%.2f", avgCps) + " CPS.");
                break;

            case "export":
                if (Tracker.size() <= 0) {
                    Chat.error(sender, "Analytics", "No data collected. Run '/gta toggle' and start clicking.");
                    return;
                }

                String fileName = "analytics_" + System.currentTimeMillis() + ".csv";
                List<ClickData> snapshot = Tracker.copy();

                Chat.message(sender, "Analytics", EnumChatFormatting.YELLOW + "Exporting session...");

                FileIO.saveAsync("analytics/" + fileName, (writer) -> {
                    writer.println("# --- START METADATA ---");
                    writer.printf("# CPS: %.1f (SD: %.1f, Range %.1f-%.1f +-%.1f)%n", Clicker.CPS_MEAN, Clicker.CPS_STANDARDDEVIATION, Clicker.CPS_MIN, Clicker.CPS_MAX, Clicker.CPS_MINMAX_FALLOUT);
                    writer.printf("# - Spike %.1f%% (%.1f-%.1f) %n", Clicker.SPIKE_CHANCE * 100, Clicker.SPIKE_MIN, Clicker.SPIKE_MAX);
                    writer.printf("# - Stutter %.1f%% (%.1f-%.1f) %n", Clicker.STUTTER_CHANCE * 100, Clicker.STUTTER_MIN, Clicker.STUTTER_MAX);
                    writer.printf("# Hold: %.1fms (SD: %.1f, Range %.1f-%.1f)%n", Clicker.HOLD_MS_MEAN, Clicker.HOLD_MS_STANDARDDEVIATION, Clicker.HOLD_MS_MIN, Clicker.HOLD_MS_MAX);
                    writer.printf("# - Heavy: %.1f%% (%.1f-%.1f)%n", Clicker.HOLD_MS_HEAVY_CHANCE, Clicker.HOLD_MS_HEAVY_MIN, Clicker.HOLD_MS_HEAVY_MAX);
                    writer.printf("# Rythm: %.1f%% %.1f%n", Clicker.RHYTHM_VOLATILITY, Clicker.RHYTHM_TENSION);
                    writer.println("# --- END METADATA ---");

                    writer.println("Timestamp,TargetCPS,ActualCPS,HoldMS,IntervalMS,Trend");
                    for (ClickData d : snapshot) {
                        writer.printf("%d,%.2f,%.2f,%.2f,%.2f,%.2f%n", d.timestamp, d.targetCps, d.actualCps, d.holdNanos / 1_000_000.0, d.intervalNanos / 1_000_000.0, d.trend);
                    }
                }, (success) -> {
                    if (success) {
                        Chat.success(sender, "Analytics", "Exported successfully: " + fileName + ".");
                    } else {
                        Chat.error(sender, "Analytics", "Export failed! Check console.");
                    }
                });
                break;

            default:
                Chat.error(sender, "Analytics", "Unknown argument. Use toggle, export, clear, or stats.");
                break;
        }
    }

    @Override
    public List<String> addTabCompletionOptions(ICommandSender sender, String[] args, BlockPos pos) {
        return getListOfStringsMatchingLastWord(args, "toggle", "export", "clear", "stats");
    }
}
