package dev.aeroclicker.bridge;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.List;
import java.util.Optional;
import java.util.function.Predicate;

import net.fabricmc.api.ClientModInitializer;

/**
 * Publishes the client state AeroClicker cannot observe from outside the game: the selected
 * hotbar slot, whether any screen (inventory, chat, chest, NPC dialog, ...) is open, whether the
 * held item is a block, whether it is a sword or a bed, and whether the crosshair is on a living
 * entity (a mob or another player). It only ever reads; nothing is sent to the server and no
 * input is changed.
 *
 * <p>The state goes to {@code $XDG_RUNTIME_DIR/aeroclicker-mc.state} as one line:
 * {@code slot=<0-8 or -1> screen=<0|1> ms=<epoch milliseconds> block=<0|1|-1>
 * sword=<0|1|-1> target=<0|1|-1> bed=<0|1|-1>}. The app treats a file whose timestamp is a
 * couple of seconds old as absent, so a crashed game cannot leave the clicker stuck. The
 * optional fields are last so an older app build still parses the line, and -1 means this build
 * cannot tell (the app then leaves the matching gate inert).</p>
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
    /**
     * The target gate keeps looking about one block past the vanilla entity reach, so a player
     * running in becomes a target just before they are close enough to hit. The extra block is
     * what the app calls the target lead.
     */
    private static final double TARGET_REACH = 4.0;
    /** Numerical slack so a ray exactly on a box edge still counts. */
    private static final double TARGET_BOX_PAD = 0.02;

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
                Snapshot snap = snapshot(binding);
                String state = snap.line + " block=" + snap.block
                             + " sword=" + snap.sword + " target=" + snap.target
                             + " bed=" + snap.bed;
                long now = System.currentTimeMillis();
                if (!state.equals(last) || now - lastWrite >= HEARTBEAT_MS) {
                    // `ms` goes before the optional fields so an app build without them still
                    // parses slot and screen from the same line.
                    write(snap.line + " ms=" + now + " block=" + snap.block
                          + " sword=" + snap.sword + " target=" + snap.target
                          + " bed=" + snap.bed);
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
                    write("slot=-1 screen=1 ms=" + System.currentTimeMillis()
                          + " block=-1 sword=-1 target=-1 bed=-1");
                    Thread.sleep(HEARTBEAT_MS);
                } catch (Throwable ignored) {
                    return;
                }
            }
        }
    }

    private static Snapshot snapshot(Binding b) throws ReflectiveOperationException {
        Object mc = b.getInstance.invoke(null);
        Object player = mc == null ? null : b.player.get(mc);
        if (player == null) {
            // Not in a world: nothing can be clicked, so report "GUI open" and no slot.
            return new Snapshot("slot=-1 screen=1", -1, -1, -1, -1);
        }
        Object inventory = b.getInventory.invoke(player);
        int slot = (Integer) b.getSelectedSlot.invoke(inventory);
        String line = "slot=" + (slot >= 0 && slot < 9 ? slot : -1)
                    + " screen=" + (screenOpen(b, mc) ? 1 : 0);
        return new Snapshot(line, b.blockState(inventory, slot),
                            b.swordState(inventory, slot), b.targetState(mc, player),
                            b.bedState(inventory, slot));
    }

    /** Logical state without the timestamp: compared to decide when to rewrite the file. */
    private static final class Snapshot {
        final String line;
        final int block;
        final int sword;
        final int target;
        final int bed;

        Snapshot(String line, int block, int sword, int target, int bed) {
            this.line = line;
            this.block = block;
            this.sword = sword;
            this.target = target;
            this.bed = bed;
        }
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
        // Held-item check, all null when this mapping does not expose it.
        final Method invGetItem;
        final Method stackIsEmpty;
        final Method stackItem;
        final Class<?> blockItemClass;
        // Sword check: the registry id is the only stable test across versions (SwordItem is
        // gone in 26.3 and modded swords are only guaranteed a name ending in "_sword").
        final Object itemRegistry;
        final Method registryGetKey;
        // Crosshair check: Minecraft.hitResult is either a BlockHitResult or an EntityHitResult.
        // A living entity covers both hostile/passive mobs and other players.
        final Field hitResult;
        final Method hitEntity;
        final Method hitLocation;
        final Class<?> entityHitClass;
        final Class<?> blockHitClass;
        final Class<?> livingClass;
        // Extended-reach check. Vanilla only puts an entity in hitResult within the normal
        // attack reach; this ray looks a little further so the gate opens as a player runs in.
        final Field level;
        final Method getEntitiesOfClass;
        final Method getBoundingBox;
        final Method inflate;
        final Method clip;
        final Method getLookAngle;
        final Method getX, getY, getZ, getEyeY;
        final Method vecGetX, vecGetY, vecGetZ;
        final java.lang.reflect.Constructor<?> vec3Ctor;
        final Class<?> vec3Class;

        Binding() throws ReflectiveOperationException {
            Class<?> mc = Class.forName("net.minecraft.client.Minecraft");
            getInstance = mc.getMethod("getInstance");
            player = mc.getField("player");
            Class<?> inventory = Class.forName("net.minecraft.world.entity.player.Inventory");
            getInventory = Class.forName("net.minecraft.world.entity.player.Player")
                                .getMethod("getInventory");
            getSelectedSlot = inventory.getMethod("getSelectedSlot");
            gui = mc.getField("gui");
            Method screen = null;
            try {
                screen = Class.forName("net.minecraft.client.gui.Gui").getMethod("screen");
            } catch (ReflectiveOperationException ignored) {
                // 26.1.2 has no Gui.screen(); the field below is used instead.
            }
            guiScreen = screen;
            mcScreen = screen == null ? mc.getField("screen") : null;

            // Optional: a mapping that renames the item API must not take slot and menu down
            // with it. When this fails, block is reported as -1 and the app's gate is inert.
            Method getItem = null, isEmpty = null, getStackItem = null;
            Class<?> blockItem = null;
            try {
                getItem = inventory.getMethod("getItem", int.class);
                Class<?> stack = Class.forName("net.minecraft.world.item.ItemStack");
                isEmpty = stack.getMethod("isEmpty");
                getStackItem = stack.getMethod("getItem");
                blockItem = Class.forName("net.minecraft.world.item.BlockItem");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            invGetItem = getItem;
            stackIsEmpty = isEmpty;
            stackItem = getStackItem;
            blockItemClass = blockItem;

            // Optional too: the sword gate reports -1 (inert) if the registry mapping is not
            // what this version calls it.
            Object itemReg = null;
            Method getKey = null;
            try {
                itemReg = Class.forName("net.minecraft.core.registries.BuiltInRegistries")
                              .getField("ITEM").get(null);
                getKey = Class.forName("net.minecraft.core.Registry")
                              .getMethod("getKey", Object.class);
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            itemRegistry = itemReg;
            registryGetKey = getKey;

            // Optional: the crosshair gate reports -1 (inert) when this mapping is missing.
            Field hit = null;
            Method getEntity = null;
            Class<?> entityHit = null, blockHit = null, livingType = null;
            try {
                hit = mc.getField("hitResult");
                entityHit = Class.forName("net.minecraft.world.phys.EntityHitResult");
                getEntity = entityHit.getMethod("getEntity");
                blockHit = Class.forName("net.minecraft.world.phys.BlockHitResult");
                livingType = Class.forName("net.minecraft.world.entity.LivingEntity");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            hitResult = hit;
            hitEntity = getEntity;
            entityHitClass = entityHit;
            blockHitClass = blockHit;
            livingClass = livingType;

            // Where a block hit landed, so the extended ray can stop at the wall instead of
            // reporting a player behind it.
            Method getLocation = null;
            try {
                getLocation = Class.forName("net.minecraft.world.phys.HitResult")
                                   .getMethod("getLocation");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            hitLocation = getLocation;

            // Optional extended reach, all null when the mapping is not what this version
            // calls it. The exact hitResult check above still works without it.
            Field lvl = null;
            Method entitiesOfClass = null, boundingBox = null, aabbInflate = null, aabbClip = null;
            Method lookAngle = null, ex = null, ey = null, ez = null, eyeY = null;
            Method vx = null, vy = null, vz = null;
            java.lang.reflect.Constructor<?> vecCtor = null;
            Class<?> vecType = null;
            try {
                lvl = mc.getField("level");
                vecType = Class.forName("net.minecraft.world.phys.Vec3");
                Class<?> aabb = Class.forName("net.minecraft.world.phys.AABB");
                Class<?> entity = Class.forName("net.minecraft.world.entity.Entity");
                entitiesOfClass = Class.forName("net.minecraft.world.level.Level")
                    .getMethod("getEntitiesOfClass", Class.class, aabb, Predicate.class);
                boundingBox = entity.getMethod("getBoundingBox");
                aabbInflate = aabb.getMethod("inflate", double.class);
                aabbClip = aabb.getMethod("clip", vecType, vecType);
                lookAngle = entity.getMethod("getLookAngle");
                ex = entity.getMethod("getX");
                ey = entity.getMethod("getY");
                ez = entity.getMethod("getZ");
                eyeY = entity.getMethod("getEyeY");
                vx = vecType.getMethod("getX");
                vy = vecType.getMethod("getY");
                vz = vecType.getMethod("getZ");
                vecCtor = vecType.getConstructor(double.class, double.class, double.class);
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            level = lvl;
            getEntitiesOfClass = entitiesOfClass;
            getBoundingBox = boundingBox;
            inflate = aabbInflate;
            clip = aabbClip;
            getLookAngle = lookAngle;
            getX = ex;
            getY = ey;
            getZ = ez;
            getEyeY = eyeY;
            vecGetX = vx;
            vecGetY = vy;
            vecGetZ = vz;
            vec3Ctor = vecCtor;
            vec3Class = vecType;
        }

        /** 1 = a block (placeable), 0 = anything else or an empty hand, -1 = cannot tell. */
        int blockState(Object inventory, int slot) {
            if (invGetItem == null) return -1;
            try {
                Object stack = invGetItem.invoke(inventory, slot);
                if (stack == null || (Boolean) stackIsEmpty.invoke(stack)) return 0;
                Object item = stackItem.invoke(stack);
                return item != null && blockItemClass.isInstance(item) ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** 1 = a sword is held, 0 = anything else or an empty hand, -1 = cannot tell. */
        int swordState(Object inventory, int slot) {
            if (invGetItem == null || registryGetKey == null || itemRegistry == null) return -1;
            try {
                Object stack = invGetItem.invoke(inventory, slot);
                if (stack == null || (Boolean) stackIsEmpty.invoke(stack)) return 0;
                Object item = stackItem.invoke(stack);
                if (item == null) return 0;
                Object id = registryGetKey.invoke(itemRegistry, item);
                return id != null && id.toString().endsWith("_sword") ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** 1 = a bed is held, 0 = anything else or an empty hand, -1 = cannot tell. */
        int bedState(Object inventory, int slot) {
            if (invGetItem == null || registryGetKey == null || itemRegistry == null) return -1;
            try {
                Object stack = invGetItem.invoke(inventory, slot);
                if (stack == null || (Boolean) stackIsEmpty.invoke(stack)) return 0;
                Object item = stackItem.invoke(stack);
                if (item == null) return 0;
                Object id = registryGetKey.invoke(itemRegistry, item);
                return id != null && id.toString().endsWith("_bed") ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** 1 = a living entity (mob or player) is in the crosshair, 0 = a block or nothing,
         *  -1 = cannot tell. The ray keeps going about one block past vanilla reach, so the
         *  gate opens as a player runs in rather than only once they are already in range. */
        int targetState(Object mc, Object self) {
            if (hitResult == null || hitEntity == null || livingClass == null) return -1;
            try {
                Object hit = hitResult.get(mc);
                double maxDist = TARGET_REACH;
                if (hit != null) {
                    if (entityHitClass.isInstance(hit)) {
                        Object entity = hitEntity.invoke(hit);
                        return entity != null && entity != self && livingClass.isInstance(entity)
                            ? 1 : 0;
                    }
                    if (blockHitClass != null && blockHitClass.isInstance(hit)) {
                        // A wall in front of the player ends the ray there: an entity behind it
                        // is not a target. A player in front of the wall is still found, because
                        // the ray only needs to reach them before the block.
                        if (hitLocation == null || vecGetX == null || vecGetY == null ||
                            vecGetZ == null || getX == null || getY == null || getZ == null ||
                            getEyeY == null) {
                            return 0;
                        }
                        Object loc = hitLocation.invoke(hit);
                        double dx = (Double) vecGetX.invoke(loc) - (Double) getX.invoke(self);
                        double dy = (Double) vecGetY.invoke(loc) - (Double) getEyeY.invoke(self);
                        double dz = (Double) vecGetZ.invoke(loc) - (Double) getZ.invoke(self);
                        double dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
                        if (dist < maxDist) maxDist = dist;
                    }
                }
                return extendedTarget(mc, self, maxDist);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        private int extendedTarget(Object mc, Object self, double maxDist) {
            if (level == null || getEntitiesOfClass == null || getBoundingBox == null ||
                inflate == null || clip == null || getLookAngle == null ||
                getX == null || getY == null || getZ == null || getEyeY == null ||
                vecGetX == null || vecGetY == null || vecGetZ == null ||
                vec3Ctor == null || vec3Class == null || maxDist <= 0.1) {
                return 0;
            }
            try {
                Object world = level.get(mc);
                if (world == null) return 0;
                Object ownBox = getBoundingBox.invoke(self);
                Object search = inflate.invoke(ownBox, TARGET_REACH + 1.0);
                Predicate<Object> notSelf = e -> e != self;
                List<?> entities = (List<?>) getEntitiesOfClass.invoke(
                    world, livingClass, search, notSelf);
                if (entities == null || entities.isEmpty()) return 0;

                double x = (Double) getX.invoke(self);
                double y = (Double) getEyeY.invoke(self);
                double z = (Double) getZ.invoke(self);
                Object look = getLookAngle.invoke(self);
                double dx = (Double) vecGetX.invoke(look);
                double dy = (Double) vecGetY.invoke(look);
                double dz = (Double) vecGetZ.invoke(look);
                Object eye = vec3Ctor.newInstance(x, y, z);
                Object end = vec3Ctor.newInstance(x + dx * maxDist,
                                                  y + dy * maxDist,
                                                  z + dz * maxDist);
                for (Object entity : entities) {
                    Object box = getBoundingBox.invoke(entity);
                    Object padded = inflate.invoke(box, TARGET_BOX_PAD);
                    Optional<?> hit = (Optional<?>) clip.invoke(padded, eye, end);
                    if (hit != null && hit.isPresent()) return 1;
                }
                return 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return 0;
            }
        }
    }
}
