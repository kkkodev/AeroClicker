package dev.aeroclicker.bridge;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

import net.fabricmc.api.ClientModInitializer;

/**
 * Publishes the two pieces of client state AeroClicker cannot observe from outside the game:
 * the selected hotbar slot and whether any screen (inventory, chat, chest, NPC dialog, ...) is
 * open. It only ever reads; nothing is sent to the server and no input is changed.
 *
 * <p>The state goes to {@code $XDG_RUNTIME_DIR/aeroclicker-mc.state} as one line:
 * {@code slot=<0-8 or -1> screen=<0|1> ms=<epoch milliseconds>}. The app treats a file whose
 * timestamp is a couple of seconds old as absent, so a crashed game cannot leave the clicker
 * stuck.</p>
 *
 * <p>Everything about the game is reached through reflection on purpose: it keeps this file
 * free of Minecraft internals, so it compiles with nothing but the Fabric loader jar and keeps
 * working across 26.1.2 / 26.2 / 26.3, where the screen moved from {@code Minecraft.screen} to
 * {@code Gui.screen()}.</p>
 */
public final class StateExport implements ClientModInitializer {
    private static final Path STATE = Path.of(
            System.getenv().getOrDefault("XDG_RUNTIME_DIR", "/tmp"), "aeroclicker-mc.state");
    private static final Path TEMP = STATE.resolveSibling(STATE.getFileName() + ".tmp");

    /** Poll interval: the app polls at 50 ms too, so this keeps the two in step. */
    private static final long INTERVAL_MS = 50;
    /** Unchanged state is still rewritten this often, so the timestamp never goes stale. */
    private static final long HEARTBEAT_MS = 500;

    @Override
    public void onInitializeClient() {
        Thread thread = new Thread(StateExport::loop, "aeroclicker-bridge");
        thread.setDaemon(true);
        thread.start();
    }

    private static void loop() {
        String last = "";
        long lastWrite = 0;
        Binding binding = null;
        while (true) {
            try {
                if (binding == null) binding = new Binding();
                String state = snapshot(binding);
                long now = System.currentTimeMillis();
                if (!state.equals(last) || now - lastWrite >= HEARTBEAT_MS) {
                    write(state + " ms=" + now);
                    last = state;
                    lastWrite = now;
                }
                Thread.sleep(INTERVAL_MS);
            } catch (InterruptedException e) {
                return;
            } catch (Throwable t) {
                // Never let a transient client state (a level being torn down, a resource
                // reload) kill the bridge. "Unknown" reads as "pause" on the app's side.
                try {
                    write("slot=-1 screen=1 ms=" + System.currentTimeMillis());
                    Thread.sleep(HEARTBEAT_MS);
                } catch (Throwable ignored) {
                    return;
                }
            }
        }
    }

    private static String snapshot(Binding b) throws ReflectiveOperationException {
        Object mc = b.getInstance.invoke(null);
        Object player = mc == null ? null : b.player.get(mc);
        if (player == null) {
            // Not in a world: nothing can be clicked, so report "GUI open" and no slot.
            return "slot=-1 screen=1";
        }
        int slot = (Integer) b.getSelectedSlot.invoke(b.getInventory.invoke(player));
        return "slot=" + (slot >= 0 && slot < 9 ? slot : -1) + " screen=" + (screenOpen(b, mc) ? 1 : 0);
    }

    private static boolean screenOpen(Binding b, Object mc) {
        try {
            Object screen = b.guiScreen != null ? b.guiScreen.invoke(b.gui.get(mc))
                                                : b.mcScreen.get(mc);
            return screen != null;
        } catch (ReflectiveOperationException | RuntimeException e) {
            return true;   // cannot tell -> pause rather than click blindly
        }
    }

    /** Write through a temporary file so the app never sees a half-written line. */
    private static void write(String line) throws java.io.IOException {
        Files.write(TEMP, (line + "\n").getBytes(StandardCharsets.UTF_8));
        try {
            Files.move(TEMP, STATE, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        } catch (java.nio.file.AtomicMoveNotSupportedException e) {
            Files.move(TEMP, STATE, StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private static final class Binding {
        final Method getInstance;
        final Field player;
        final Method getInventory;
        final Method getSelectedSlot;
        final Field gui;
        final Method guiScreen;   // 26.2+
        final Field mcScreen;     // 26.1.2 only

        Binding() throws ReflectiveOperationException {
            Class<?> mc = Class.forName("net.minecraft.client.Minecraft");
            getInstance = mc.getMethod("getInstance");
            player = mc.getField("player");
            getInventory = Class.forName("net.minecraft.world.entity.player.Player")
                                .getMethod("getInventory");
            getSelectedSlot = Class.forName("net.minecraft.world.entity.player.Inventory")
                                   .getMethod("getSelectedSlot");
            gui = mc.getField("gui");
            Method screen = null;
            try {
                screen = Class.forName("net.minecraft.client.gui.Gui").getMethod("screen");
            } catch (ReflectiveOperationException ignored) {
                // 26.1.2 has no Gui.screen(); the field below is used instead.
            }
            guiScreen = screen;
            mcScreen = screen == null ? mc.getField("screen") : null;
        }
    }
}
