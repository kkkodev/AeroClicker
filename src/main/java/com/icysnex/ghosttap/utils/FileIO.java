package com.icysnex.ghosttap.utils;

import net.minecraft.client.Minecraft;

import java.io.File;
import java.io.PrintWriter;
import java.util.function.Consumer;

public class FileIO {

    public static void saveAsync(String fileName, Consumer<PrintWriter> writerConsumer, Consumer<Boolean> onComplete) {
        new Thread(() -> {
            try {
                File baseDir = new File(Minecraft.getMinecraft().mcDataDir, "GhostTap");
                File targetFile = new File(baseDir, fileName);

                File parentFolder = targetFile.getParentFile();
                if (parentFolder != null && !parentFolder.exists()) {
                    if (!parentFolder.mkdirs()) {
                        onComplete.accept(false);
                        return;
                    }
                }

                try (PrintWriter writer = new PrintWriter(targetFile)) {
                    writerConsumer.accept(writer);
                }
                onComplete.accept(true);
            } catch (Exception e) {
                e.printStackTrace();
                onComplete.accept(false);
            }
        }, "GhostTap-IO").start();
    }
}
