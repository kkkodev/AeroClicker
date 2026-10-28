#pragma once
#include "clicker_config.hpp"
#include "tracker.hpp"
#include "variance.hpp"
#include "uinput_device.hpp"
#include <atomic>
#include <thread>
#include <chrono>

class Clicker {
    ClickerProfile& profile_;
    const UInputDevice& device_;
    std::atomic<bool> enabled_{false};
    std::atomic<bool> running_{true};
    // Set when the pending start is a MOUSE-mode press, which is the only place the start
    // delay applies (hold the button that long before the train begins, like GhostTap).
    std::atomic<bool> gated_start_{false};
    // Why a clicker is not clicking even though the user switched it on. Several causes apply
    // at once, so they are bits rather than one flag: closing a game screen must not un-pause
    // a clicker whose pointer is sitting on AeroClicker's own window.
    std::atomic<uint32_t> pause_mask_{0};
    std::thread thread_;
    double current_trend_ = 0.0;
    double hold_trend_ = 0.0;

    std::atomic<uint32_t> click_count_{0};
    Tracker tracker_;

public:
    Clicker(ClickerProfile& profile, const UInputDevice& device)
    : profile_(profile), device_(device) {
        thread_ = std::thread(&Clicker::run, this);
    }

    ~Clicker() {
        running_ = false;
        pause_mask_ = 0;
        enabled_ = true;
        if (thread_.joinable()) thread_.join();
    }

    Clicker(const Clicker&) = delete;
    Clicker& operator=(const Clicker&) = delete;

    enum Pause : uint32_t {
        PAUSE_POINTER   = 1u << 0,   // pointer is over AeroClicker's own window
        PAUSE_INVENTORY = 1u << 1,   // the bridge mod reports an open game screen
        PAUSE_HELD_ITEM = 1u << 2,   // the bridge mod reports a non-block in hand
    };
    // Pauses an explicit key press may override: the user asked to run now.
    static constexpr uint32_t PAUSE_AUTO = PAUSE_INVENTORY;
    // Pauses that must keep a physical MOUSE press out of the clicker. If it would not click,
    // the press belongs to the game, and swallowing it would break normal right-click use.
    static constexpr uint32_t PAUSE_NO_TAKEOVER = PAUSE_INVENTORY | PAUSE_HELD_ITEM;

