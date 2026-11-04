#pragma once
#include "clicker_config.hpp"
#include "uinput_device.hpp"
#include "variance.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>

// Minecraft samples input once per 50ms client tick.
constexpr double TICK_MS = 50.0;

// Runs one delayed job at a time on its own thread. cancel() drops a pending job and wakes any
// sleep_ms() inside the running one.
class Scheduler {
    using clock = std::chrono::steady_clock;

    std::mutex mu_;
    std::condition_variable cv_;
    std::function<void()> job_;
    clock::time_point at_;
    uint64_t generation_ = 0;
    uint64_t job_generation_ = 0;
    bool busy_ = false;
    bool running_ = true;
    std::thread thread_;

public:
    Scheduler() { thread_ = std::thread(&Scheduler::run, this); }

    ~Scheduler() {
        {
            std::lock_guard<std::mutex> lock(mu_);
            running_ = false;
            generation_++;
        }
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // Returns false while another job is pending or running.
    bool schedule(double delay_ms, std::function<void()> job) {
        std::lock_guard<std::mutex> lock(mu_);
        if (job_ || busy_) return false;
        job_ = std::move(job);
        at_ = clock::now() + std::chrono::microseconds(static_cast<int64_t>(delay_ms * 1000));
        cv_.notify_all();
        return true;
    }

    void cancel() {
        {
            std::lock_guard<std::mutex> lock(mu_);
            job_ = nullptr;
            generation_++;
        }
        cv_.notify_all();
    }

    bool idle() {
        std::lock_guard<std::mutex> lock(mu_);
        return !job_ && !busy_;
    }

    // For use inside a job. Returns false if the job was cancelled meanwhile.
    bool sleep_ms(double ms) {
        auto until = clock::now() + std::chrono::microseconds(static_cast<int64_t>(ms * 1000));
        std::unique_lock<std::mutex> lock(mu_);
        return !cv_.wait_until(lock, until, [&] { return generation_ != job_generation_ || !running_; });
    }

private:
    void run() {
        std::unique_lock<std::mutex> lock(mu_);
        while (running_) {
            if (!job_) { cv_.wait(lock); continue; }
            if (clock::now() < at_) { cv_.wait_until(lock, at_); continue; }

            auto job = std::move(job_);
            job_ = nullptr;
            busy_ = true;
            job_generation_ = generation_;
            lock.unlock();
            job();
            lock.lock();
            busy_ = false;
        }
    }
};

// Tracks the physical keyboard and owns the virtual one. While forwarding (physical keyboard
// grabbed), every physical key goes out through the virtual device, which is what lets W-Tap
// release W while you still hold it.
class VirtualKeyboard {
    static constexpr int N = UInputKeyboard::MAX_CODE + 1;

    UInputKeyboard dev_;
    std::mutex mu_;
    bool physical_[N] = {};
    bool virtual_[N] = {};
    bool masked_[N] = {};
    // A release that could not be queued. The key stays marked down in virtual_ and is retried
    // until the kernel accepts it: a lost release is exactly what leaves a movement key stuck.
    bool owed_release_[N] = {};
    int owed_count_ = 0;
    // Keys this layer has pressed at least once. Used to re-assert key-up without spraying
    // releases for the whole 255-key range.
    bool ever_down_[N] = {};
    bool forwarding_ = false;

    // Must be called with mu_ held.
    bool set(int code, bool down) {
        if (virtual_[code] == down) {
            // Pressing a key supersedes a release that was still owed for it.
            if (down && owed_release_[code]) {
                owed_release_[code] = false;
                owed_count_--;
            }
            return true;
        }
        if (!dev_.key(code, down ? 1 : 0)) {
            if (!down && !owed_release_[code]) {
                owed_release_[code] = true;
                owed_count_++;
            }
            return false;
        }
        virtual_[code] = down;
        if (down) ever_down_[code] = true;
        if (owed_release_[code]) {
            // The key is now in the state we wanted, so the release debt is settled.
            owed_release_[code] = false;
            owed_count_--;
        }
        return true;
    }

