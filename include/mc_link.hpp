#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Real Minecraft state, published by the companion Fabric mod in tools/mc-bridge.
//
// The app is fully external, so the only things it can see are key presses: E does not say
// whether the inventory is open or the player is typing in chat, and the hotbar slot can be
// moved by the wheel, pick-block or another binding. The mod closes exactly that gap by
// writing one line to the session runtime directory, which both processes can reach:
//
//     slot=<0-8, or -1 outside a world> screen=<0|1> ms=<epoch milliseconds>
//     block=<0|1|-1> sword=<0|1|-1> target=<0|1|-1> bed=<0|1|-1>
//     look=<0|1|-1> water=<0|1|-1> ground=<0|1|-1> hurt=<counter> hit=<counter>
//     back=<counter>
//
// `block` says whether the held item is a block (placeable), `sword` whether it is a sword,
// `target` is a bitmask of what the crosshair ray found (1 = a mob or player, 2 = a fireball,
// 3 = both), and `bed` says whether the held item is a bed. The app decides whether fireballs
// count; an older mod that only reports 0/1 simply leaves that option inert. `look` is set
// while the crosshair is on a block, `water` while the local player is in water, and `ground`
// while the player stands on the ground.
// `hurt` and `hit` are counters: hurt goes up when the local player takes damage, hit when the
// local player attacks a living entity, and `back` goes up alongside hurt when the attacker was
// behind the player (for an arrow, snowball or egg: when it came from behind, judged by its
// flight direction). `seen` counts every damage tick, attributed or not: comparing it with hurt
// separates a hit that never reached the bridge from one a filter dropped. They are last so a
// build without them still parses as a prefix, and -1 means "this mod build cannot tell". Older mod builds call the target field `player`; it is
// read as `target` too.
// Reading is the whole contract: nothing is sent to the server and no input is touched.
namespace McLink {

// A game that crashed or was closed must not leave the clicker paused forever, so a line older
// than this is treated as no mod at all and the key-based guesses take over again.
inline constexpr long long STALE_MS = 2500;

inline const char* path() {
    // XDG_RUNTIME_DIR is per-user and already private; /tmp is the documented fallback for both
    // sides, so the app and the mod agree even without a session bus.
    static const std::string file = [] {
        const char* dir = getenv("XDG_RUNTIME_DIR");
        return std::string(dir && dir[0] ? dir : "/tmp") + "/aeroclicker-mc.state";
    }();
    return file.c_str();
}

inline long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// One poll of the bridge mod. Anything the running build cannot report stays at -1; the event
// counters stay at their previous value, so a mod that predates them never fires a reset.
struct State {
    int slot = -1;
    bool screen = false;
    int block = -1;
    int sword = -1;
    int target = -1;
    int bed = -1;
    int look = -1;
    int water = -1;
    int ground = -1;
    // Posture latched by the bridge for the newest hurt tick (the sample before the damage).
    int hwater = -1;
    int hground = -1;
    long long hurt = 0;
    long long hit = 0;
    long long back = 0;
    long long seen = 0;
};

// Returns true only while the file is fresh, and fills `out` with that line.
inline bool poll(State& out) {
    FILE* f = fopen(path(), "r");
    if (!f) return false;
    char line[320] = "";
    const bool got = fgets(line, sizeof(line), f) != nullptr;
    fclose(f);
    if (!got) return false;

    int s = -1, sc = 0, blk = -1, swd = -1;
    long long ms = 0;
    // The fields are appended in order, so an older mod's shorter line still matches its
    // prefix: 3 fields is the original format, 4 adds block, 5 adds sword and the target field.
    const int fields = sscanf(line, "slot=%d screen=%d ms=%lld block=%d sword=%d",
                              &s, &sc, &ms, &blk, &swd);
    if (fields < 3) return false;
    // Both directions: a wall clock that jumped forward would make an old line look current.
    const long long age = now_ms() - ms;
    if (age > STALE_MS || age < -STALE_MS) return false;

    auto int_field = [&](const char* name, int fallback) {
        const std::string needle = std::string(" ") + name + "=";
        if (const char* p = strstr(line, needle.c_str())) {
            int v = fallback;
            if (sscanf(p, (needle + "%d").c_str(), &v) == 1) return v;
        }
        return fallback;
    };
    auto long_field = [&](const char* name) {
        const std::string needle = std::string(" ") + name + "=";
        if (const char* p = strstr(line, needle.c_str())) {
            long long v = 0;
            if (sscanf(p, (needle + "%lld").c_str(), &v) == 1) return v;
        }
        return 0LL;
    };

    // The line is not trusted to stay in range: a corrupted, foreign or hand-written file must
    // not index the nine-slot maps out of bounds. Anything outside 0-8 means "no slot".
    out.slot = (s >= 0 && s < 9) ? s : -1;
    out.screen = sc != 0;
    out.block = blk;
    out.sword = swd;
    // The target field was called `player` before it started covering mobs.
    out.target = -1;
    if (const char* p = strstr(line, " target=")) sscanf(p, " target=%d", &out.target);
    else if (const char* p = strstr(line, " player=")) sscanf(p, " player=%d", &out.target);
    out.bed = int_field("bed", -1);
    out.look = int_field("look", -1);
    out.water = int_field("water", -1);
    out.ground = int_field("ground", -1);
    out.hwater = int_field("hwater", -1);
    out.hground = int_field("hground", -1);
    out.hurt = long_field("hurt");
    out.hit = long_field("hit");
    out.back = long_field("back");
    out.seen = long_field("seen");
    return true;
}

}  // namespace McLink
