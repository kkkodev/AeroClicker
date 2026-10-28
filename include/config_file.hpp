#pragma once
#include "clicker_config.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

class ConfigFile {
public:
    // A profile is a plain file name inside the config directory, and the Settings tab passes
    // through exactly what was typed there. Anything that could climb out of that directory is
    // refused, so a name like "../../.bashrc" cannot overwrite a file somewhere else.
    static bool valid_name(const std::string& n) {
        if (n.empty() || n.size() > 64) return false;
        if (n[0] == '.') return false;
        return n.find('/') == std::string::npos && n.find('\\') == std::string::npos;
    }

    // Overrides where the config lives, for when HOME is not the user's home: as root the app
    // redirects HOME to /tmp so GTK cannot trip over an unreadable icon theme, and the config
    // must not follow it into a world-writable directory. `owner` is chowned onto everything
    // written, because sudo writes as root but the files belong to the login user.
    static void use_dir(const std::string& dir, uid_t owner = static_cast<uid_t>(-1),
                        gid_t group = static_cast<gid_t>(-1)) {
        dir_override() = dir;
        owner_uid() = owner;
        owner_gid() = group;
    }

    static std::string path() {
        if (!dir_override().empty()) return dir_override();
        // Resolved once per run. AeroClicker used to be called Ghast Clicker, so if the new
        // directory does not exist yet but the old one does, seed it from the old directory:
        // existing profiles, keybinds and macros survive the rename.
        static const std::string dir = [] {
            const char* home = getenv("HOME");
            if (!home) home = "/tmp";
            const std::string base = std::string(home) + "/.config";
            const std::string current = base + "/aeroclicker";
            const std::string legacy = base + "/ghast-clicker";

            struct stat st;
            if (stat(current.c_str(), &st) != 0 && stat(legacy.c_str(), &st) == 0) {
                std::error_code ec;
                std::filesystem::copy(legacy, current,
                    std::filesystem::copy_options::recursive |
                    std::filesystem::copy_options::copy_symlinks, ec);
            }
            return current;
        }();
        return dir;
    }

private:
    static std::string& dir_override() { static std::string d; return d; }
    static uid_t& owner_uid() { static uid_t u = static_cast<uid_t>(-1); return u; }
    static gid_t& owner_gid() { static gid_t g = static_cast<gid_t>(-1); return g; }

public:
    static void write_profile(FILE* f, const char* prefix, const ClickerProfile& p) {
        fprintf(f, "%s.cps_mean %g\n", prefix, p.cps_mean);
        fprintf(f, "%s.cps_stddev %g\n", prefix, p.cps_stddev);
        fprintf(f, "%s.cps_min %g\n", prefix, p.cps_min);
        fprintf(f, "%s.cps_max %g\n", prefix, p.cps_max);
        fprintf(f, "%s.cps_minmax_fallout %g\n", prefix, p.cps_minmax_fallout);
        fprintf(f, "%s.spike_chance %g\n", prefix, p.spike_chance);
        fprintf(f, "%s.spike_min %g\n", prefix, p.spike_min);
        fprintf(f, "%s.spike_max %g\n", prefix, p.spike_max);
        fprintf(f, "%s.stutter_chance %g\n", prefix, p.stutter_chance);
        fprintf(f, "%s.stutter_min %g\n", prefix, p.stutter_min);
        fprintf(f, "%s.stutter_max %g\n", prefix, p.stutter_max);
        fprintf(f, "%s.hold_ms_mean %g\n", prefix, p.hold_ms_mean);
        fprintf(f, "%s.hold_ms_stddev %g\n", prefix, p.hold_ms_stddev);
        fprintf(f, "%s.hold_ms_min %g\n", prefix, p.hold_ms_min);
        fprintf(f, "%s.hold_ms_max %g\n", prefix, p.hold_ms_max);
        fprintf(f, "%s.hold_ms_heavy_chance %g\n", prefix, p.hold_ms_heavy_chance);
        fprintf(f, "%s.hold_ms_heavy_min %g\n", prefix, p.hold_ms_heavy_min);
        fprintf(f, "%s.hold_ms_heavy_max %g\n", prefix, p.hold_ms_heavy_max);
        fprintf(f, "%s.rhythm_volatility %g\n", prefix, p.rhythm_volatility);
        fprintf(f, "%s.rhythm_tension %g\n", prefix, p.rhythm_tension);
        fprintf(f, "%s.start_delay_ms %g\n", prefix, p.start_delay_ms);
        fprintf(f, "%s.double_click_chance %g\n", prefix, p.double_click_chance);
        fprintf(f, "%s.double_click_delay_min %g\n", prefix, p.double_click_delay_min);
        fprintf(f, "%s.double_click_delay_max %g\n", prefix, p.double_click_delay_max);
        fprintf(f, "%s.breaks_enabled %d\n", prefix, p.breaks_enabled ? 1 : 0);
        fprintf(f, "%s.break_interval_min %g\n", prefix, p.break_interval_min);
        fprintf(f, "%s.break_interval_max %g\n", prefix, p.break_interval_max);
        fprintf(f, "%s.break_duration_min %g\n", prefix, p.break_duration_min);
        fprintf(f, "%s.break_duration_max %g\n", prefix, p.break_duration_max);
        fprintf(f, "%s.distribution %d\n", prefix, static_cast<int>(p.distribution));
        fprintf(f, "%s.hold_drift_ms %g\n", prefix, p.hold_drift_ms);
        fprintf(f, "%s.hold_drift_tension %g\n", prefix, p.hold_drift_tension);
    }

