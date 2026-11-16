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
 * held item is a block, whether it is a sword or a bed, whether a living entity (a mob or
 * another player) is under or just beside the crosshair, and whether the crosshair block is
 * interactive (opens a menu, a door, a bed, ...) instead of accepting a placed block.
 * It only ever reads; nothing is sent to the server and no input is changed.
 *
 * <p>The state goes to {@code $XDG_RUNTIME_DIR/aeroclicker-mc.state} as one line:
 * {@code slot=<0-8 or -1> screen=<0|1> ms=<epoch milliseconds> block=<0|1|-1>
 * sword=<0|1|-1> target=<0|1|-1> bed=<0|1|-1> look=<0|1|-1> water=<0|1|-1>
 * ground=<0|1|-1> hurt=<counter> hit=<counter> back=<counter> hground=<0|1|-1> hwater=<0|1|-1>
 * focus=<0|1|-1> interactive=<0|1|-1>}. The app treats a file whose timestamp is a
 * couple of seconds old as absent, so a crashed game cannot leave the clicker stuck. The
 * optional fields are last so an older app build still parses the line, and -1 means this build
 * cannot tell (the app then leaves the matching gate inert). {@code focus} says whether the
 * game window owns the input focus, which is what the "only click inside Minecraft" option
 * uses to stay quiet in another window. {@code interactive} says whether the block under the
 * crosshair would react to a use (a menu block such as a chest, or a door, trapdoor, bed,
 * button, ...), so the right clicker can stay off it instead of interacting. {@code target}
 * reaches half a block past vanilla attack reach and accepts a ray that passes just beside a
 * player, while {@code look} extends vanilla's block ray by half a block; both give their
 * clicker an early start at the cost of the odd miss.</p>
 *
 * <p>The counters make the combat resets reliable: {@code hurt} goes up when the local
 * player's {@code hurtTime} rises from an attack (an entity dealt the damage), and {@code hit}
 * when a new swing starts while the exact crosshair target is a living entity (attacking one).
 * Fall, fire, lava and other environmental damage are deliberately left out: a jump reset on
 * fall damage is both useless and an obvious tell. When an older server protocol (1.8 through
 * ViaFabricPlus, or a server-side Via translation) sends no damage-event packet, the client only
 * sees the hurt animation (and the health update), so the attacker is inferred instead: an
 * arrow, snowball or egg that disappears next to the player, or a living entity that is still
 * swinging. A crosshair target or a nearby entity alone is not enough, because fall and burn
 * ticks happen during fights too and must not look like hits. The app turns
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

    /**
     * The loop wakes often enough to catch a damage tick promptly, but the expensive part (the
     * crosshair rays, the entity scans and the item checks) still runs only every fifth pass.
     * Reading hurtTime and health is two reflection calls, so the fast pass costs almost
     * nothing and removes up to 40 ms from a reset's reaction time.
     */
    private static final long FAST_INTERVAL_MS = 10;
    private static final int FULL_EVERY = 5;
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
    /**
     * How far beside a player the cursor may be and still open the target gate. The click is
     * then aimed next to the player instead of at them, so it can miss on purpose: moving onto
     * a player fades the clicker in rather than switching it on at the exact frame the crosshair
     * touches the box, which is what a real player's aim looks like.
     */
    private static final double TARGET_NEAR_PAD = 0.25;
    /**
     * The look gate extends vanilla's block ray by half a block, so the right clicker starts
     * when a block is still just out of reach. This is the block-side counterpart of the target
     * lead: the gate opens a little early and the first click may miss, exactly as the left
     * clicker's clicks can miss a player who is still running in. The game's own reach is not
     * changed.
     */
    private static final double BLOCK_LEAD = 0.5;
    /**
     * Block classes whose right click is an interaction instead of a placement. Names are the
     * lower-cased simple names, matched against the whole superclass chain so modded doors and
     * similar (which extend the vanilla classes) are covered too. Menu blocks are found
     * separately through the game's own {@code getMenuProvider} check.
     */
    private static final String[] INTERACTIVE_BLOCK_CLASSES = {
        "doorblock", "trapdoorblock", "fencegateblock", "bedblock", "buttonblock", "leverblock",
        "signblock", "noteblock", "jukeboxblock", "cakeblock", "candlecakeblock", "candleblock",
        "abstractcauldronblock", "composterblock", "flowerpotblock", "respawnanchorblock",
        "bellblock", "diodeblock", "chiseledbookshelfblock", "decoratedpotblock", "campfireblock",
    };

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
        Snapshot lastSnap = null;
        long lastHurt = -2;
        long lastHit = -2;
        int tick = 0;
        while (true) {
            try {
                if (binding == null) binding = new Binding();
                Object mc = binding.getInstance.invoke(null);
                Object player = mc == null ? null : binding.player.get(mc);

                // Fast pass: only the two counters that drive the combat resets. The snapshot
                // below has already run once, so lastPlayer/lastHurtTime/lastHealth are seeded.
                if (player != null && player == binding.lastPlayer) {
                    binding.sampleMotion(player);
                    int ground = binding.groundState(player);
                    int water = binding.waterState(player);
                    long hurt = binding.hurtState(mc, player, ground, water);
                    long hit = binding.hitState(mc, player);
                    long back = binding.backCounter;
                    long seen = binding.seenCounter;
                    int hground = binding.hurtGround;
                    int hwater = binding.hurtWater;
                    boolean changed = (hurt >= 0 && hurt != lastHurt) ||
                                      (hit >= 0 && hit != lastHit);
                    if (changed && lastSnap != null) {
                        long outHurt = hurt >= 0 ? hurt : lastHurt;
                        long outHit = hit >= 0 ? hit : lastHit;
                        long now = System.currentTimeMillis();
                        write(wire(lastSnap, now, outHurt, outHit, back, seen, hground, hwater));
                        last = stateString(lastSnap, outHurt, outHit, back, seen, hground, hwater);
                        lastWrite = now;
                    }
                    if (hurt >= 0) lastHurt = hurt;
                    if (hit >= 0) lastHit = hit;
                }

                // Full pass: the gates and the item/window state. This is the part that costs
                // real work, so it keeps the old 50 ms cadence.
                if (tick++ % FULL_EVERY == 0) {
                    Snapshot snap = snapshot(binding);
                    lastSnap = snap;
                    if (snap.hurt >= 0) lastHurt = snap.hurt;
                    if (snap.hit >= 0) lastHit = snap.hit;
                    String state = stateString(snap, snap.hurt, snap.hit, snap.back,
                                               snap.seen, snap.hground, snap.hwater);
                    long now = System.currentTimeMillis();
                    if (!state.equals(last) || now - lastWrite >= HEARTBEAT_MS) {
                        // `ms` goes before the optional fields so an app build without them
                        // still parses slot and screen from the same line.
                        write(wire(snap, now, snap.hurt, snap.hit, snap.back,
                                   snap.seen, snap.hground, snap.hwater));
                        last = state;
                        lastWrite = now;
                    }
                }
                Thread.sleep(FAST_INTERVAL_MS);
            } catch (InterruptedException e) {
                return;
            } catch (Throwable t) {
                // Never let a transient client state (a level being torn down, a resource
                // reload) kill the bridge. "Unknown" reads as "pause" on the app's side.
                try {
                    write("slot=-1 screen=1 ms=" + System.currentTimeMillis()
                          + " block=-1 sword=-1 target=-1 bed=-1 look=-1 water=-1 ground=-1"
                          + " hurt=-1 hit=-1 back=-1 seen=-1 hground=-1 hwater=-1 focus=-1 interactive=-1");
                    Thread.sleep(HEARTBEAT_MS);
                } catch (Throwable ignored) {
                    return;
                }
            }
        }
    }

    /** Everything except the timestamp: compared between passes to avoid a needless write. */
    private static String stateString(Snapshot s, long hurt, long hit, long back, long seen,
                                      int hground, int hwater) {
        return s.line + " block=" + s.block
             + " sword=" + s.sword + " target=" + s.target
             + " bed=" + s.bed + " look=" + s.look
             + " water=" + s.water + " ground=" + s.ground
             + " hurt=" + hurt + " hit=" + hit
             + " back=" + back + " seen=" + seen
             + " hground=" + hground + " hwater=" + hwater
             + " focus=" + s.focus + " interactive=" + s.interactive;
    }

    /** The same fields with the timestamp inserted right after slot/screen. */
    private static String wire(Snapshot s, long now, long hurt, long hit, long back, long seen,
                               int hground, int hwater) {
        String body = stateString(s, hurt, hit, back, seen, hground, hwater);
        return s.line + " ms=" + now + body.substring(s.line.length());
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
                                b.hurtGround, b.hurtWater, b.focusState(mc), b.interactiveState(mc, player));
        }
        if (player != b.lastPlayer) {
            // A new world or respawn: hurtTime and the swing object belong to the old player.
            b.lastPlayer = player;
            b.lastHurtTime = -1;
            b.lastHealth = -1.0f;
            b.lastHurtRiseMs = -1_000_000L;
            b.lastHealthDropMs = -1_000_000L;
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
                            b.bedState(inventory, slot), b.lookState(mc, player),
                            water, ground,
                            hurt, b.seenCounter, hit, b.backCounter,
                            b.hurtGround, b.hurtWater, b.focusState(mc), b.interactiveState(mc, player));
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
        final int focus;
        final int interactive;
        final long hurt;
        final long seen;
        final long hit;
        final long back;

        Snapshot(String line, int block, int sword, int target, int bed, int look, int water,
                 int ground, long hurt, long seen, long hit, long back, int hground,
                 int hwater, int focus, int interactive) {
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
            this.focus = focus;
            this.interactive = interactive;
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

    /** First public method named {@code name} whose parameters accept the given types. */
    private static Method findMethod(Class<?> type, String name, Class<?>... args) {
        for (Method m : type.getMethods()) {
            if (!m.getName().equals(name)) continue;
            Class<?>[] p = m.getParameterTypes();
            if (p.length != args.length) continue;
            boolean ok = true;
            for (int i = 0; i < p.length; i++) {
                if (args[i] != null && !p[i].isAssignableFrom(args[i])) { ok = false; break; }
            }
            if (ok) return m;
        }
        return null;
    }

    /** True when the block class or one of its superclasses is a right-click interaction. */
    private static boolean isInteractiveBlock(Object block) {
        for (Class<?> c = block.getClass(); c != null; c = c.getSuperclass()) {
            String name = c.getSimpleName().toLowerCase(java.util.Locale.ROOT);
            for (String known : INTERACTIVE_BLOCK_CLASSES)
                if (known.equals(name)) return true;
        }
        return false;
    }

    /**
     * A public static constant by name, whether the type models it as an enum or as a field
     * (newer mappings turn some of the old enums into holders).
     */
    private static Object constant(Class<?> type, String name) {
        if (type.isEnum()) {
            for (Object c : type.getEnumConstants())
                if (((Enum<?>) c).name().equals(name)) return c;
            return null;
        }
        try {
            return type.getField(name).get(null);
        } catch (ReflectiveOperationException | RuntimeException e) {
            return null;
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
        // Window focus for the app's "only click inside Minecraft" option. Both mappings are
        // optional: a missing one leaves focus at -1, which the app draws as unsupported.
        final Method isWindowActive;
        final Field windowField;
        final Method windowFocused;
        // Interactive-block gate: whether the crosshair block reacts to a use instead of
        // taking a placed block. All optional; a mapping without them reports -1 and the
        // app leaves the gate inert.
        final Method hitBlockPos;
        final Method levelBlockState;
        final Method stateMenuProvider;
        final Method stateBlockGetter;
        final Method blockMenuProvider;
        final Method isShiftKeyDown;
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
        // One damage tick can surface as a hurtTime rise and a health drop in different polls.
        // Remember when each signal was last seen so the second one is not counted twice.
        long lastHurtRiseMs = -1_000_000L;
        long lastHealthDropMs = -1_000_000L;
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
        // Look-gate lead: the block ray extended by BLOCK_LEAD through the game's own clip call.
        // Optional; when this mapping lacks it the look gate stays at vanilla reach.
        final Method levelClip;
        final java.lang.reflect.Constructor<?> clipCtor;
        final Object clipBlockShape;
        final Object clipFluidShape;
        final Method clipCollisionOf;
        final Method clipCollisionEmpty;
        final Field crosshairEntity;

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

            // Focus is optional in both directions: some mappings expose a ready-made
            // isWindowActive(), others only the Window field. Either one works, and neither
            // being available just leaves the option unsupported.
            Method active = null;
            try {
                active = mc.getMethod("isWindowActive");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            Field window = null;
            Method focused = null;
            try {
                Field w = mc.getDeclaredField("window");
                w.setAccessible(true);
                Method f = w.getType().getMethod("isFocused");
                window = w;
                focused = f;
            } catch (ReflectiveOperationException | RuntimeException ignored) {
                window = null;
                focused = null;
            }
            isWindowActive = active;
            windowField = window;
            windowFocused = focused;

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

            // Look-gate lead. Vanilla stops its block ray at the block-interaction range, so a
            // block just out of reach reports a MISS. Clipping the same ray BLOCK_LEAD further
            // through the game tests exactly that block. ClipContext has had both an Entity and
            // a CollisionContext as its last argument across versions, so the constructor is
            // picked by shape and the last argument is built to match.
            Method worldClip = null;
            java.lang.reflect.Constructor<?> clipContextCtor = null;
            Object blockShape = null, fluidShape = null;
            Method collisionOf = null, collisionEmpty = null;
            Field crosshair = null;
            if (vecType != null) {
                try {
                    Class<?> clipContext = Class.forName("net.minecraft.world.level.ClipContext");
                    worldClip = Class.forName("net.minecraft.world.level.Level")
                                     .getMethod("clip", clipContext);
                    for (java.lang.reflect.Constructor<?> ctor : clipContext.getConstructors()) {
                        Class<?>[] p = ctor.getParameterTypes();
                        if (p.length != 5 || p[0] != vecType || p[1] != vecType) continue;
                        Object block = constant(p[2], "OUTLINE");
                        if (block == null) block = constant(p[2], "COLLIDER");
                        Object fluid = constant(p[3], "NONE");
                        if (block == null || fluid == null) continue;
                        clipContextCtor = ctor;
                        blockShape = block;
                        fluidShape = fluid;
                        break;
                    }
                    // Newer versions pass the collision context instead of the entity, so
                    // resolve both ways and let the constructor's last parameter decide.
                    try {
                        Class<?> collision =
                            Class.forName("net.minecraft.world.phys.shapes.CollisionContext");
                        try {
                            collisionOf = collision.getMethod(
                                "of", Class.forName("net.minecraft.world.entity.Entity"));
                        } catch (ReflectiveOperationException | RuntimeException ignored) {
                        }
                        try {
                            collisionEmpty = collision.getMethod("empty");
                        } catch (ReflectiveOperationException | RuntimeException ignored) {
                        }
                    } catch (ReflectiveOperationException | RuntimeException ignored) {
                    }
                    // Vanilla keeps the entity under the crosshair separately from hitResult,
                    // so an entity in the crosshair must not be mistaken for a block behind it.
                    try {
                        crosshair = mc.getField("crosshairPickEntity");
                    } catch (ReflectiveOperationException | RuntimeException ignored) {
                    }
                } catch (ReflectiveOperationException | RuntimeException ignored) {
                    worldClip = null;
                    clipContextCtor = null;
                    blockShape = null;
                    fluidShape = null;
                }
            }
            levelClip = worldClip;
            clipCtor = clipContextCtor;
            clipBlockShape = blockShape;
            clipFluidShape = fluidShape;
            clipCollisionOf = collisionOf;
            clipCollisionEmpty = collisionEmpty;
            crosshairEntity = crosshair;

            // Interactive-block gate. The game's own menu check answers "would a right click
            // open a chest / crafting table / furnace"; a class-name match adds the blocks
            // that react without a screen (doors, trapdoors, beds, buttons, ...). Sneaking is
            // checked too, because then the game places the held block instead.
            Method hitPos = null, stateAt = null, stateMenu = null, stateBlock = null,
                   blockMenu = null, shiftDown = null;
            try {
                if (blockHit != null && lvl != null) {
                    hitPos = blockHit.getMethod("getBlockPos");
                    Class<?> posType = hitPos.getReturnType();
                    stateAt = lvl.getType().getMethod("getBlockState", posType);
                    Class<?> stateType = stateAt.getReturnType();
                    // The declaring type may be a superclass or interface (BlockBehaviour),
                    // so match by assignability instead of an exact getMethod signature.
                    stateMenu = findMethod(stateType, "getMenuProvider", lvl.getType(), posType);
                    // The block instance is also needed for the class-name check, whether or
                    // not the state exposes getMenuProvider itself.
                    try {
                        stateBlock = stateType.getMethod("getBlock");
                    } catch (ReflectiveOperationException | RuntimeException ignored) {
                    }
                    if (stateMenu == null && stateBlock != null) {
                        blockMenu = findMethod(stateBlock.getReturnType(), "getMenuProvider",
                                               stateType, lvl.getType(), posType);
                    }
                }
            } catch (ReflectiveOperationException | RuntimeException ignored) {
                hitPos = null;
                stateAt = null;
                stateMenu = null;
                stateBlock = null;
                blockMenu = null;
            }
            try {
                shiftDown = Class.forName("net.minecraft.world.entity.Entity")
                                  .getMethod("isShiftKeyDown");
            } catch (ReflectiveOperationException | RuntimeException ignored) {
            }
            hitBlockPos = hitPos;
            levelBlockState = stateAt;
            stateMenuProvider = stateMenu;
            stateBlockGetter = stateBlock;
            blockMenuProvider = blockMenu;
            isShiftKeyDown = shiftDown;
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

        /**
         * 1 = the crosshair is on a block (or within half a block of one), 0 = air or an
         * entity, -1 = cannot tell. Vanilla only reports blocks inside the block-interaction
         * range; the lead ray opens the gate just before the block is actually reachable, so
         * the right clicker starts a little early - the same deal the target gate gives the
         * left clicker.
         */
        int lookState(Object mc, Object self) {
            if (hitResult == null || (blockHitClass == null && hitType == null)) return -1;
            try {
                Object hit = hitResult.get(mc);
                if (isBlockHit(hit)) return 1;
                if (hit == null || self == null) return 0;
                // Some mappings put the entity pick in hitResult itself; that is not a block.
                if (entityHitClass != null && entityHitClass.isInstance(hit)) return 0;
                // Aiming at an entity is not aiming at a block, even when a wall is right behind
                // it. The block ray is a MISS then, so the separate entity pick has to be read.
                if (crosshairEntity != null && crosshairEntity.get(mc) != null) return 0;
                return blockJustBeyond(mc, self, hit) ? 1 : 0;
            } catch (ReflectiveOperationException | RuntimeException e) {
                return -1;
            }
        }

        /**
         * True when the vanilla block ray missed only because the block is just outside reach,
         * i.e. a block lies within BLOCK_LEAD of the ray end. The extension goes through the
         * game's own clip call, so non-solid blocks do not count and a ray that passes a corner
         * is judged the same way the game judges it.
         */
        private boolean blockJustBeyond(Object mc, Object self, Object hit) {
            if (level == null || levelClip == null || clipCtor == null ||
                clipBlockShape == null || clipFluidShape == null ||
                hitLocation == null || vec3Ctor == null ||
                vecX == null || vecY == null || vecZ == null ||
                getX == null || getY == null || getZ == null || getEyeY == null) {
                return false;
            }
            try {
                Object world = level.get(mc);
                Object loc = hitLocation.invoke(hit);
                if (world == null || loc == null) return false;
                double x = (Double) getX.invoke(self);
                double y = (Double) getEyeY.invoke(self);
                double z = (Double) getZ.invoke(self);
                double dx = vecX.getDouble(loc) - x;
                double dy = vecY.getDouble(loc) - y;
                double dz = vecZ.getDouble(loc) - z;
                double len = Math.sqrt(dx * dx + dy * dy + dz * dz);
                if (len < 1.0e-4) return false;
                // Extend along the actual pick ray rather than the look vector: they agree on a
                // normal miss, and this also stays right when the game nudged or clipped it.
                double scale = (len + BLOCK_LEAD) / len;
                Object from = vec3Ctor.newInstance(x, y, z);
                Object to = vec3Ctor.newInstance(x + dx * scale, y + dy * scale, z + dz * scale);
                Class<?>[] p = clipCtor.getParameterTypes();
                Object last;
                if (p[4].isInstance(self)) {
                    last = self;
                } else if (clipCollisionOf != null) {
                    last = clipCollisionOf.invoke(null, self);
                } else if (clipCollisionEmpty != null) {
                    last = clipCollisionEmpty.invoke(null);
                } else {
                    return false;
                }
                Object context = clipCtor.newInstance(from, to, clipBlockShape, clipFluidShape, last);
                return isBlockHit(levelClip.invoke(world, context));
            } catch (ReflectiveOperationException | RuntimeException e) {
                return false;
            }
        }

        /**
         * 1 when the crosshair is on a block that reacts to a use instead of taking a placed
         * block: a menu block (chest, crafting table, furnace, ...), a door, trapdoor, bed,
         * button, lever and similar. 0 when the block would accept the placement, -1 when
         * this mapping cannot tell. Sneaking is excluded because vanilla then places the
         * held block; without a sneak mapping the conservative answer is reported.
         */
        int interactiveState(Object mc, Object player) {
            if (hitResult == null || hitBlockPos == null || levelBlockState == null ||
                level == null ||
                (stateBlockGetter == null && stateMenuProvider == null && blockMenuProvider == null))
                return -1;
            try {
                Object hit = hitResult.get(mc);
                if (!isBlockHit(hit)) return 0;
                Object world = level.get(mc);
                if (world == null) return 0;
                Object pos = hitBlockPos.invoke(hit);
                if (pos == null) return 0;
                Object state = levelBlockState.invoke(world, pos);
                if (state == null) return 0;
                Object block = stateBlockGetter == null ? null : stateBlockGetter.invoke(state);
                boolean interactive = block != null && isInteractiveBlock(block);
                if (!interactive) {
                    Object provider = null;
                    if (stateMenuProvider != null) {
                        provider = stateMenuProvider.invoke(state, world, pos);
                    } else if (block != null && blockMenuProvider != null) {
                        provider = blockMenuProvider.invoke(block, state, world, pos);
                    }
                    interactive = provider != null;
                }
                if (!interactive) return 0;
                if (isShiftKeyDown != null && player != null &&
                    Boolean.TRUE.equals(isShiftKeyDown.invoke(player))) return 0;
                return 1;
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

        /** 1 while the game window has focus, 0 while another window does, -1 unknown. */
        int focusState(Object mc) {
            if (isWindowActive != null) {
                try {
                    return (Boolean) isWindowActive.invoke(mc) ? 1 : 0;
                } catch (ReflectiveOperationException | RuntimeException ignored) {
                    // Fall through to the window handle; a failure here is not fatal.
                }
            }
            if (windowField != null && windowFocused != null) {
                try {
                    Object window = windowField.get(mc);
                    if (window != null) return (Boolean) windowFocused.invoke(window) ? 1 : 0;
                } catch (ReflectiveOperationException | RuntimeException ignored) {
                    // Unknown.
                }
            }
            return -1;
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
                int hurtNow = hurtTime.getInt(self);
                float health = healthOf(self);
                if (lastHurtTime < 0) {
                    // First look at this player: remember the value, do not replay old damage.
                    lastHurtTime = hurtNow;
                } else {
                    long wallNow = System.nanoTime() / 1_000_000L;
                    boolean hurtRose = hurtNow > lastHurtTime;
                    // Health updates can arrive a poll before or after the animation. A drop
                    // is only a damage tick when it is more than float noise.
                    boolean healthFell = health >= 0.0f && lastHealth >= 0.0f &&
                                         health < lastHealth - 0.01f;
                    // The animation and the health update describe the same damage tick when
                    // they land close together. Count it once: a later health drop after a
                    // hurtTime rise (or the other way around) must not fire a second reset.
                    boolean newTick;
                    if (hurtRose && healthFell) newTick = true;
                    else if (hurtRose) newTick = wallNow - lastHealthDropMs > 100;
                    else if (healthFell) newTick = wallNow - lastHurtRiseMs > 100;
                    else newTick = false;
                    if (hurtRose) lastHurtRiseMs = wallNow;
                    if (healthFell) lastHealthDropMs = wallNow;
                    if (newTick) {
                        seenCounter++;
                        // The fast pass can see a damage tick between two full snapshots, so
                        // refresh the projectile list here before naming the attacker: an
                        // arrow that vanished this pass is the strongest 1.8 hit cue.
                        trackProjectiles(mc, self);
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
                    lastHurtTime = hurtNow;
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
            "fallingBlock", "fallingAnvil", "inWall", "cramming", "outsideBorder",
            "lightningBolt", "dry_out", "fallingStalactite", "sting", "campfire", "badRespawnPoint",
            "windCharge", "sonicBoom", "explosion", "fireworks", "unattributedFireball",
            "genericKill");

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
         * Best guess at what hurt the player when the server sent no attacker. The only local
         * cues that may open the counter are a projectile that vanished next to the player and
         * a living entity that is actually swinging. A crosshair target or merely being near an
         * entity is not enough: both are present during an ordinary fight, and using them made
         * a fall or burn tick look like a hit whenever someone happened to be close. Knockback
         * only decides the side and, when this mapping cannot read swings at all, is the last
         * signal left.
         *
         * <p>Nothing found means environmental damage, so the counter does not rise and the
         * module stays quiet. A health-only tick (the hurt animation was missed) needs a swing
         * or a projectile too, which keeps starvation and fire ticks out.</p>
         */
        private int classifyLocally(Object mc, Object self, boolean strict) {
            if (pendingProjectilePolls > 0) {
                int side = pendingProjectileSide;
                pendingProjectilePolls = 0;
                pendingProjectileSide = HIT_NONE;
                if (side != HIT_NONE) return side;
            }

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
                if (dist <= 4.5 && swinging(entity) && dist < swingerDist) {
                    swingerDist = dist;
                    swinger = entity;
                }
            }
            if (swinger != null) return sideOf(swinger, self);
            // No one is swinging, so this tick was environmental: fall, fire, drowning and
            // starvation all raise hurtTime without an attacker. Without a swing mapping the
            // old knockback/reach heuristic is still the only thing available.
            if (isSwinging != null) return HIT_NONE;
            if (knockbackPolls > 0) {
                knockbackPolls = 0;
                return knockbackSide;
            }
            double reach = strict ? 3.0 : 4.5;
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
                // The fast pass samples five times as often as the old loop, so the counter is
                // scaled to keep the same ~100 ms side window for the legacy fallback below.
                knockbackPolls = 10;
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
         *  half a block past vanilla reach and tolerates a cursor just beside a player, so the
         *  gate opens as a target runs in and the first clicks can miss. */
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
                    // The near pad is for players and mobs only: it is what makes the gate
                    // start while the cursor is just beside one, so the first clicks can miss.
                    // A fireball keeps the numerical slack, since aiming at one is more exact.
                    double pad = livingClass.isInstance(entity)
                                     ? TARGET_BOX_PAD + TARGET_NEAR_PAD : TARGET_BOX_PAD;
                    Object padded = inflate.invoke(box, pad);
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