    void set_enabled(bool v) {
        if (!v) gated_start_ = false;
        enabled_ = v;
    }
    // Turning on from a MOUSE-mode press: the first click waits start_delay_ms. Toggle and
    // hold starts use set_enabled() and are never delayed.
    void set_enabled_gated(bool v) {
        gated_start_ = v;
        enabled_ = v;
    }
    bool is_enabled() const { return enabled_; }
    void set_pause(uint32_t reason, bool on) {
        if (on) pause_mask_.fetch_or(reason);
        else    pause_mask_.fetch_and(~reason);
    }
    bool is_paused() const { return pause_mask_.load() != 0; }
    uint32_t pause_mask() const { return pause_mask_.load(); }
    // Synthesized clicks must not hit AeroClicker's own interface (e.g. toggling the L/R
    // pills with the cursor still on top of them): the GUI drives this one directly.
    void set_paused(bool v) { set_pause(PAUSE_POINTER, v); }
    void toggle() { set_enabled(!enabled_.load()); }
    uint32_t click_count() const { return click_count_.load(); }
    ClickerProfile& profile() { return profile_; }
    const Tracker& tracker() const { return tracker_; }
    Tracker& tracker() { return tracker_; }

private:
    void run() {
        using clock = std::chrono::steady_clock;

        while (running_) {
            while (!enabled_ && running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!running_) break;

            // Start delay, MOUSE starts only. Sliced rather than slept in one go so that
            // switching off or pausing cancels it instead of leaving the thread stuck for up
            // to half a second.
            if (gated_start_.exchange(false) && profile_.start_delay_ms > 0.0) {
                const auto deadline = clock::now() + std::chrono::microseconds(
                    static_cast<int64_t>(profile_.start_delay_ms * 1000.0));
                while (enabled_ && running_ && clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                if (!enabled_ || !running_) continue;
            }

            current_trend_ = 0.0;
            hold_trend_ = 0.0;
            bool have_prev_press = false;
            clock::time_point prev_press{};

            auto next_break = clock::now();
            if (profile_.breaks_enabled) {
                double secs = Variance::range(profile_.break_interval_min, profile_.break_interval_max);
                next_break += std::chrono::milliseconds(static_cast<int64_t>(secs * 1000));
            }

            // Press-to-press anchoring: the next press is scheduled from the press that
            // actually happened, not from the end of the previous cycle, so uinput and hold
            // overhead cannot accumulate into a slow drift away from the requested rate.
            auto next_press = clock::now();

            while (enabled_ && running_) {
                if (pause_mask_.load() != 0) {
                    // Resuming must not start with a fatigue break, and a long pause must not
                    // let the break clock expire while nothing is clicking, so the break
                    // timer restarts for as long as the pause lasts.
                    if (profile_.breaks_enabled) {
                        double secs = Variance::range(profile_.break_interval_min,
                                                      profile_.break_interval_max);
                        next_break = clock::now() + std::chrono::milliseconds(
                            static_cast<int64_t>(secs * 1000));
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    // The gap is not a click interval: restart the schedule and forget the
                    // previous press so the resumed click does not record a bogus rate.
                    next_press = clock::now();
                    have_prev_press = false;
                    continue;
                }
                if (profile_.breaks_enabled && clock::now() >= next_break) {
                    double dur = Variance::range(profile_.break_duration_min, profile_.break_duration_max);
                    int64_t dur_ms = static_cast<int64_t>(dur * 1000);
                    auto end = clock::now() + std::chrono::milliseconds(dur_ms);
                    while (enabled_ && running_ && clock::now() < end)
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    double secs = Variance::range(profile_.break_interval_min, profile_.break_interval_max);
                    next_break = clock::now() + std::chrono::milliseconds(static_cast<int64_t>(secs * 1000));
                    next_press = clock::now();
                    have_prev_press = false;
                }

                const int64_t interval_ns = calculate_interval_ns();
                const int64_t hold_ns = calculate_hold_ns(interval_ns);

                // Wait for the scheduled press, checking often enough that switching off
                // stays responsive even on a long (low CPS) interval.
                while (enabled_ && running_ && clock::now() < next_press) {
                    if (next_press - clock::now() > std::chrono::milliseconds(4))
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    else
                        wait_until(next_press);
                }
                // Switched off during the wait: do not emit a press that nothing will release.
                if (!enabled_ || !running_) break;

                device_.click(profile_.mouse_button, 1);
                const auto press_at = clock::now();
                next_press = press_at + std::chrono::nanoseconds(interval_ns);
                click_count_++;
                wait_ns(hold_ns);
                device_.click(profile_.mouse_button, 0);

                // Real press-to-press rate. The first click after a start, pause or break has
                // no predecessor, so it reports the target instead of a multi-second gap.
                double actual_cps = 1'000'000'000.0 / static_cast<double>(interval_ns);
                if (have_prev_press) {
                    const double measured_s =
                        std::chrono::duration<double>(press_at - prev_press).count();
                    if (measured_s > 1e-6) actual_cps = 1.0 / measured_s;
                }
                auto last_press = press_at;

                if (profile_.double_click_chance > 0 && Variance::chance(profile_.double_click_chance)) {
                    double delay_ms = Variance::range(profile_.double_click_delay_min, profile_.double_click_delay_max);
                    // The next beat is re-anchored from the second press, so a doublet costs
                    // hold + delay. Capping the delay at interval - hold keeps two clicks
                    // inside at most two intervals, which is exactly the condition for the
                    // doublet never to pull the average rate below the profile's own.
                    // Below ~5 ms there is no room for a pair, so the doublet is skipped.
                    const double cap_ms = static_cast<double>(interval_ns - hold_ns) / 1'000'000.0;
                    if (delay_ms > cap_ms) delay_ms = cap_ms;
                    if (delay_ms >= 5.0) {
                        wait_ns(static_cast<int64_t>(delay_ms * 1'000'000));
                        int64_t dbl_hold = calculate_hold_ns(interval_ns);
                        device_.click(profile_.mouse_button, 1);
                        last_press = clock::now();
                        click_count_++;
                        wait_ns(dbl_hold);
                        device_.click(profile_.mouse_button, 0);
                        // The second click starts its own cycle, so it does not turn the next
                        // scheduled click into a burst.
                        next_press = last_press + std::chrono::nanoseconds(interval_ns);
                    }
                }

                prev_press = last_press;
                have_prev_press = true;

                tracker_.record(
                    1'000'000'000.0 / static_cast<double>(interval_ns),
                    actual_cps, static_cast<double>(hold_ns),
                    static_cast<double>(interval_ns), current_trend_);
            }
        }
    }

    int64_t calculate_interval_ns() {
        current_trend_ = Variance::trend(current_trend_, profile_.rhythm_volatility, profile_.rhythm_tension);

        double cps;
        switch (profile_.distribution) {
            case DistributionType::UNIFORM:
                cps = Variance::range(profile_.cps_min, profile_.cps_max) + current_trend_;
                break;
            default:
                cps = Variance::gaussian(profile_.cps_mean + current_trend_, profile_.cps_stddev);
                break;
        }

        if (Variance::chance(profile_.spike_chance))
            cps += Variance::range(profile_.spike_min, profile_.spike_max);

        if (Variance::chance(profile_.stutter_chance))
            cps -= Variance::range(profile_.stutter_min, profile_.stutter_max);

        if (cps > profile_.cps_max)
            cps = profile_.cps_max + Variance::range(-profile_.cps_minmax_fallout, profile_.cps_minmax_fallout);
        if (cps < profile_.cps_min)
            cps = profile_.cps_min + Variance::range(-profile_.cps_minmax_fallout, profile_.cps_minmax_fallout);
        if (cps < 0.5) cps = 0.5;

        return static_cast<int64_t>(1'000'000'000.0 / cps);
    }

    int64_t calculate_hold_ns(int64_t interval_ns) {
        const double cps = 1'000'000'000.0 / static_cast<double>(interval_ns);
        // Holds shorten as the rate rises (fingers move faster), but never below 80% of the
        // mean. The old form, max(0.8, 1 - cps/25), was below 0.8 for every rate above 5 CPS,
        // so it collapsed to a constant 0.8 and the factor never actually varied. Dividing by
        // 125 puts the 0.8 floor where it was meant to start: 25 CPS.
        const double speed_factor = std::min(1.0, std::max(0.8, 1.0 - cps / 125.0));

        // Slow grip/tension drift, so consecutive holds are correlated instead of being
        // independent draws around a fixed mean.
        hold_trend_ = Variance::trend(hold_trend_, profile_.hold_drift_ms,
                                      profile_.hold_drift_tension);
        const double adjusted_mean = profile_.hold_ms_mean * speed_factor + hold_trend_;

        double hold = Variance::gaussian(adjusted_mean, profile_.hold_ms_stddev);

        if (Variance::chance(profile_.hold_ms_heavy_chance))
            hold += Variance::range(profile_.hold_ms_heavy_min, profile_.hold_ms_heavy_max);

        hold = std::max(profile_.hold_ms_min, std::min(profile_.hold_ms_max, hold));

        int64_t hold_ns = static_cast<int64_t>(hold * 1'000'000);
        int64_t max_hold = static_cast<int64_t>(interval_ns * 0.85);

        return std::min(hold_ns, max_hold);
    }

    // Sleep most of the wait, then spin the tail for precision. Takes an absolute deadline
    // so a schedule can be anchored to a real event instead of to "now".
    static void wait_until(std::chrono::steady_clock::time_point target) {
        auto sleep_until = target - std::chrono::microseconds(1500);
        if (std::chrono::steady_clock::now() < sleep_until)
            std::this_thread::sleep_until(sleep_until);
        while (std::chrono::steady_clock::now() < target)
            std::this_thread::yield();
    }

    void wait_ns(int64_t ns) {
        if (ns <= 0) return;
        wait_until(std::chrono::steady_clock::now() + std::chrono::nanoseconds(ns));
    }
};