    // Must be called with mu_ held.
    void flush_owed_locked() {
        if (owed_count_ == 0) return;
        for (int i = 1; i < N; i++) {
            if (!owed_release_[i]) continue;
            if (virtual_[i]) set(i, false);
            else if (dev_.key(i, 0)) {
                // A phantom release (release_unheld) is not tracked in virtual_, so clear
                // the debt here once the kernel accepts it.
                owed_release_[i] = false;
                owed_count_--;
            }
        }
    }

public:
    void on_physical(int code, int value) {
        if (code <= 0 || code >= N) return;
        std::lock_guard<std::mutex> lock(mu_);
        physical_[code] = value != 0;
        if (value != 0 && owed_release_[code]) {
            // The key is held again, so a release that was still owed is stale.
            owed_release_[code] = false;
            owed_count_--;
        }
        if (!forwarding_ || masked_[code]) return;
        if (value == 2) {
            // A repeat means the key is still held. If the original press never made it out
            // (a full queue), use the repeat to try again rather than leaving the game deaf
            // to a key that is still down.
            if (virtual_[code]) dev_.key(code, 2);
            else set(code, true);
        } else {
            set(code, value != 0);
        }
    }

    void sync_physical(const bool* down) {
        std::lock_guard<std::mutex> lock(mu_);
        for (int i = 1; i < N; i++) physical_[i] = down[i];
    }

    // The kernel's event queue overflowed (SYN_DROPPED), so events between the last one read
    // and now are gone. The device's key state is still authoritative: mirror it and make the
    // virtual keyboard agree again, because a missed release would leave the key held and a
    // missed press would leave the game deaf to a key that is still down.
    void resync(const bool* down) {
        std::lock_guard<std::mutex> lock(mu_);
        for (int i = 1; i < N; i++) {
            physical_[i] = down[i];
            if (!forwarding_ || masked_[i]) continue;
            if (virtual_[i] != down[i]) set(i, down[i]);
        }
    }

    bool physical(int code) {
        std::lock_guard<std::mutex> lock(mu_);
        return code > 0 && code < N && physical_[code];
    }

    // Retry any release the kernel refused earlier. Called between polls, so a transient full
    // uinput queue costs at most one iteration instead of a permanently stuck key.
    void retry_owed() {
        std::lock_guard<std::mutex> lock(mu_);
        flush_owed_locked();
    }

    // Re-send key-up for keys this layer has forwarded and no longer holds anything on. A game
    // can lose a release during a hitch and keep moving on a key that is already up; a
    // redundant release is harmless, so a toggle press is a safe moment to clear that phantom.
    // A key the user is still holding (physical_) and a key a module is holding (virtual_) are
    // left alone.
    void release_unheld() {
        std::lock_guard<std::mutex> lock(mu_);
        if (!forwarding_) return;
        for (int i = 1; i < N; i++) {
            if (!ever_down_[i] || physical_[i] || virtual_[i]) continue;
            if (!dev_.key(i, 0) && !owed_release_[i]) {
                owed_release_[i] = true;
                owed_count_++;
            }
        }
    }

    void set_forwarding(bool v) {
        std::lock_guard<std::mutex> lock(mu_);
        if (!v) {
            // A mask hides a key from the game, so it must not survive the handover: the next
            // grab starts from the physical state again.
            for (int i = 1; i < N; i++) masked_[i] = false;
            for (int i = 1; i < N; i++)
                if (virtual_[i]) set(i, false);
            flush_owed_locked();
        }
        forwarding_ = v;
    }

    bool forwarding() {
        std::lock_guard<std::mutex> lock(mu_);
        return forwarding_;
    }

    void press(int code) {
        std::lock_guard<std::mutex> lock(mu_);
        set(code, true);
    }

    void release(int code) {
        std::lock_guard<std::mutex> lock(mu_);
        set(code, false);
    }

    // Hide a held key from the game until unmask(); only works while forwarding.
    bool mask(int code) {
        std::lock_guard<std::mutex> lock(mu_);
        if (!forwarding_) return false;
        masked_[code] = true;
        set(code, false);
        return true;
    }

