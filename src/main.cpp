#include "audio.hpp"
#include "clicker.hpp"
#include "clicker_config.hpp"
#include "config_file.hpp"
#include "gui.hpp"
#include "tracker.hpp"
#include "uinput_device.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/input.h>

static std::atomic<bool> g_running{true};

static void on_signal(int) { g_running = false; }

// The on/off state the user sees on the L/R pills: in MOUSE mode the pill reflects the
// armed flag (the clicker itself is only enabled while the mouse button is held), otherwise
// it reflects the clicker's enabled flag.
static bool clicker_master_on(const Clicker& c, bool armed, ActivationMode mode) {
    return mode == ActivationMode::MOUSE ? armed : c.is_enabled();
}

static bool is_physical_device(int fd) {
    char phys[256] = "";
    if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0 || phys[0] == '\0')
        return false;

    char name[256] = "";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    if (strcasestr(name, "AeroClicker")) return false;
    if (strcasestr(name, "Virtual")) return false;

    return true;
}

// Keybinds the binding widget can produce for mouse buttons.
static bool is_mouse_button(int code) {
    return code == BTN_LEFT || code == BTN_RIGHT || code == BTN_MIDDLE ||
           code == BTN_SIDE || code == BTN_EXTRA;
}

// The mouse device has to be read in two situations:
//  - a clicker injects through it (MOUSE activation consumes the physical button),
//  - a hotkey is bound to a mouse button. Without this, binding "Mouse M" as a toggle only
//    ever worked in Mouse activation mode, because that was the only case the device was
//    opened at all.
static bool mouse_read_wanted(const Config& c) {
    if (c.left_mode == ActivationMode::MOUSE || c.right_mode == ActivationMode::MOUSE) return true;
    if (is_mouse_button(c.key_left) || is_mouse_button(c.key_right)) return true;
    for (int m = 0; m < MAX_MACROS; m++)
        if (c.macros[m].key_bind > 0 && is_mouse_button(c.macros[m].key_bind)) return true;
    return false;
}

// Only MOUSE activation needs the exclusive grab; a hotkey-only session reads the device
// passively so the events reach the system (and the app) exactly once.
static bool mouse_grab_wanted(const Config& c) {
    return c.left_mode == ActivationMode::MOUSE || c.right_mode == ActivationMode::MOUSE;
}

static int find_device(const char* label, bool want_mouse) {
    DIR* dir = opendir("/dev/input");
    if (!dir) return -1;

    int fallback_fd = -1;
    char fallback_name[256] = "";
    char fallback_path[128] = "";

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char path[128];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        unsigned long evbits = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits);

        if (evbits & (1 << EV_KEY)) {
            unsigned long keybits[KEY_MAX / (8 * sizeof(unsigned long)) + 1] = {};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);

            auto has_key = [&](int code) {
                return keybits[code / (8 * sizeof(unsigned long))] &
                       (1UL << (code % (8 * sizeof(unsigned long))));
            };

            // A real keyboard has the full alphanumeric set plus modifiers.
            // Gaming mice (G305) register as kbd but lack the number row / F-row.
            bool is_keyboard = has_key(KEY_A) && has_key(KEY_Z) &&
                               has_key(KEY_1) && has_key(KEY_0) &&
                               has_key(KEY_F1) && has_key(KEY_F12) &&
                               has_key(KEY_SPACE) && has_key(KEY_ENTER) &&
                               has_key(KEY_LEFTSHIFT) && has_key(KEY_BACKSPACE) &&
                               !has_key(BTN_LEFT);
            bool is_mouse = has_key(BTN_LEFT) && has_key(BTN_RIGHT);

            if ((want_mouse && is_mouse) || (!want_mouse && is_keyboard)) {
                char name[256] = "Unknown";
                ioctl(fd, EVIOCGNAME(sizeof(name)), name);

                // Never pick our own virtual mouse back up (not even as the fallback): it
                // emits the clicks the clicker synthesises, which would re-trigger hotkeys.
                if (strcasestr(name, "AeroClicker") || strcasestr(name, "Virtual")) {
                    close(fd);
                    continue;
                }

                if (is_physical_device(fd)) {
                    printf("  %s: %s (%s)\n", label, name, path);
                    if (fallback_fd >= 0) close(fallback_fd);
                    closedir(dir);
                    return fd;
                }

                // Keep as fallback if no physical device found
                if (fallback_fd < 0) {
                    fallback_fd = fd;
                    strncpy(fallback_name, name, sizeof(fallback_name) - 1);
                    strncpy(fallback_path, path, sizeof(fallback_path) - 1);
                    continue;
                }
            }
        }
        close(fd);
    }
    closedir(dir);

    if (fallback_fd >= 0) {
        printf("  %s (virtual): %s (%s)\n", label, fallback_name, fallback_path);
        return fallback_fd;
    }
    return -1;
}

