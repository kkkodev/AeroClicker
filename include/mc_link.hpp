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
//     slot=<0-8, or -1 outside a world> screen=<0|1> ms=<epoch milliseconds> block=<0|1|-1>
//
// `block` says whether the held item is a block (placeable). It is last so a build without it
// still parses as a prefix, and -1 means "this mod build cannot tell".
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

// Returns true (and fills slot/screen/block) only while the file is fresh. block is 1 when the
// held item is a block, 0 when it is not, and -1 when the running mod build does not report it.
inline bool poll(int& slot, bool& screen, int& block) {
    FILE* f = fopen(path(), "r");
    if (!f) return false;
    char line[160] = "";
    const bool got = fgets(line, sizeof(line), f) != nullptr;
    fclose(f);
    if (!got) return false;

    int s = -1, sc = 0, blk = -1;
    long long ms = 0;
    // `block` is last so a state line from an older mod still matches the first three fields.
    const int fields = sscanf(line, "slot=%d screen=%d ms=%lld block=%d", &s, &sc, &ms, &blk);
    if (fields != 3 && fields != 4) return false;
    if (fields == 3) blk = -1;
    // Both directions: a wall clock that jumped forward would make an old line look current.
    const long long age = now_ms() - ms;
    if (age > STALE_MS || age < -STALE_MS) return false;

    slot = s;
    screen = sc != 0;
    block = blk;
    return true;
}

}  // namespace McLink