    void unmask(int code) {
        std::lock_guard<std::mutex> lock(mu_);
        masked_[code] = false;
        if (forwarding_) set(code, physical_[code]);
    }
};

// What the caller knows about the game when the bridge mod reports an event.
struct CombatContext {
    bool in_menu;
    bool slot_blocked;
    bool in_water;
    bool on_ground;
    // How long ago the bridge last reported the player on the ground. Large when no sample has
    // been seen, so a fresh link cannot grant a grace window it did not observe.
    double ms_since_ground;
    bool hit_from_behind;   // the hurt event being handled came from behind the player
};

inline double combat_clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

inline double combat_gaussian(double mean, double stddev, double lo, double hi) {
    double v = stddev > 0 ? Variance::gaussian(mean, stddev) : mean;
    return combat_clamp(v, std::min(lo, hi), std::max(lo, hi));
}

// Human reaction times are not symmetric: most land just after the minimum and a few are much
// slower. A log-normal draw keeps the configured mean and spread but gives that right tail,
// where a Gaussian would either clip it at the max or put just as many values below the mean.
inline double combat_reaction(double mean, double stddev, double lo, double hi) {
    const double a = std::min(lo, hi), b = std::max(lo, hi);
    if (mean <= 0.0 || stddev <= 0.0) return combat_clamp(mean, a, b);
    const double sigma2 = std::log(1.0 + (stddev * stddev) / (mean * mean));
    const double mu = std::log(mean) - sigma2 * 0.5;
    const double v = std::exp(mu + std::sqrt(sigma2) * Variance::gaussian(0.0, 1.0));
    return combat_clamp(v, a, b);
}

// Shared cooldown/chance gate. The cooldown stands in for hit confirmation: Minecraft only lets
// a player take a hit every 10 ticks, so one decision per window matches how often a real hit
// lands.
class AttackGate {
    using clock = std::chrono::steady_clock;
    std::mutex mu_;
    clock::time_point last_{};

public:
    // True if this attack starts a new decision window.
    bool claim(double cooldown_ms) {
        std::lock_guard<std::mutex> lock(mu_);
        auto now = clock::now();
        if (now - last_ < std::chrono::microseconds(static_cast<int64_t>(cooldown_ms * 1000)))
            return false;
        last_ = now;
        return true;
    }
};

// Why a reset event did or did not act. A hit a filter dropped and a hit the bridge never
// delivered look exactly the same without it.
enum class ResetOutcome : int {
    none = 0, fired, busy, menu, slot, behind, moving, forward, water, air, chance, cooldown,
    space_held, key_missing, key_held
};
constexpr int kResetOutcomeCount = static_cast<int>(ResetOutcome::key_held) + 1;

inline const char* reset_outcome_label(ResetOutcome o) {
    switch (o) {
        case ResetOutcome::fired:       return "fired";
        case ResetOutcome::busy:        return "busy";
        case ResetOutcome::menu:        return "menu";
        case ResetOutcome::slot:        return "slot";
        case ResetOutcome::behind:      return "behind";
        case ResetOutcome::moving:      return "not moving";
        case ResetOutcome::forward:     return "not forward";
        case ResetOutcome::water:       return "water";
        case ResetOutcome::air:         return "mid-air";
        case ResetOutcome::chance:      return "chance";
        case ResetOutcome::cooldown:    return "cooldown";
        case ResetOutcome::space_held:  return "space held";
        case ResetOutcome::key_missing: return "key released";
        case ResetOutcome::key_held:    return "key held";
        default:                        return "none";
    }
}

inline const char* reset_outcome_text(ResetOutcome o) {
    switch (o) {
        case ResetOutcome::fired:       return "fired";
        case ResetOutcome::busy:        return "still busy from the last action";
        case ResetOutcome::menu:        return "game menu open";
        case ResetOutcome::slot:        return "slot filter";
        case ResetOutcome::behind:      return "hit from behind";
        case ResetOutcome::moving:      return "not moving";
        case ResetOutcome::forward:     return "not moving forward";
        case ResetOutcome::water:       return "in water";
        case ResetOutcome::air:         return "mid-air";
        case ResetOutcome::chance:      return "chance roll";
        case ResetOutcome::cooldown:    return "cooldown";
        case ResetOutcome::space_held:  return "Space already held";
        case ResetOutcome::key_missing: return "the key was released first";
        case ResetOutcome::key_held:    return "the key was already held";
        default:                        return "no event yet";
    }
}

// Fired-of-events tally shared by both resets: every event ends as an action or as a named
// reason, so the two numbers on a panel always add up.
class ResetStats {
    std::atomic<long long> events_{0};
    std::atomic<int> last_{0};
    std::atomic<int> skips_[kResetOutcomeCount];

public:
    ResetStats() {
        for (auto& s : skips_) s = 0;
    }