static void input_thread(Config& config, Clicker& left_clicker, Clicker& right_clicker,
                          UInputDevice& device, GuiState& gui_state) {
    // A device that was opened at startup can vanish: a keyboard swapped or replugged, a mouse
    // re-docked, the whole input stack restarting with the compositor. A dead fd never reports
    // POLLIN again, so without re-acquiring it the hotkeys or the mouse stayed dead until the
    // app was restarted. EVIOCGNAME fails with ENODEV once the device is gone, which is a cheap
    // way to notice.
    auto device_alive = [](int fd) {
        if (fd < 0) return false;
        char name[64];
        return ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0;
    };
    // Rescanning opens every node under /dev/input, so a missing device is looked for on a
    // timer instead of on every pass of the loop below (which runs every 50 ms).
    constexpr auto RESCAN_INTERVAL = std::chrono::seconds(2);
    using Clock = std::chrono::steady_clock;

    int kb_fd = -1;
    bool kb_seen = false;
    bool warned_no_kb = false;
    auto next_kb_scan = Clock::now();

    int mouse_fd = -1;
    bool mouse_grabbed = false;
    bool mouse_seen = false;
    auto next_mouse_scan = Clock::now();

    // Keeps the mouse device/grab in sync with the current modes and keybinds: at startup, when
    // they are changed in the UI, and after the device is replugged.
    auto sync_mouse_device = [&]() {
        const bool want_read = mouse_read_wanted(config);
        const bool want_grab = mouse_grab_wanted(config);

        static bool warned_no_mouse = false;
        if (want_read && mouse_fd < 0) {
            const auto now = Clock::now();
            if (now >= next_mouse_scan) {
                mouse_fd = find_device("Mouse", true);
                if (mouse_fd >= 0) {
                    if (mouse_seen) printf("  Mouse reconnected\n");
                    mouse_seen = true;
                } else {
                    next_mouse_scan = now + RESCAN_INTERVAL;
                    if (!warned_no_mouse) {
                        warned_no_mouse = true;
                        fprintf(stderr, "  Warning: no mouse device readable (need read access "
                                        "to /dev/input/*). Mouse-button hotkeys and Mouse "
                                        "activation will not work until one appears.\n");
                    }
                }
            }
        } else if (!want_read && mouse_fd >= 0) {
            if (mouse_grabbed) { ioctl(mouse_fd, EVIOCGRAB, 0); mouse_grabbed = false; }
            close(mouse_fd);
            mouse_fd = -1;
        }
        if (mouse_fd < 0) return;

        if (want_grab && !mouse_grabbed) {
            if (ioctl(mouse_fd, EVIOCGRAB, 1) == 0) {
                mouse_grabbed = true;
                printf("  Mouse grabbed for click injection\n");
            }
        } else if (!want_grab && mouse_grabbed) {
            ioctl(mouse_fd, EVIOCGRAB, 0);
            mouse_grabbed = false;
            printf("  Mouse grab released (hotkey only)\n");
        }
    };

    bool mouse_left_held = false;
    bool mouse_right_held = false;
    // Which button's current physical press belongs to the clicker (MOUSE mode). Deciding
    // this once, on the press, and remembering it until the matching release is what keeps
    // a press and its release symmetric: if the cursor moves over the app (e.g. the click
    // landed on the titlebar and activated the window) the release used to be forwarded to
    // the app instead of stopping the clicker, which left it clicking with nothing held.
    bool mouse_left_owned = false;
    bool mouse_right_owned = false;

    // The automatic pauses are derived state, recomputed every iteration so that changing the
    // slot filter or the inventory settings takes effect at once instead of at the next key
    // press. They pause rather than switch a clicker off, which is what keeps the on/off state
    // the user asked for visible (and resumable) in the UI.
    auto apply_pauses = [&](Clicker& clicker, bool slot_override) {
        clicker.set_pause(Clicker::PAUSE_INVENTORY, gui_state.in_menu.load());
        const bool filtered = config.slot_filter_enabled && !slot_override &&
                              !config.allowed_slots[gui_state.active_slot.load()];
        clicker.set_pause(Clicker::PAUSE_SLOT, filtered);
    };

    // A clicker's bound key is its master control: it acts even while the app believes the
    // inventory is open (typing the inventory letter in chat leaves that state inverted), and
    // it overrides both automatic pauses. The slot filter takes control back as soon as another
    // hotbar slot is selected. Shared by the keyboard and mouse paths, since the same key can
    // be bound on either device.
    //
    // A clicker that is switched on but held back by a pause is *not* switched off by its own
    // key: the press means "run now", so it clears the pause and a second press stops it as
    // usual. Otherwise rescuing the state after a stray inventory key would take two presses.
    auto handle_clicker_key = [&](bool left_side, int value) {
        Clicker& clicker = left_side ? left_clicker : right_clicker;
        bool& armed = left_side ? gui_state.left_armed : gui_state.right_armed;
        std::atomic<bool>& slot_override = left_side ? gui_state.left_slot_override
                                                     : gui_state.right_slot_override;
        const ActivationMode mode = left_side ? config.left_mode : config.right_mode;

        const bool was = clicker_master_on(clicker, armed, mode);
        const bool unpause_only = value == 1 && was && (clicker.pause_mask() & Clicker::PAUSE_AUTO);

        if (!unpause_only) {
            if (mode == ActivationMode::TOGGLE) {
                if (value == 1) clicker.toggle();
            } else if (mode == ActivationMode::HOLD) {
                clicker.set_enabled(value != 0);
            } else {
                if (value == 1) {
                    armed = !armed;
                    const bool button_held = left_side ? mouse_left_held : mouse_right_held;
                    if (!armed) clicker.set_enabled(false);
                    else if (button_held) clicker.set_enabled(true);
                }
            }
        }

        const bool now = clicker_master_on(clicker, armed, mode);
        if (now) {
            // Coming on (or being un-paused) beats the guessed inventory state and the filter.
            gui_state.in_menu = false;
            clicker.set_pause(Clicker::PAUSE_INVENTORY, false);
            clicker.set_pause(Clicker::PAUSE_SLOT, false);
            slot_override = true;
        } else {
            slot_override = false;
        }
        if (config.audio_enabled && now != was) {
            if (now) Audio::beep_on();
            else Audio::beep_off();
        }
    };

    struct pollfd fds[2] = {};
    struct input_event ev;

    while (g_running) {
        // A replugged device leaves the old fd reporting errors forever, so drop it here: the
        // keyboard acquisition below and sync_mouse_device() then open the new node.
        if (kb_fd >= 0 && !device_alive(kb_fd)) {
            fprintf(stderr, "  Keyboard disconnected, waiting for it to come back\n");
            close(kb_fd);
            kb_fd = -1;
            // The hotkeys that could clear the guessed inventory state went with it, so do not
            // leave the clickers paused because of a key press we can no longer see.
            gui_state.in_menu = false;
            next_kb_scan = Clock::now() + RESCAN_INTERVAL;
        }
        if (mouse_fd >= 0 && !device_alive(mouse_fd)) {
            fprintf(stderr, "  Mouse disconnected, waiting for it to come back\n");
            // Nothing can release a button that vanished, so stop the clickers that were
            // driving it: otherwise they keep clicking with no physical button held.
            if (mouse_left_owned)  { left_clicker.set_enabled(false);  mouse_left_owned = false; }
            if (mouse_right_owned) { right_clicker.set_enabled(false); mouse_right_owned = false; }
            mouse_left_held = mouse_right_held = false;
            mouse_grabbed = false;          // the grab went with the device
            close(mouse_fd);
            mouse_fd = -1;
        }
        // (Re)acquire the keyboard when it is not open: at startup, and after a replug.
        if (kb_fd < 0) {
            const auto now = Clock::now();
            if (now >= next_kb_scan) {
                kb_fd = find_device("Keyboard", false);
                if (kb_fd >= 0) {
                    if (kb_seen) printf("  Keyboard reconnected\n");
                    kb_seen = true;
                } else {
                    next_kb_scan = now + RESCAN_INTERVAL;
                    if (!warned_no_kb) {
                        warned_no_kb = true;
                        fprintf(stderr, "  Warning: No keyboard found (need read access to "
                                        "/dev/input/*). Keyboard hotkeys and the slot filter "
                                        "will not work until one appears.\n");
                    }
                }
            }
        }
        sync_mouse_device();
        apply_pauses(left_clicker, gui_state.left_slot_override.load());
        apply_pauses(right_clicker, gui_state.right_slot_override.load());

        int nfds = 1;
        fds[0].fd = kb_fd;          // -1 is fine: poll() ignores negative descriptors
        fds[0].events = POLLIN;
        if (mouse_fd >= 0) {
            fds[1].fd = mouse_fd;
            fds[1].events = POLLIN;
            nfds = 2;
        }

        int ret = poll(fds, nfds, 50);
        if (ret <= 0) continue;

        if (fds[0].revents & POLLIN) {
            while (read(kb_fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type != EV_KEY) continue;

                // Hotbar slots: top-row digits and numpad digits both work, so the filter
                // tracks whatever the player actually uses.
                int slot = -1;
                if (ev.code >= KEY_1 && ev.code <= KEY_9)
                    slot = (int)ev.code - KEY_1;
                else if (ev.code >= KEY_KP1 && ev.code <= KEY_KP9)
                    slot = (int)ev.code - KEY_KP1;
                if (ev.value == 1 && slot >= 0) {
                    gui_state.active_slot = slot;
                    // Selecting a slot hands control back to the filter, so an override made in
                    // the slot the player just left cannot outlive it.
                    gui_state.left_slot_override = false;
                    gui_state.right_slot_override = false;
                }

                if (config.inventory_key_enabled && ev.value == 1 && ev.code == static_cast<unsigned>(config.key_inventory)) {
                    gui_state.in_menu = !gui_state.in_menu;
                    continue;
                }
                if (ev.value == 1 && ev.code == static_cast<unsigned>(config.key_escape) && gui_state.in_menu) {
                    gui_state.in_menu = false;
                    continue;
                }

                if (ev.code == static_cast<unsigned>(config.key_left)) {
                    handle_clicker_key(true, ev.value);
                }
                else if (ev.code == static_cast<unsigned>(config.key_right)) {
                    handle_clicker_key(false, ev.value);
                }
                else if (ev.value == 1 && config.macros_enabled && !gui_state.in_menu) {
                    for (int m = 0; m < MAX_MACROS; m++) {
                        if (config.macros[m].key_bind > 0 &&
                            ev.code == static_cast<unsigned>(config.macros[m].key_bind) &&
                            config.macros[m].step_count > 0) {
                            gui_state.macro_runner.toggle(config.macros[m], &gui_state.active_slot);
                            break;
                        }
                    }
                }
            }
        }

        if (nfds > 1 && (fds[1].revents & POLLIN)) {
            while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY) {
                    bool is_bound_toggle = false;

                    if (ev.code == static_cast<unsigned>(config.key_left)) {
                        handle_clicker_key(true, ev.value);
                        is_bound_toggle = true;
                    }
                    else if (ev.code == static_cast<unsigned>(config.key_right)) {
                        handle_clicker_key(false, ev.value);
                        is_bound_toggle = true;
                    }

                    // Hotkey-only session (device not grabbed): the compositor already
                    // delivers these events, so never forward or re-inject them — doing so
                    // would double every click.
                    if (!mouse_grabbed) continue;

                    if (!is_bound_toggle) {
                        // While the pointer is over AeroClicker the button belongs to the
                        // interface: keeping it as a MOUSE-mode trigger makes the whole UI
                        // unclickable, because the press is swallowed instead of forwarded.
                        const bool over_app = gui_state.ui_hovered.load();
                        // PAUSE_AUTO is the inventory pause plus the slot filter; both already
                        // account for the bound key's override, so this path needs no separate
                        // slot test. over_app is checked locally because ownership is decided
                        // before the GUI refreshes that pause for this frame.
                        if (ev.code == BTN_LEFT) {
                            if (ev.value == 1) {
                                mouse_left_held = true;
                                mouse_left_owned = !over_app &&
                                                   !(left_clicker.pause_mask() & Clicker::PAUSE_AUTO) &&
                                                   config.left_mode == ActivationMode::MOUSE &&
                                                   gui_state.left_armed;
                                if (mouse_left_owned) left_clicker.set_enabled(true);
                                else device.click(BTN_LEFT, 1);
                            } else if (ev.value == 0) {
                                mouse_left_held = false;
                                if (mouse_left_owned) {
                                    left_clicker.set_enabled(false);
                                    mouse_left_owned = false;
                                } else {
                                    device.click(BTN_LEFT, 0);
                                }
                            }
                        }
                        else if (ev.code == BTN_RIGHT) {
                            if (ev.value == 1) {
                                mouse_right_held = true;
                                mouse_right_owned = !over_app &&
                                                    !(right_clicker.pause_mask() & Clicker::PAUSE_AUTO) &&
                                                    config.right_mode == ActivationMode::MOUSE &&
                                                    gui_state.right_armed;
                                if (mouse_right_owned) right_clicker.set_enabled(true);
                                else device.click(BTN_RIGHT, 1);
                            } else if (ev.value == 0) {
                                mouse_right_held = false;
                                if (mouse_right_owned) {
                                    right_clicker.set_enabled(false);
                                    mouse_right_owned = false;
                                } else {
                                    device.click(BTN_RIGHT, 0);
                                }
                            }
                        }
                        else {
                            device.click(ev.code, ev.value);
                        }
                    } else {
                        // It is a bound toggle key. We must still inject it because the mouse is grabbed.
                        if (ev.code == BTN_LEFT) mouse_left_held = (ev.value != 0);
                        else if (ev.code == BTN_RIGHT) mouse_right_held = (ev.value != 0);
                        device.click(ev.code, ev.value);
                    }
                }
                else if (ev.type == EV_REL || ev.type == EV_SYN) {
                    if (mouse_grabbed) device.emit_raw(ev.type, ev.code, ev.value);
                }
            }
        }
    }

    if (mouse_fd >= 0) {
        if (mouse_grabbed) ioctl(mouse_fd, EVIOCGRAB, 0);
        close(mouse_fd);
    }
    if (kb_fd >= 0) close(kb_fd);
}

