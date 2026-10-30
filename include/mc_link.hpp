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
//     block=<0|1|-1> sword=<0|1|-1> target=<0|1|-1>
//
// `block` says whether the held item is a block (placeable), `sword` whether it is a sword,
// and `target` whether the crosshair is on a living entity (a mob or another player). They are
// last so a build without them still parses as a prefix, and -1 means "this mod build cannot
// tell". Older mod builds call the last field `player`; it is read as `target` too.
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

// Returns true (and fills the fields) only while the file is fresh. block is 1 when the held
// item is a block, sword is 1 when it is a sword, target is 1 when the crosshair is on a living
// entity (mob or player), and -1 means the running mod build does not report that field.
inline bool poll(int& slot, bool& screen, int& block, int& sword, int& target) {
    FILE* f = fopen(path(), "r");
    if (!f) return false;
    char line[160] = "";
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

    // The target field was called `player` before it started covering mobs. Read either name,
    // and leave it at -1 when the running mod predates both.
    int tgt = -1;
    if (const char* p = strstr(line, " target=")) sscanf(p, " target=%d", &tgt);
    else if (const char* p = strstr(line, " player=")) sscanf(p, " player=%d", &tgt);

    slot = s;
    screen = sc != 0;
    block = blk;
    sword = swd;
    target = tgt;
    return true;
}

}  // namespace McLink