    void begin_event() { events_.fetch_add(1); }
    void note(ResetOutcome o) {
        last_ = static_cast<int>(o);
        if (o != ResetOutcome::fired && o != ResetOutcome::none)
            skips_[static_cast<int>(o)].fetch_add(1);
    }
    void clear() {
        events_ = 0;
        last_ = 0;
        for (auto& s : skips_) s = 0;
    }
    long long event_count() const { return events_.load(); }
    int skip_count(ResetOutcome o) const { return skips_[static_cast<int>(o)].load(); }
    const char* last_text() const {
        return reset_outcome_text(static_cast<ResetOutcome>(last_.load()));
    }

    // "cooldown 9, chance 4": only the reasons that actually cost an action, in enum order.
    void summary(char* out, size_t n) const {
        if (n == 0) return;
        out[0] = '\0';
        size_t used = 0;
        for (int i = static_cast<int>(ResetOutcome::busy); i < kResetOutcomeCount; i++) {
            const int count = skips_[i].load();
            if (count <= 0) continue;
            const int wrote = std::snprintf(out + used, n - used, "%s%s %d",
                                            used ? ", " : "",
                                            reset_outcome_label(static_cast<ResetOutcome>(i)),
                                            count);
            if (wrote < 0 || static_cast<size_t>(wrote) >= n - used) return;
            used += static_cast<size_t>(wrote);
        }
    }
};

// Presses Space shortly after the game reports that an entity hurt the player, so the jump lines
// up with the knockback you take in a trade. Environmental damage never reaches this module.
class JumpReset {
    const JumpResetProfile& p_;
    VirtualKeyboard& kb_;
    Scheduler sched_;
    AttackGate gate_;
    std::atomic<bool> enabled_{false};
    std::atomic<uint32_t> fires_{0};
    ResetStats stats_;

public:
    JumpReset(const JumpResetProfile& p, VirtualKeyboard& kb) : p_(p), kb_(kb) {}

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool v) {
        enabled_ = v;
        if (v) clear_stats();
        else sched_.cancel();
    }
    void toggle() { set_enabled(!enabled_.load()); }
    bool busy() { return !sched_.idle(); }
    long long event_count() const { return stats_.event_count(); }
    void clear_stats() {
        stats_.clear();
        fires_ = 0;
    }
    // "cooldown 9, chance 4": only the reasons that actually cost a press.
    void skip_summary(char* out, size_t n) const { stats_.summary(out, n); }
    // Analytics/Debug: how often the module actually pressed Space.
    uint32_t fire_count() const { return fires_.load(); }
    void reset_fire_count() { fires_ = 0; }

    const char* outcome_text() const { return stats_.last_text(); }

