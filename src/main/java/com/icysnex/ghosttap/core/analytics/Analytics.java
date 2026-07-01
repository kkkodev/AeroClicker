package com.icysnex.ghosttap.core.analytics;

import com.icysnex.ghosttap.core.Clicker;
import com.icysnex.ghosttap.utils.Chat;
import com.icysnex.ghosttap.utils.FileIO;
import net.minecraft.command.ICommandSender;
import net.minecraft.util.EnumChatFormatting;

import java.util.List;

// Shared analytics actions so the /gta command and the config UI both go through
// one place. Each button exports to its own CSV keeping the single-button format
// the python analyzer expects.
public class Analytics {

    public static int totalSize() {
        return Clicker.LEFT.tracker.size() + Clicker.RIGHT.tracker.size();
    }

    public static void clear() {
        Clicker.LEFT.tracker.clear();
        Clicker.RIGHT.tracker.clear();
    }

    public static void export(ICommandSender sender) {
        if (totalSize() <= 0) {
            Chat.error(sender, "Analytics", "No data collected. Enable tracking and start clicking.");
            return;
        }

        Chat.message(sender, "Analytics", EnumChatFormatting.YELLOW + "Exporting session...");
        exportButton(sender, Clicker.LEFT, "left");
        exportButton(sender, Clicker.RIGHT, "right");
    }

    private static void exportButton(ICommandSender sender, Clicker c, String label) {
        if (c.tracker.size() <= 0)
            return;

        String fileName = "analytics_" + label + "_" + System.currentTimeMillis() + ".csv";
        List<ClickData> snapshot = c.tracker.copy();

        FileIO.saveAsync("analytics/" + fileName, (writer) -> {
            writer.printf("# --- START METADATA (%s) ---%n", label.toUpperCase());
            writer.printf("# CPS: %.1f (SD: %.1f, Range %.1f-%.1f +-%.1f)%n", c.cpsMean, c.cpsStandardDeviation, c.cpsMin, c.cpsMax, c.cpsMinMaxFallout);
            writer.printf("# - Spike %.1f%% (%.1f-%.1f) %n", c.spikeChance * 100, c.spikeMin, c.spikeMax);
            writer.printf("# - Stutter %.1f%% (%.1f-%.1f) %n", c.stutterChance * 100, c.stutterMin, c.stutterMax);
            writer.printf("# Hold: %.1fms (SD: %.1f, Range %.1f-%.1f)%n", c.holdMsMean, c.holdMsStandardDeviation, c.holdMsMin, c.holdMsMax);
            writer.printf("# - Heavy: %.1f%% (%.1f-%.1f)%n", c.holdMsHeavyChance, c.holdMsHeavyMin, c.holdMsHeavyMax);
            writer.printf("# Rythm: %.1f%% %.1f%n", c.rhythmVolatility, c.rhythmTension);
            writer.println("# --- END METADATA ---");

            writer.println("Timestamp,TargetCPS,ActualCPS,HoldMS,IntervalMS,Trend");
            for (ClickData d : snapshot) {
                writer.printf("%d,%.2f,%.2f,%.2f,%.2f,%.2f%n", d.timestamp, d.targetCps, d.actualCps, d.holdNanos / 1_000_000.0, d.intervalNanos / 1_000_000.0, d.trend);
            }
        }, (success) -> {
            if (success) {
                Chat.success(sender, "Analytics", "Exported " + label + ": " + fileName);
            } else {
                Chat.error(sender, "Analytics", "Export failed for " + label + "! Check console.");
            }
        });
    }
}
