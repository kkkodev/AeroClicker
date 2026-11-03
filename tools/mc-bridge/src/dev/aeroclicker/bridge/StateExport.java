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
 * sword=<0|1|-1> target=<0|1|-1> bed=<0|1|-1> look=<0|1|-1> water=<0|1|-1>
 * ground=<0|1|-1> hurt=<counter> hit=<counter> back=<counter> hground=<0|1|-1> hwater=<0|1|-1>}. The app treats a file whose timestamp is a
 * couple of seconds old as absent, so a crashed game cannot leave the clicker stuck. The
 * optional fields are last so an older app build still parses the line, and -1 means this build
 * cannot tell (the app then leaves the matching gate inert).</p>
 *
 * <p>The counters make the combat resets reliable: {@code hurt} goes up when the local
 * player's {@code hurtTime} rises from an attack (an entity dealt the damage), and {@code hit}
 * when a new swing starts while the exact crosshair target is a living entity (attacking one).
 * Fall, fire, lava and other environmental damage are deliberately left out: a jump reset on
 * fall damage is both useless and an obvious tell. When an older server protocol (1.8 through
 * ViaFabricPlus, or a server-side Via translation) sends no damage-event packet, the client only
 * sees the hurt animation (and the health update), so the attacker is inferred instead: an
 * arrow, snowball or egg that disappears next to the player, a living entity under the
 * crosshair, one that is still swinging, or the nearest one within melee reach. The app turns
 * those counters into one Jump Reset / Sprint Reset per step instead of firing on its own
 * clicks. {@code back} rises alongside {@code hurt} when the attacker was behind the player, so
 * the app can leave the jump reset alone after a backstab; the side is worked out the same way
 * for projectiles, from the direction they were flying in.</p>
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
     * The target gate keeps looking half a block past the vanilla entity reach, so a player
     * running in becomes a target just before they are close enough to hit. The extra reach is
     * what the app calls the target lead; it never changes the game's own reach.
     */
    private static final double TARGET_REACH = 3.5;
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
                             + " bed=" + snap.bed + " look=" + snap.look
                             + " water=" + snap.water + " ground=" + snap.ground
                             + " hurt=" + snap.hurt + " hit=" + snap.hit
                             + " back=" + snap.back + " seen=" + snap.seen
                             + " hground=" + snap.hground + " hwater=" + snap.hwater;
                long now = System.currentTimeMillis();
                if (!state.equals(last) || now - lastWrite >= HEARTBEAT_MS) {
                    // `ms` goes before the optional fields so an app build without them still
                    // parses slot and screen from the same line.
                    write(snap.line + " ms=" + now + " block=" + snap.block
                          + " sword=" + snap.sword + " target=" + snap.target
                          + " bed=" + snap.bed + " look=" + snap.look
                          + " water=" + snap.water + " ground=" + snap.ground
                          + " hurt=" + snap.hurt + " hit=" + snap.hit
                          + " back=" + snap.back + " seen=" + snap.seen
                          + " hground=" + snap.hground + " hwater=" + snap.hwater);
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
                          + " block=-1 sword=-1 target=-1 bed=-1 look=-1 water=-1 ground=-1"
                          + " hurt=-1 hit=-1 back=-1 seen=-1 hground=-1 hwater=-1");
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
            // Not in a world: nothing can be clicked, so report "GUI open" and no slot. The
            // counters are monotonic across worlds, so the app never replays an old event.
            b.prevGround = -1;
            b.prevWater = -1;
            return new Snapshot("slot=-1 screen=1", -1, -1, -1, -1, -1, -1, -1,
                                b.hurtCounter, b.seenCounter, b.hitCounter, b.backCounter,
                                b.hurtGround, b.hurtWater);
        }
        if (player != b.lastPlayer) {
            // A new world or respawn: hurtTime and the swing object belong to the old player.
            b.lastPlayer = player;
            b.lastHurtTime = -1;
            b.lastHealth = -1.0f;
            b.lastSwing = null;
            b.lastExactTarget = 0;
            b.lastProjectiles = new java.util.ArrayList<>();
            b.pendingProjectileSide = 0;
            b.pendingProjectilePolls = 0;
            b.prevGround = -1;
            b.prevWater = -1;
            b.hurtGround = -1;
            b.hurtWater = -1;
        }
        Object inventory = b.getInventory.invoke(player);
        int slot = (Integer) b.getSelectedSlot.invoke(inventory);
        String line = "slot=" + (slot >= 0 && slot < 9 ? slot : -1)
                    + " screen=" + (screenOpen(b, mc) ? 1 : 0);
        // Sampled before the counters: a projectile that vanished this poll is what names
        // the attacker of a hurt tick without a damage source.
        b.trackProjectiles(mc, player);
        b.sampleMotion(player);
        // Posture is sampled first so hurtState can latch it for the hit it is about to report.
        // The counters still have to be read before the snapshot, since the hurt counter is what
        // bumps the raw tick counter the two are compared on.
        final int ground = b.groundState(player);
        final int water = b.waterState(player);
        long hurt = b.hurtState(mc, player, ground, water);
        long hit = b.hitState(mc, player);
        return new Snapshot(line, b.blockState(inventory, slot),
                            b.swordState(inventory, slot), b.targetState(mc, player),
                            b.bedState(inventory, slot), b.lookState(mc),
                            water, ground,
                            hurt, b.seenCounter, hit, b.backCounter,
                            b.hurtGround, b.hurtWater);
    }

    /** Logical state without the timestamp: compared to decide when to rewrite the file. */
    private static final class Snapshot {
        final String line;
        final int block;
        final int sword;
        final int target;
        final int bed;
        final int look;
        final int water;
        final int ground;
        final int hground;
        final int hwater;
        final long hurt;
        final long seen;
        final long hit;
        final long back;

        Snapshot(String line, int block, int sword, int target, int bed, int look, int water,
                 int ground, long hurt, long seen, long hit, long back, int hground,
                 int hwater) {
            this.line = line;
            this.block = block;
            this.sword = sword;
            this.target = target;
            this.bed = bed;
            this.look = look;
            this.water = water;
            this.ground = ground;
            this.hground = hground;
            this.hwater = hwater;
            this.hurt = hurt;
            this.seen = seen;
            this.hit = hit;
            this.back = back;
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
        final Method hitType;
        final Class<?> entityHitClass;
        final Class<?> blockHitClass;
        final Class<?> livingClass;
        // Local-player state for the combat resets and the right-clicker block gate.
        final Field hurtTime;
        final Method isInWater;
        final Method onGround;
        // Posture from the sample before the newest hurt tick. The poll that detects a hit runs
        // up to 50 ms after it, and the knockback from that hit has usually lifted the player by
        // then, so reading the ground at that moment reports "mid-air" for grounded hits.
        int prevGround = -1;
        int prevWater = -1;
        int hurtGround = -1;
        int hurtWater = -1;
        final Method getCurrentSwing;
        // Client-side damage: handleDamageEvent stores the DamageSource (and rises hurtTime),
        // which is the only reliable "who hit me" on the client. getLastHurtByMob is only set
        // by the server-side hurtServer path, so it is always null out here.
        final Method getLastDamageSource;
        final Method damageEntity;
        final Method damageDirectEntity;
        // Legacy protocols (1.8 through ViaFabricPlus) have no damage-event packet, so the
        // client only sees the hurt animation. Health catches a damage tick even when that
        // animation is swallowed, and isSwinging names the attacker that is still swinging.
        final Method getHealth;
        final Method isSwinging;
        Object lastPlayer;
        int lastHurtTime = -1;
        float lastHealth = -1.0f;
        Object lastSwing;
        int lastExactTarget;
        long hurtCounter;
        long hitCounter;
        long backCounter;
        // Every damage tick seen, attributed or not: the app compares it with hurtCounter to
        // tell "the hit never reached the bridge" from "a filter dropped it".
        long seenCounter;
        // Attribution when the server names no attacker (a 1.8 protocol has no damage packet).
        final Method damageMsgId;
        final Method noArgDamageSource;
        final Method getDeltaMovement;
        final Method projectileOwner;
        final Class<?> entityClass;
        List<Track> lastProjectiles = new java.util.ArrayList<>();
        int pendingProjectileSide;
        int pendingProjectilePolls;
        // The player's own velocity: a hit knocks you away from the attacker even when the
        // server names nobody, which keeps 1.8 hits working at any range.
        double lastHorizontalSpeed = -1.0;
        int knockbackPolls;
        int knockbackSide;
        // Extended-reach check. Vanilla only puts an entity in hitResult within the normal
        // attack reach; this ray looks a little further so the gate opens as a player runs in.
        final Field level;
        final Method getEntitiesOfClass;
        final Method getBoundingBox;
        final Method inflate;
        final Method clip;
        final Method getLookAngle;
        final Method getX, getY, getZ, getEyeY;
        final Field vecX, vecY, vecZ;
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

            // Optional: the combat resets and look/water/ground gates report -1 (inert) when a
            // mapping is missing. `hurtTime` is a public field that rises when damage lands;
            // `getCurrentSwing` changes object identity for every new swing.
            Field hurtField = null;
            Method inWater = null, onGroundMethod = null, currentSwing = null;
            Method lastDamageSource = null, dmgEntity = null, dmgDirectEntity = null;
            Method healthMethod = null, swingingMethod = null;
            try {
                Class<?> livingType2 = Class.forName("net.minecraft.world.entity.LivingEntity");
                hurtField = livingType2.getField("hurtTime");
                currentSwing = livingType2.getMethod("getCurrentSwing");
                lastDamageSource = livingType2.getMethod("getLastDamageSource", int.class);
                Class<?> damageSource = Class.forName("net.minecraft.world.damagesource.DamageSource");
                dmgEntity = damageSource.getMethod("getEntity");
                dmgDirectEntity = damageSource.getMethod("getDirectEntity");
                // Optional legacy fallback, resolved separately so a rename cannot take the
                // modern damage-source path down with it.
                try { healthMethod = livingType2.getMethod("getHealth"); }
                catch (ReflectiveOperationException | RuntimeException ignored2) { }
                try { swingingMethod = livingType2.getMethod("isSwinging"); }
                catch (ReflectiveOperationException | RuntimeException ignored2) { }
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            try {
                Class<?> entityType = Class.forName("net.minecraft.world.entity.Entity");
                inWater = entityType.getMethod("isInWater");
                onGroundMethod = entityType.getMethod("onGround");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            hurtTime = hurtField;
            isInWater = inWater;
            onGround = onGroundMethod;
            getCurrentSwing = currentSwing;
            getLastDamageSource = lastDamageSource;
            damageEntity = dmgEntity;
            damageDirectEntity = dmgDirectEntity;
            getHealth = healthMethod;
            isSwinging = swingingMethod;

            // A source that names no attacker still carries a damage type string, which is
            // what keeps fall and fire out without an entity. getLastDamageSource(int) has a
            // no-arg form on some versions, so the fallback is resolved separately.
            Method msg = null;
            try {
                msg = Class.forName("net.minecraft.world.damagesource.DamageSource")
                           .getMethod("getMsgId");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            Method noArg = null;
            try {
                noArg = Class.forName("net.minecraft.world.entity.LivingEntity")
                              .getMethod("getLastDamageSource");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            damageMsgId = msg;
            noArgDamageSource = noArg;

            // Projectiles are sampled every poll: on a 1.8 server an arrow hit is only visible
            // as the arrow disappearing plus the hurt animation.
            Method delta = null;
            Class<?> entityType2 = null;
            try {
                entityType2 = Class.forName("net.minecraft.world.entity.Entity");
                delta = entityType2.getMethod("getDeltaMovement");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            Method owner = null;
            try {
                owner = Class.forName("net.minecraft.world.entity.projectile.Projectile")
                             .getMethod("getOwner");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            getDeltaMovement = delta;
            projectileOwner = owner;
            entityClass = entityType2;

            // Where a block hit landed, so the extended ray can stop at the wall instead of
            // reporting a player behind it.
            Method getLocation = null, getType = null;
            try {
                Class<?> hitBase = Class.forName("net.minecraft.world.phys.HitResult");
                getLocation = hitBase.getMethod("getLocation");
                getType = hitBase.getMethod("getType");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            hitLocation = getLocation;
            hitType = getType;

            // Optional extended reach, all null when the mapping is not what this version
            // calls it. The exact hitResult check above still works without it.
            Field lvl = null;
            Method entitiesOfClass = null, boundingBox = null, aabbInflate = null, aabbClip = null;
            Method lookAngle = null, ex = null, ey = null, ez = null, eyeY = null;
            Field vx = null, vy = null, vz = null;
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
                // Vec3 exposes x/y/z as public final fields in 26.x, not getters. Asking for
                // getX() used to throw here and leave the whole extended-reach block null,
                // which silently disabled the target lead and kept the gate at vanilla reach.
                vx = vecType.getField("x");
                vy = vecType.getField("y");
                vz = vecType.getField("z");
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
            vecX = vx;
            vecY = vy;
            vecZ = vz;
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

        /**
         * True only for a real block hit. Looking at the sky produces a BlockHitResult of type
         * MISS (a convenient carrier for the ray end point), which is still an instance of the
         * class but must not count as aiming at a block.
         */
        private boolean isBlockHit(Object hit) {
            if (hit == null) return false;
            if (hitType != null) {
                try {
                    Object type = hitType.invoke(hit);
                    if (type != null) return "BLOCK".equals(type.toString());
                } catch (ReflectiveOperationException | RuntimeException ignored) {
                }
            }
            return blockHitClass != null && blockHitClass.isInstance(hit);
        }

        /** 1 = the crosshair is on a block, 0 = air or an entity, -1 = cannot tell. */
        int lookState(Object mc) {
            if (hitResult == null || (blockHitClass == null && hitType == null)) return -1;
            try {
                return isBlockHit(hitResult.get(mc)) ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** 1 = the local player is in water, 0 = not, -1 = cannot tell. */
        int waterState(Object self) {
            if (isInWater == null) return -1;
            try {
                return (Boolean) isInWater.invoke(self) ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** 1 = the local player stands on the ground, 0 = airborne, -1 = cannot tell. */
        int groundState(Object self) {
            if (onGround == null) return -1;
            try {
                return (Boolean) onGround.invoke(self) ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** 1 only for an exact crosshair hit on a living entity, 0 otherwise, -1 unknown. */
        int exactTargetState(Object mc, Object self) {
            if (hitResult == null || hitEntity == null || entityHitClass == null ||
                livingClass == null) {
                return -1;
            }
            try {
                Object hit = hitResult.get(mc);
                if (hit == null || !entityHitClass.isInstance(hit)) return 0;
                Object entity = hitEntity.invoke(hit);
                return entity != null && entity != self && livingClass.isInstance(entity) ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /**
         * Counter that rises once each time the local player is hurt by an entity.
         *
         * <p>Modern versions raise {@code hurtTime} inside {@code handleDamageEvent}, which
         * also stores the damage source. A 1.8 server reached through ViaFabricPlus has no
         * damage-event packet: the client only gets the hurt animation. Health is tracked as
         * well so a damage tick is still seen when the animation is missed, and the attacker is
         * inferred by classifyHit (projectiles that vanished nearby, the crosshair target, a
         * swinging entity, melee reach).</p>
         */
        long hurtState(Object mc, Object self, int nowGround, int nowWater) {
            if (hurtTime == null) return -1;
            try {
                int now = hurtTime.getInt(self);
                float health = healthOf(self);
                if (lastHurtTime < 0) {
                    // First look at this player: remember the value, do not replay old damage.
                    lastHurtTime = now;
                } else {
                    boolean hurtRose = now > lastHurtTime;
                    // Health updates can arrive a poll before or after the animation. A drop
                    // is only a damage tick when it is more than float noise.
                    boolean healthFell = health >= 0.0f && lastHealth >= 0.0f &&
                                         health < lastHealth - 0.01f;
                    if (hurtRose || healthFell) {
                        seenCounter++;
                        // Fall, fire, lava, drowning and starvation raise hurtTime too, but no
                        // entity dealt them. classifyHit names the attacker and its side, or
                        // returns none, so those ticks never bump either counter.
                        int side = classifyHit(mc, self, !hurtRose);
                        if (side != HIT_NONE) {
                            hurtCounter++;
                            // Posture before the knockback: this poll is already post-hit, so
                            // the previous sample is the one that saw the player as they were
                            // when the damage landed. A missing previous sample falls back to
                            // this one rather than leaving the hit unclassified.
                            hurtGround = prevGround >= 0 ? prevGround : nowGround;
                            hurtWater = prevWater >= 0 ? prevWater : nowWater;
                            if (side == HIT_BEHIND) backCounter++;
                        }
                    }
                    lastHurtTime = now;
                }
                lastHealth = health;
                prevGround = nowGround;
                prevWater = nowWater;
                return hurtCounter;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** Current health, or -1 when this mapping does not expose it. */
        private float healthOf(Object self) {
            if (getHealth == null) return -1.0f;
            try {
                return (Float) getHealth.invoke(self);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1.0f;
            }
        }

        /** True for a living entity that has not died yet. */
        private boolean isAlive(Object entity) {
            if (getHealth == null) return true;
            try {
                return (Float) getHealth.invoke(entity) > 0.0f;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return true;
            }
        }

        /** Side of a hit: no entity to blame, in front of the player, or behind the player. */
        private static final int HIT_NONE = 0;
        private static final int HIT_FRONT = 1;
        private static final int HIT_BEHIND = 2;

        /**
         * Damage types that no jump reset should answer. Only used when a source names no
         * attacker: modern sources usually name one, so this is what keeps fall, fire and lava
         * quiet when the server sends a damage type without an entity.
         */
        private static final java.util.Set<String> ENVIRONMENTAL = java.util.Set.of(
            "fall", "lava", "inFire", "onFire", "hotFloor", "drown", "starve", "cactus",
            "outOfWorld", "flyIntoWall", "freeze", "sweetBerryBush", "stalagmite", "anvil",
            "fallingBlock", "fallingAnvil", "inWall", "cramming", "outsideBorder");

        /**
         * Names the attacker and the side the hit came from: 0 when nothing caused it, 1 when
         * the attacker is in front, 2 when it is behind. The side matters because a jump reset
         * after a backstab only hands the attacker a cleaner follow-up.
         *
         * <p>A damage source that names an entity is trusted completely. When it names none,
         * the source is only believed if its damage type is a known environmental one;
         * anything else (a 1.8 packet translated by ViaFabricPlus carries a source with no
         * attacker) falls through to the local evidence in {@link #classifyLocally}.</p>
         */
        private int classifyHit(Object mc, Object self, boolean strict) {
            int fromSource = classifyFromSource(self);
            if (fromSource != -1) return fromSource;
            return classifyLocally(mc, self, strict);
        }

        /** -1 = the source says nothing about an attacker, 0 = environmental, 1/2 = front/back. */
        private int classifyFromSource(Object self) {
            Object source = damageSource(self);
            if (source == null) return -1;
            Object attacker = entityOf(source, damageEntity);
            if (attacker != null && attacker != self) return sideOf(attacker, self);
            Object direct = entityOf(source, damageDirectEntity);
            if (direct != null && direct != self) return sideOf(direct, self);
            return environmental(source) ? HIT_NONE : -1;
        }

        /** The damage source for the hit that just landed, through either signature. */
        private Object damageSource(Object self) {
            if (getLastDamageSource == null) return null;
            try {
                return getLastDamageSource.invoke(self, 5);
            } catch (ReflectiveOperationException | RuntimeException e) {
                if (noArgDamageSource == null) return null;
                try {
                    return noArgDamageSource.invoke(self);
                } catch (ReflectiveOperationException | RuntimeException ignored) {
                    return null;
                }
            }
        }

        private Object entityOf(Object source, Method getter) {
            if (getter == null) return null;
            try {
                return getter.invoke(source);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return null;
            }
        }

        private boolean environmental(Object source) {
            if (damageMsgId == null || source == null) return false;
            try {
                Object id = damageMsgId.invoke(source);
                return id != null && ENVIRONMENTAL.contains(id.toString());
            } catch (ReflectiveOperationException | RuntimeException e) {
                return false;
            }
        }

        /**
         * Best guess at what hurt the player when the server sent no attacker. The cues are
         * ordered from strongest to weakest, and every one of them also says which side the
         * hit came from:
         *
         * <ol>
         *   <li>a projectile that was next to the player last poll and is gone now - arrows,
         *       snowballs and eggs are removed the frame they land on an entity, so this is
         *       what a bow or snowball hit looks like on a 1.8 server;</li>
         *   <li>a living entity under the crosshair - you can see who hit you;</li>
         *   <li>a living entity that is still swinging;</li>
         *   <li>the nearest living entity within melee reach plus a little lag.</li>
         * </ol>
         *
         * <p>Nothing found means environmental damage, so the counter does not rise and the
         * module stays quiet. A health-only tick (the hurt animation was missed) needs a swing
         * or a hit under the crosshair, which keeps starvation ticks out.</p>
         */
        private int classifyLocally(Object mc, Object self, boolean strict) {
            if (pendingProjectilePolls > 0) {
                int side = pendingProjectileSide;
                pendingProjectilePolls = 0;
                pendingProjectileSide = HIT_NONE;
                if (side != HIT_NONE) return side;
            }
            if (exactTargetState(mc, self) == 1) return HIT_FRONT;

            List<?> entities = nearbyLiving(mc, self);
            if (entities == null || entities.isEmpty()) return HIT_NONE;
            Object nearest = null;
            double nearestDist = Double.MAX_VALUE;
            Object swinger = null;
            double swingerDist = Double.MAX_VALUE;
            for (Object entity : entities) {
                double dist = horizontalDistance(entity, self);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearest = entity;
                }
                if (dist <= 6.0 && swinging(entity) && dist < swingerDist) {
                    swingerDist = dist;
                    swinger = entity;
                }
            }
            if (swinger != null) return sideOf(swinger, self);
            // A hit also pushes the player away from the attacker, which is the one cue that
            // still works when nothing identifiable is left nearby.
            if (knockbackPolls > 0) {
                knockbackPolls = 0;
                return knockbackSide;
            }
            // Swings are short; when this mapping cannot read them at all, reach is the only
            // signal left, so it is allowed the full radius.
            double reach = (strict && isSwinging != null) ? 3.0 : 4.5;
            if (nearest != null && nearestDist <= reach) return sideOf(nearest, self);
            return HIT_NONE;
        }

        /** Living entities close enough to have hit the player, or null when unreadable. */
        private List<?> nearbyLiving(Object mc, Object self) {
            if (level == null || getEntitiesOfClass == null || getBoundingBox == null ||
                inflate == null || livingClass == null || entityClass == null) {
                return null;
            }
            try {
                Object world = level.get(mc);
                if (world == null) return null;
                Object search = inflate.invoke(getBoundingBox.invoke(self), 6.0);
                Predicate<Object> wanted = e -> e != self && livingClass.isInstance(e) &&
                                                isAlive(e);
                return (List<?>) getEntitiesOfClass.invoke(world, entityClass, search, wanted);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return null;
            }
        }

        private double horizontalDistance(Object other, Object self) {
            if (getX == null || getZ == null) return Double.MAX_VALUE;
            try {
                double dx = (Double) getX.invoke(other) - (Double) getX.invoke(self);
                double dz = (Double) getZ.invoke(other) - (Double) getZ.invoke(self);
                return Math.sqrt(dx * dx + dz * dz);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return Double.MAX_VALUE;
            }
        }

        private boolean swinging(Object entity) {
            if (isSwinging == null) return false;
            try {
                return (Boolean) isSwinging.invoke(entity);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return false;
            }
        }

        /**
         * 1 in front, 2 behind. A living attacker is judged by where it stands; a projectile by
         * the direction it was flying, because one that came from behind travels roughly the
         * way the player looks instead of toward them.
         */
        private int sideOf(Object attacker, Object self) {
            if (livingClass != null && livingClass.isInstance(attacker)) {
                return dotTo(attacker, self) < -0.25 ? HIT_BEHIND : HIT_FRONT;
            }
            return projectileBehind(attacker, self) ? HIT_BEHIND : HIT_FRONT;
        }

        private double dotTo(Object other, Object self) {
            if (getX == null || getZ == null) return 0.0;
            try {
                double dx = (Double) getX.invoke(other) - (Double) getX.invoke(self);
                double dz = (Double) getZ.invoke(other) - (Double) getZ.invoke(self);
                return horizontalDot(dx, dz, self);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return 0.0;
            }
        }

        /** Cosine of the angle between (dx, dz) and the player's horizontal look vector. */
        private double horizontalDot(double dx, double dz, Object self) {
            if (getLookAngle == null || vecX == null || vecZ == null) return 0.0;
            try {
                Object look = getLookAngle.invoke(self);
                double lx = vecX.getDouble(look);
                double lz = vecZ.getDouble(look);
                double ld = Math.sqrt(lx * lx + lz * lz);
                double d = Math.sqrt(dx * dx + dz * dz);
                if (ld < 1.0e-4 || d < 1.0e-4) return 0.0;
                return (dx * lx + dz * lz) / (d * ld);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return 0.0;
            }
        }

        private boolean projectileBehind(Object projectile, Object self) {
            double[] motion = motionOf(projectile);
            if (motion != null && motion[0] * motion[0] + motion[2] * motion[2] > 1.0e-6) {
                return horizontalDot(motion[0], motion[2], self) > 0.25;
            }
            return dotTo(projectile, self) < -0.25;
        }

        private double[] motionOf(Object entity) {
            if (getDeltaMovement == null || vecX == null || vecY == null || vecZ == null) {
                return null;
            }
            try {
                Object motion = getDeltaMovement.invoke(entity);
                if (motion == null) return null;
                return new double[] { vecX.getDouble(motion), vecY.getDouble(motion),
                                      vecZ.getDouble(motion) };
            } catch (ReflectiveOperationException | RuntimeException e) {
                return null;
            }
        }

        /**
         * Samples the arrows, snowballs and eggs around the player every poll. A projectile
         * that was within a couple of blocks last poll and is gone now was removed because it
         * landed on an entity - and when a hurt tick arrives in the same poll, that entity was
         * the player. Nothing is counted here; the hit still has to arrive.
         */
        void trackProjectiles(Object mc, Object self) {
            if (getEntitiesOfClass == null || entityClass == null || level == null ||
                getBoundingBox == null || inflate == null || getX == null || getZ == null) {
                return;
            }
            try {
                Object world = level.get(mc);
                if (world == null) return;
                Object search = inflate.invoke(getBoundingBox.invoke(self), 3.0);
                Predicate<Object> wanted = e -> e != self && isProjectile(e) &&
                                                !ownedByPlayer(e, self);
                List<?> found = (List<?>) getEntitiesOfClass.invoke(
                    world, entityClass, search, wanted);
                List<Track> now = new java.util.ArrayList<>();
                if (found != null) {
                    for (Object entity : found) {
                        now.add(new Track(entity, horizontalDistance(entity, self),
                                          projectileBehind(entity, self)));
                    }
                }
                // A hit can land either side of the sample, so the flag survives one poll.
                if (pendingProjectilePolls > 0) pendingProjectilePolls--;
                for (Track was : lastProjectiles) {
                    if (was.distance > 2.5 || present(now, was.entity)) continue;
                    pendingProjectileSide = was.behind ? HIT_BEHIND : HIT_FRONT;
                    pendingProjectilePolls = 2;
                    break;
                }
                lastProjectiles = now;
            } catch (ReflectiveOperationException | RuntimeException ignored) {
                // Missing a sample only costs projectile recognition for one poll.
            }
        }

        /**
         * Samples the player's own velocity to catch the knockback a hit applies. Walking keeps
         * its speed from tick to tick, so only a spike counts. The push points away from the
         * attacker, so its horizontal direction is also a side signal: pushed roughly the way
         * you look means the hit came from behind.
         */
        void sampleMotion(Object self) {
            double[] motion = motionOf(self);
            if (motion == null) return;
            double speed = Math.sqrt(motion[0] * motion[0] + motion[2] * motion[2]);
            if (lastHorizontalSpeed >= 0.0 && speed > 0.20 &&
                speed - lastHorizontalSpeed > 0.12) {
                knockbackSide = horizontalDot(motion[0], motion[2], self) > 0.25
                                ? HIT_BEHIND : HIT_FRONT;
                knockbackPolls = 2;
            }
            lastHorizontalSpeed = speed;
            if (knockbackPolls > 0) knockbackPolls--;
        }

        private static boolean present(List<Track> tracks, Object entity) {
            for (Track t : tracks) {
                if (t.entity == entity) return true;
            }
            return false;
        }

        /** An arrow, spectral arrow, trident, snowball or egg, whatever the mapping calls it. */
        private static boolean isProjectile(Object entity) {
            for (Class<?> c = entity.getClass(); c != null; c = c.getSuperclass()) {
                String name = c.getName();
                if (name.endsWith(".Arrow") || name.endsWith("ArrowEntity") ||
                    name.endsWith(".ThrownTrident") || name.endsWith("TridentEntity") ||
                    name.endsWith(".Snowball") || name.endsWith("SnowballEntity") ||
                    name.endsWith(".ThrownEgg") || name.endsWith("EggEntity")) {
                    return true;
                }
            }
            return false;
        }

        /** Your own arrow leaving the bow must not be mistaken for a hit on you. */
        private boolean ownedByPlayer(Object projectile, Object self) {
            if (projectileOwner == null) return false;
            try {
                return projectileOwner.invoke(projectile) == self;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return false;
            }
        }

        /** A projectile remembered between polls: enough to place its last flight direction. */
        private static final class Track {
            final Object entity;
            final double distance;
            final boolean behind;

            Track(Object entity, double distance, boolean behind) {
                this.entity = entity;
                this.distance = distance;
                this.behind = behind;
            }
        }

        /** Counter that rises once for each new swing that starts on a living entity. */
        long hitState(Object mc, Object self) {
            if (getCurrentSwing == null) return -1;
            try {
                Object swing = getCurrentSwing.invoke(self);
                int target = exactTargetState(mc, self);
                if (swing != null && swing != lastSwing &&
                    (target == 1 || lastExactTarget == 1)) {
                    // A killing blow can remove the entity from hitResult before the next
                    // sample; the previous sample still proves the swing started on it.
                    hitCounter++;
                }
                lastSwing = swing;
                lastExactTarget = target;
                return hitCounter;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /** True for any fireball (large, small or dragon) regardless of the mapping's class
         *  name: Yarn calls it FireballEntity, official mappings call it Fireball. */
        private static boolean isFireball(Object e) {
            for (Class<?> c = e.getClass(); c != null; c = c.getSuperclass()) {
                String n = c.getName();
                if (n.endsWith(".Fireball") || n.endsWith("FireballEntity")) return true;
            }
            return false;
        }

        /** Bitmask of what the gate can see: 1 = a mob/player, 2 = a fireball, 3 = both,
         *  0 = a block or nothing, -1 = cannot tell. Keeping the two apart lets the app's
         *  "Click at fireballs" option decide whether fireballs count. The ray keeps going
         *  half a block past vanilla reach, so the gate opens as a target runs in. */
        int targetState(Object mc, Object self) {
            if (hitResult == null || hitEntity == null || livingClass == null) return -1;
            try {
                Object hit = hitResult.get(mc);
                double maxDist = TARGET_REACH;
                if (hit != null) {
                    if (entityHitClass.isInstance(hit)) {
                        Object entity = hitEntity.invoke(hit);
                        if (entity == null || entity == self) return 0;
                        if (livingClass.isInstance(entity)) return 1;
                        return isFireball(entity) ? 2 : 0;
                    }
                    if (isBlockHit(hit)) {
                        // A wall in front of the player ends the ray there: an entity behind it
                        // is not a target. A player in front of the wall is still found, because
                        // the ray only needs to reach them before the block. A MISS result is
                        // not a wall; it must not shorten the lead ray.
                        if (hitLocation == null || vecX == null || vecY == null ||
                            vecZ == null || getX == null || getY == null || getZ == null ||
                            getEyeY == null) {
                            return 0;
                        }
                        Object loc = hitLocation.invoke(hit);
                        double dx = vecX.getDouble(loc) - (Double) getX.invoke(self);
                        double dy = vecY.getDouble(loc) - (Double) getEyeY.invoke(self);
                        double dz = vecZ.getDouble(loc) - (Double) getZ.invoke(self);
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
                vecX == null || vecY == null || vecZ == null ||
                vec3Ctor == null || vec3Class == null || maxDist <= 0.1) {
                return 0;
            }
            try {
                Object world = level.get(mc);
                if (world == null) return 0;
                Object ownBox = getBoundingBox.invoke(self);
                Object search = inflate.invoke(ownBox, TARGET_REACH + 1.0);
                // Search Entity (LivingEntity's superclass) and filter here: a LivingEntity
                // class filter would drop fireballs, which the app may or may not count.
                Class<?> entityClass = livingClass.getSuperclass();
                if (entityClass == null) return 0;
                Predicate<Object> wanted = e -> e != self &&
                    (livingClass.isInstance(e) || isFireball(e));
                List<?> entities = (List<?>) getEntitiesOfClass.invoke(
                    world, entityClass, search, wanted);
                if (entities == null || entities.isEmpty()) return 0;

                double x = (Double) getX.invoke(self);
                double y = (Double) getEyeY.invoke(self);
                double z = (Double) getZ.invoke(self);
                Object look = getLookAngle.invoke(self);
                double dx = vecX.getDouble(look);
                double dy = vecY.getDouble(look);
                double dz = vecZ.getDouble(look);
                Object eye = vec3Ctor.newInstance(x, y, z);
                Object end = vec3Ctor.newInstance(x + dx * maxDist,
                                                  y + dy * maxDist,
                                                  z + dz * maxDist);
                boolean living = false, fireball = false;
                for (Object entity : entities) {
                    Object box = getBoundingBox.invoke(entity);
                    Object padded = inflate.invoke(box, TARGET_BOX_PAD);
                    Optional<?> hit = (Optional<?>) clip.invoke(padded, eye, end);
                    if (hit != null && hit.isPresent()) {
                        if (livingClass.isInstance(entity)) living = true;
                        if (isFireball(entity)) fireball = true;
                    }
                }
                return (living ? 1 : 0) | (fireball ? 2 : 0);
            } catch (ReflectiveOperationException | RuntimeException e) {
                return 0;
            }
        }
    }
}