    void on_hurt(const CombatContext& ctx) {
        if (!enabled_) return;
        stats_.begin_event();
        if (!sched_.idle()) { stats_.note(ResetOutcome::busy); return; }
        // Filters first: an event the module would ignore must not burn its cooldown window,
        // or the next real hit after a menu closes or you start moving would still wait.
        if (ctx.in_menu && !p_.allow_in_menu) { stats_.note(ResetOutcome::menu); return; }
        if (ctx.slot_blocked && p_.use_slot_filter) { stats_.note(ResetOutcome::slot); return; }
        // A hit from behind is not a trade, so jumping would only give the attacker a cleaner
        // follow-up. Ignore the event before the cooldown is claimed.
        if (ctx.hit_from_behind) { stats_.note(ResetOutcome::behind); return; }
        if (p_.moving_only && !(kb_.physical(KEY_W) || kb_.physical(KEY_A) ||
                                kb_.physical(KEY_S) || kb_.physical(KEY_D))) {
            stats_.note(ResetOutcome::moving);
            return;
        }
        if (p_.forward_only && !kb_.physical(KEY_W)) {
            stats_.note(ResetOutcome::forward);
            return;
        }
        if (p_.disable_in_water && ctx.in_water) { stats_.note(ResetOutcome::water); return; }
        // The bridge latches the posture before the hit, but a hit can still land just after
        // the player leaves the floor, or the previous sample can be one tick too old. The
        // grace keeps a hit that was on the ground a moment ago, while a hit taken after
        // actually leaving the floor is still ignored once the window has passed.
        if (p_.require_on_ground && !ctx.on_ground &&
            !(p_.ground_grace_ms > 0.0 && ctx.ms_since_ground <= p_.ground_grace_ms)) {
            stats_.note(ResetOutcome::air);
            return;
        }
        if (!gate_.claim(p_.cooldown_ms)) { stats_.note(ResetOutcome::cooldown); return; }

        double probability = combat_clamp(p_.chance, 0, 1);
        if (probability <= 0 || (probability < 1 && !Variance::chance(probability))) {
            stats_.note(ResetOutcome::chance);
            return;
        }

        stats_.note(ResetOutcome::fired);
        double delay = std::round(combat_reaction(p_.delay_ms_mean, p_.delay_ms_stddev,
                                                  p_.delay_ms_min, p_.delay_ms_max));
        sched_.schedule(std::max(0.0, delay), [this, delay] { press(delay); });
    }

private:
    void press(double delay) {
        (void)delay;
        if (!enabled_) return;
        // Pressing Space you already hold does nothing, so a decided press that lands here is
        // still a lost one and has to appear in the tally.
        if (kb_.physical(KEY_SPACE)) { stats_.note(ResetOutcome::space_held); return; }

        double hold = std::round(combat_gaussian(p_.hold_ms_mean, p_.hold_ms_stddev,
                                                 p_.hold_ms_min, p_.hold_ms_max));
        kb_.press(KEY_SPACE);
        fires_.fetch_add(1);
        sched_.sleep_ms(hold);
        kb_.release(KEY_SPACE);
    }
};

// Briefly breaks sprint after the game reports that you hit a living entity (W release, S press
// or Shift press) for whole 50ms ticks, so your next hit deals full sprint knockback.
class SprintReset {
    const SprintResetProfile& p_;
    VirtualKeyboard& kb_;
    Scheduler sched_;
    AttackGate gate_;
    std::atomic<bool> enabled_{false};
    std::atomic<uint32_t> fires_{0};
    ResetStats stats_;

public:
    SprintReset(const SprintResetProfile& p, VirtualKeyboard& kb) : p_(p), kb_(kb) {}

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool v) {
        enabled_ = v;
        if (v) clear_stats();
        else sched_.cancel();
    }
    void toggle() { set_enabled(!enabled_.load()); }
    bool busy() { return !sched_.idle(); }
    // Analytics/Debug: how often the module actually performed its tap.
    uint32_t fire_count() const { return fires_.load(); }
    void reset_fire_count() { fires_ = 0; }
    long long event_count() const { return stats_.event_count(); }
    void clear_stats() {
        stats_.clear();
        fires_ = 0;
    }
    const char* outcome_text() const { return stats_.last_text(); }
    void skip_summary(char* out, size_t n) const { stats_.summary(out, n); }

    // W-Tap is the only mode that has to hide a key you're physically holding.
    bool needs_keyboard_grab() const { return enabled_ && p_.mode == SprintResetMode::W_TAP; }

    void on_hit(const CombatContext& ctx) {
        if (!enabled_) return;
        stats_.begin_event();
        if (!sched_.idle()) { stats_.note(ResetOutcome::busy); return; }
        // Filters first: see JumpReset::on_hurt.
        if (ctx.in_menu) { stats_.note(ResetOutcome::menu); return; }
        if (ctx.slot_blocked && p_.use_slot_filter) {
            stats_.note(ResetOutcome::slot);
            return;
        }
        bool need_w = p_.forward_only || p_.mode == SprintResetMode::W_TAP;
        if (need_w && !kb_.physical(KEY_W)) { stats_.note(ResetOutcome::forward); return; }
        if (!gate_.claim(p_.cooldown_ms)) { stats_.note(ResetOutcome::cooldown); return; }

        double probability = combat_clamp(p_.chance, 0, 1);
        if (probability <= 0 || (probability < 1 && !Variance::chance(probability))) {
            stats_.note(ResetOutcome::chance);
            return;
        }

        double lo = combat_clamp(std::min(p_.delay_ms_min, p_.delay_ms_max), 0, 500);
        double hi = combat_clamp(std::max(p_.delay_ms_min, p_.delay_ms_max), lo, 500);
        double delay = std::round(Variance::range(lo, hi));
        SprintResetMode mode = p_.mode;
        if (!sched_.schedule(delay, [this, delay, mode] { tap(delay, mode); }))
            stats_.note(ResetOutcome::busy);
        else
            stats_.note(ResetOutcome::fired);
    }