    static void read_profile_key(const char* key, double val, const char* prefix, ClickerProfile& p) {
        char buf[96];
        auto match = [&](const char* field) {
            snprintf(buf, sizeof(buf), "%s.%s", prefix, field);
            return strcmp(key, buf) == 0;
        };
        if (match("cps_mean")) p.cps_mean = val;
        else if (match("cps_stddev")) p.cps_stddev = val;
        else if (match("cps_min")) p.cps_min = val;
        else if (match("cps_max")) p.cps_max = val;
        else if (match("cps_minmax_fallout")) p.cps_minmax_fallout = val;
        else if (match("spike_chance")) p.spike_chance = val;
        else if (match("spike_min")) p.spike_min = val;
        else if (match("spike_max")) p.spike_max = val;
        else if (match("stutter_chance")) p.stutter_chance = val;
        else if (match("stutter_min")) p.stutter_min = val;
        else if (match("stutter_max")) p.stutter_max = val;
        else if (match("hold_ms_mean")) p.hold_ms_mean = val;
        else if (match("hold_ms_stddev")) p.hold_ms_stddev = val;
        else if (match("hold_ms_min")) p.hold_ms_min = val;
        else if (match("hold_ms_max")) p.hold_ms_max = val;
        else if (match("hold_ms_heavy_chance")) p.hold_ms_heavy_chance = val;
        else if (match("hold_ms_heavy_min")) p.hold_ms_heavy_min = val;
        else if (match("hold_ms_heavy_max")) p.hold_ms_heavy_max = val;
        else if (match("rhythm_volatility")) p.rhythm_volatility = val;
        else if (match("rhythm_tension")) p.rhythm_tension = val;
        else if (match("start_delay_ms")) p.start_delay_ms = val;
        else if (match("double_click_chance")) p.double_click_chance = val;
        else if (match("double_click_delay_min")) p.double_click_delay_min = val;
        else if (match("double_click_delay_max")) p.double_click_delay_max = val;
        else if (match("breaks_enabled")) p.breaks_enabled = (int)val != 0;
        else if (match("break_interval_min")) p.break_interval_min = val;
        else if (match("break_interval_max")) p.break_interval_max = val;
        else if (match("break_duration_min")) p.break_duration_min = val;
        else if (match("break_duration_max")) p.break_duration_max = val;
        else if (match("distribution")) {
            // 2 is UNIFORM, 0 is GAUSSIAN. 1 was POISSON, which no longer exists: a profile
            // saved with it comes back as Gaussian instead of silently becoming uniform.
            p.distribution = static_cast<int>(val) == 2 ? DistributionType::UNIFORM
                                                        : DistributionType::GAUSSIAN;
        }
        else if (match("hold_drift_ms")) p.hold_drift_ms = val;
        else if (match("hold_drift_tension")) p.hold_drift_tension = val;
    }