int main(int argc, char* argv[]) {
    // Under sudo -E, GTK finds the user's broken icon theme via HOME and crashes.
    // Redirect HOME so GTK can't find ~/.local/share/icons/kora.
    if (getuid() == 0) {
        const char* real_home = getenv("HOME");
        // HOME is replaced below, so the config location has to be pinned to the real home
        // first: left alone, ~/.config follows HOME to /tmp, where saving quietly fails and
        // where another local user could plant the directory it writes into.
        struct stat st;
        if (real_home && stat(real_home, &st) == 0) {
            ConfigFile::use_dir(std::string(real_home) + "/.config/aeroclicker",
                                st.st_uid, st.st_gid);
        }

        setenv("HOME", "/tmp", 1);
        setenv("XDG_DATA_HOME", "/tmp", 1);
        setenv("XDG_CACHE_HOME", "/tmp", 1);
    }

    Config config;
    ConfigFile::load(config);

    bool headless = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lcps") == 0 && i + 1 < argc)
            config.left.cps_mean = atof(argv[++i]);
        else if (strcmp(argv[i], "--rcps") == 0 && i + 1 < argc)
            config.right.cps_mean = atof(argv[++i]);
        else if (strcmp(argv[i], "--lkey") == 0 && i + 1 < argc)
            config.key_left = atoi(argv[++i]);
        else if (strcmp(argv[i], "--rkey") == 0 && i + 1 < argc)
            config.key_right = atoi(argv[++i]);
        else if (strcmp(argv[i], "--invkey") == 0 && i + 1 < argc)
            config.key_inventory = atoi(argv[++i]);
        else if (strcmp(argv[i], "--hold") == 0) {
            config.left_mode = ActivationMode::HOLD;
            config.right_mode = ActivationMode::HOLD;
        }
        else if (strcmp(argv[i], "--toggle") == 0) {
            config.left_mode = ActivationMode::TOGGLE;
            config.right_mode = ActivationMode::TOGGLE;
        }
        else if (strcmp(argv[i], "--mouse") == 0) {
            config.left_mode = ActivationMode::MOUSE;
            config.right_mode = ActivationMode::MOUSE;
        }
        else if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        }
        else if (strcmp(argv[i], "--save") == 0) {
            ConfigFile::save(config);
            printf("  Config saved to %s/config\n", ConfigFile::path().c_str());
            return 0;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("AeroClicker - Humanized auto-clicker for Linux/Wayland\n\n");
            printf("Usage: aeroclicker [options]\n\n");
            printf("Options:\n");
            printf("  --lcps <n>     Left CPS mean (default: %.1f)\n", config.left.cps_mean);
            printf("  --rcps <n>     Right CPS mean (default: %.1f)\n", config.right.cps_mean);
            printf("  --lkey <code>  Left toggle key (default: %d)\n", config.key_left);
            printf("  --rkey <code>  Right toggle key (default: %d)\n", config.key_right);
            printf("  --invkey <code> Inventory key (default: %d)\n", config.key_inventory);
            printf("  --toggle       Toggle mode\n");
            printf("  --hold         Hold mode\n");
            printf("  --mouse        Mouse mode (default; arm with key, hold mouse to click)\n");
            printf("  --headless     Run without GUI (CLI only)\n");
            printf("  --save         Save current config to ~/.config/aeroclicker/config\n\n");
            printf("Requires /dev/uinput and /dev/input/* access.\n");
            printf("Run: sudo usermod -aG input $USER  (then re-login)\n");
            return 0;
        }
    }

    printf("\n  AeroClicker - Humanized Auto-Clicker\n");
    printf("  ────────────────────────────────────\n");

    UInputDevice device;
    printf("  Virtual mouse device created\n");

    Clicker left_clicker(config.left, device);
    Clicker right_clicker(config.right, device);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    // The toggle chime pipes its samples into `paplay`; if that child is already gone the
    // write raises SIGPIPE, whose default action would kill the app. Ignore it so the write
    // just fails, which audio.hpp checks.
    signal(SIGPIPE, SIG_IGN);

    GuiState gui_state(config, left_clicker, right_clicker, device);

    std::thread input_thr(input_thread, std::ref(config), std::ref(left_clicker),
                          std::ref(right_clicker), std::ref(device), std::ref(gui_state));

    if (headless) {
        printf("  Running headless (Ctrl+C to quit)\n\n");
        while (g_running) {
            printf("\r  L: %-3s %5.1f cps (%u)  |  R: %-3s %5.1f cps (%u)   ",
                   left_clicker.is_enabled() ? "ON" : "OFF",
                   left_clicker.tracker().avg_cps(), left_clicker.click_count(),
                   right_clicker.is_enabled() ? "ON" : "OFF",
                   right_clicker.tracker().avg_cps(), right_clicker.click_count());
            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    } else {
        gui_run(gui_state);
        g_running = false;
    }

    left_clicker.set_enabled(false);
    right_clicker.set_enabled(false);

    if (input_thr.joinable()) input_thr.join();

    printf("\n  Stopped.\n");
    return 0;
}