private:
    int sample_ticks() const {
        double lo = combat_clamp(p_.duration_ticks_min, 1, 3);
        double hi = combat_clamp(p_.duration_ticks_max, lo, 3);
        double base = combat_gaussian(combat_clamp(p_.duration_ticks_mean, lo, hi),
                                      combat_clamp(p_.duration_ticks_stddev, 0, 1), lo, hi);
        // Stochastic rounding keeps the configured mean instead of always snapping to the
        // nearest whole tick.
        int ticks = static_cast<int>(std::floor(base));
        double frac = base - ticks;
        if (frac > 0 && Variance::chance(frac)) ticks++;
        return std::max(static_cast<int>(std::round(lo)),
                        std::min(static_cast<int>(std::round(hi)), ticks));
    }

    void tap(double delay, SprintResetMode mode) {
        (void)delay;
        if (!enabled_) return;

        double ms = sample_ticks() * TICK_MS;
        // Whole ticks only give 50 ms or 100 ms taps. Randomising around that by a few
        // milliseconds keeps the release off the same tick boundary every time, while the floor
        // guarantees a full tick so the game actually samples the key as released.
        const double jitter = combat_clamp(p_.tap_jitter_ms, 0.0, 40.0);
        if (jitter > 0.0)
            ms = std::max(TICK_MS, ms + Variance::range(-jitter, jitter));
        if (mode == SprintResetMode::W_TAP) {
            if (!kb_.physical(KEY_W)) { stats_.note(ResetOutcome::key_missing); return; }
            if (!kb_.mask(KEY_W)) { stats_.note(ResetOutcome::key_missing); return; }
            fires_.fetch_add(1);
            sched_.sleep_ms(ms);
            kb_.unmask(KEY_W);
        } else {
            int key = mode == SprintResetMode::S_TAP ? KEY_S : KEY_LEFTSHIFT;
            if (kb_.physical(key)) { stats_.note(ResetOutcome::key_held); return; }
            kb_.press(key);
            fires_.fetch_add(1);
            sched_.sleep_ms(ms);
            kb_.release(key);
        }
    }
};

// Owns the virtual keyboard and both reset modules. Constructing it creates the uinput
// keyboard, so it throws when /dev/uinput is not accessible.
class Combat {
public:
    VirtualKeyboard keyboard;
    JumpReset jump;
    SprintReset sprint;

    explicit Combat(Config& cfg)
        : jump(cfg.jump_reset, keyboard), sprint(cfg.sprint_reset, keyboard) {}

    bool any_enabled() const { return jump.is_enabled() || sprint.is_enabled(); }

    // Two separate entry points, because the game reports the two events separately: a hurt
    // event may arrive without an attack (you got hit) and an attack without a hurt (you hit
    // someone else).
    void on_hurt(const CombatContext& ctx) { jump.on_hurt(ctx); }
    void on_hit(const CombatContext& ctx)  { sprint.on_hit(ctx); }
};