    static bool load(Config& cfg, const std::string& filename = "config") {
        if (!valid_name(filename)) return false;
        std::string p = path() + "/" + filename;
        FILE* f = fopen(p.c_str(), "r");
        if (!f) return false;

        char line[256];
        bool accent_order_seen = false;   // file was written with the current accent list
        while (fgets(line, sizeof(line), f)) {
            char key[96];
            double val;
            if (sscanf(line, "%95s %lf", key, &val) != 2) continue;

            read_profile_key(key, val, "left", cfg.left);
            read_profile_key(key, val, "right", cfg.right);

            if (!strcmp(key, "key_left")) cfg.key_left = (int)val;
            else if (!strcmp(key, "key_right")) cfg.key_right = (int)val;
            else if (!strcmp(key, "accent_preset")) cfg.accent_preset = (int)val;
            else if (!strcmp(key, "accent_order")) accent_order_seen = true;
            else if (!strcmp(key, "accent_r")) cfg.accent_r = val;
            else if (!strcmp(key, "accent_g")) cfg.accent_g = val;
            else if (!strcmp(key, "accent_b")) cfg.accent_b = val;
            else if (!strcmp(key, "left_mode")) cfg.left_mode = (int)val == 0 ? ActivationMode::TOGGLE : ((int)val == 1 ? ActivationMode::HOLD : ActivationMode::MOUSE);
            else if (!strcmp(key, "right_mode")) cfg.right_mode = (int)val == 0 ? ActivationMode::TOGGLE : ((int)val == 1 ? ActivationMode::HOLD : ActivationMode::MOUSE);
            else if (!strcmp(key, "slot_auto_enabled")) cfg.slot_auto_enabled = (int)val != 0;
            else if (!strcmp(key, "pause_in_menu")) cfg.pause_in_menu = (int)val != 0;
            else if (!strcmp(key, "right_placeable_only")) cfg.right_placeable_only = (int)val != 0;
            else if (!strcmp(key, "audio_enabled")) cfg.audio_enabled = (int)val != 0;
            else if (!strcmp(key, "macro_count")) { /* counted implicitly */ }
            else {
                bool handled = false;
                for (int i = 0; i < 9 && !handled; i++) {
                    char sbuf[32];
                    snprintf(sbuf, sizeof(sbuf), "left_slot_%d", i);
                    if (!strcmp(key, sbuf)) { cfg.left_slots[i] = (int)val != 0; handled = true; }
                    snprintf(sbuf, sizeof(sbuf), "right_slot_%d", i);
                    if (!strcmp(key, sbuf)) { cfg.right_slots[i] = (int)val != 0; handled = true; }
                }
                for (int m = 0; m < MAX_MACROS && !handled; m++) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "macro_%d.name", m);
                    if (!strcmp(key, buf)) {
                        const char* name_start = strstr(line, buf);
                        if (name_start) {
                            name_start += strlen(buf);
                            while (*name_start == ' ') name_start++;
                            strncpy(cfg.macros[m].name, name_start, 31);
                            cfg.macros[m].name[31] = '\0';
                            char* nl = strchr(cfg.macros[m].name, '\n');
                            if (nl) *nl = '\0';
                        }
                        handled = true; break;
                    }
                    snprintf(buf, sizeof(buf), "macro_%d.loop", m);
                    if (!strcmp(key, buf)) { cfg.macros[m].loop = (int)val != 0; handled = true; break; }
                    snprintf(buf, sizeof(buf), "macro_%d.key_bind", m);
                    if (!strcmp(key, buf)) { cfg.macros[m].key_bind = (int)val; handled = true; break; }
                    snprintf(buf, sizeof(buf), "macro_%d.step_count", m);
                    if (!strcmp(key, buf)) { cfg.macros[m].step_count = std::min((int)val, MAX_MACRO_STEPS); handled = true; break; }
                    for (int s = 0; s < MAX_MACRO_STEPS && !handled; s++) {
                        snprintf(buf, sizeof(buf), "macro_%d.step_%d.action", m, s);
                        if (!strcmp(key, buf)) { cfg.macros[m].steps[s].action = static_cast<MacroAction>((int)val); handled = true; break; }
                        snprintf(buf, sizeof(buf), "macro_%d.step_%d.count", m, s);
                        if (!strcmp(key, buf)) { cfg.macros[m].steps[s].count = (int)val; handled = true; break; }
                        snprintf(buf, sizeof(buf), "macro_%d.step_%d.delay_ms", m, s);
                        if (!strcmp(key, buf)) { cfg.macros[m].steps[s].delay_ms = val; handled = true; break; }
                        snprintf(buf, sizeof(buf), "macro_%d.step_%d.slot", m, s);
                        if (!strcmp(key, buf)) { cfg.macros[m].steps[s].slot = (int)val; handled = true; break; }
                    }
                }
            }
        }
        // accent_preset used to be Oxygen Blue, Red, Green, Purple, Custom, Follow KDE; it is
        // now Follow KDE, Blue, Red, Green, Purple, Orange, Custom. Saved files carry an order
        // marker, so a file without one still holds old numbers and is remapped exactly once.
        if (!accent_order_seen) {
            static const int legacy_to_current[6] = { 1, 2, 3, 4, 6, 0 };
            cfg.accent_preset = (cfg.accent_preset >= 0 && cfg.accent_preset < 6)
                              ? legacy_to_current[cfg.accent_preset] : 0;
        }
        if (cfg.accent_preset < 0 || cfg.accent_preset > 6) cfg.accent_preset = 0;
        fclose(f);
        return true;
    }

    static bool save(const Config& cfg, const std::string& filename = "config") {
        if (!valid_name(filename)) return false;
        std::string dir = path();
        mkdir(dir.c_str(), 0755);

        std::string p = dir + "/" + filename;
        // Written beside the target and renamed into place: truncating the live file first
        // means a crash or a full disk part-way through loses every setting.
        std::string tmp = p + ".tmp";
        FILE* f = fopen(tmp.c_str(), "w");
        if (!f) return false;

        write_profile(f, "left", cfg.left);
        write_profile(f, "right", cfg.right);

        fprintf(f, "key_left %d\n", cfg.key_left);
        fprintf(f, "key_right %d\n", cfg.key_right);
        fprintf(f, "accent_preset %d\n", cfg.accent_preset);
        fprintf(f, "accent_order 1\n");
        fprintf(f, "accent_r %f\n", cfg.accent_r);
        fprintf(f, "accent_g %f\n", cfg.accent_g);
        fprintf(f, "accent_b %f\n", cfg.accent_b);
        fprintf(f, "left_mode %d\n", cfg.left_mode == ActivationMode::TOGGLE ? 0 : (cfg.left_mode == ActivationMode::HOLD ? 1 : 2));
        fprintf(f, "right_mode %d\n", cfg.right_mode == ActivationMode::TOGGLE ? 0 : (cfg.right_mode == ActivationMode::HOLD ? 1 : 2));
        fprintf(f, "slot_auto_enabled %d\n", cfg.slot_auto_enabled ? 1 : 0);
        fprintf(f, "pause_in_menu %d\n", cfg.pause_in_menu ? 1 : 0);
        fprintf(f, "right_placeable_only %d\n", cfg.right_placeable_only ? 1 : 0);
        for (int i = 0; i < 9; i++)
            fprintf(f, "left_slot_%d %d\nright_slot_%d %d\n",
                    i, cfg.left_slots[i] ? 1 : 0, i, cfg.right_slots[i] ? 1 : 0);

        fprintf(f, "audio_enabled %d\n", cfg.audio_enabled ? 1 : 0);

        int macro_count = 0;
        for (int m = 0; m < MAX_MACROS; m++)
            if (cfg.macros[m].step_count > 0 || cfg.macros[m].name[0]) macro_count = m + 1;
        fprintf(f, "macro_count %d\n", macro_count);
        for (int m = 0; m < macro_count; m++) {
            const auto& mac = cfg.macros[m];
            fprintf(f, "macro_%d.name %s\n", m, mac.name[0] ? mac.name : "Untitled");
            fprintf(f, "macro_%d.loop %d\n", m, mac.loop ? 1 : 0);
            fprintf(f, "macro_%d.key_bind %d\n", m, mac.key_bind);
            fprintf(f, "macro_%d.step_count %d\n", m, mac.step_count);
            for (int s = 0; s < mac.step_count; s++) {
                const auto& step = mac.steps[s];
                fprintf(f, "macro_%d.step_%d.action %d\n", m, s, static_cast<int>(step.action));
                fprintf(f, "macro_%d.step_%d.count %d\n", m, s, step.count);
                fprintf(f, "macro_%d.step_%d.delay_ms %g\n", m, s, step.delay_ms);
                fprintf(f, "macro_%d.step_%d.slot %d\n", m, s, step.slot);
            }
        }

        if (fclose(f) != 0) { ::remove(tmp.c_str()); return false; }
        if (::rename(tmp.c_str(), p.c_str()) != 0) { ::remove(tmp.c_str()); return false; }
        if (owner_uid() != static_cast<uid_t>(-1)) {
            (void)chown(p.c_str(), owner_uid(), owner_gid());
            (void)chown(dir.c_str(), owner_uid(), owner_gid());
        }
        return true;
    }

    static std::vector<std::string> list_profiles() {
        std::vector<std::string> result;
        std::string dir = path();
        DIR* d = opendir(dir.c_str());
        if (!d) return result;

        struct dirent* entry;
        while ((entry = readdir(d))) {
            if (entry->d_name[0] == '.') continue;
            // The live config and a stray subdirectory are not profiles: listing them let
            // "Delete" remove the running config itself.
            if (!strcmp(entry->d_name, "config")) continue;
            struct stat st;
            std::string p = dir + "/" + entry->d_name;
            if (stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
            result.push_back(entry->d_name);
        }
        closedir(d);
        return result;
    }

    static bool remove_profile(const std::string& filename) {
        if (!valid_name(filename)) return false;
        std::string p = path() + "/" + filename;
        return ::remove(p.c_str()) == 0;
    }
};
