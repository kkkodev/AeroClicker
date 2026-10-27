#pragma once
#include "clicker.hpp"
#include "clicker_config.hpp"
#include "config_file.hpp"
#include "macro.hpp"
#include <atomic>
#include <chrono>
#include <vector>
#include <string>

struct GuiState {
    Config& config;
    Clicker& left;
    Clicker& right;
    MacroRunner macro_runner;

    bool left_armed = false;
    bool right_armed = false;
    // Written by the input thread (the inventory key) and by the GUI (the L/R pills), read by
    // both, so it is atomic.
    std::atomic<bool> in_menu{false};
    // Written by the input thread, read by the UI thread: keep it atomic so the label can
    // never keep showing a stale value.
    std::atomic<int> active_slot{0};
    // Set when the user switches a clicker on with its bound key or its pill while the active
    // hotbar slot is filtered out, so that explicit press beats the filter. Handed back to the
    // filter on the next slot change (see apply_pauses in main.cpp).
    std::atomic<bool> left_slot_override{false};
    std::atomic<bool> right_slot_override{false};
    // Written by the UI thread, read by the input thread: while the pointer is over the app,
    // physical button presses belong to the interface, not to the clicker's MOUSE arming.
    std::atomic<bool> ui_hovered{false};

    bool show_advanced = false;
    bool show_debug = false;

    int selected_macro = 0;

    std::chrono::steady_clock::time_point session_start = std::chrono::steady_clock::now();
    double active_time_s = 0.0;
    double peak_cps_left = 0.0;
    double peak_cps_right = 0.0;

    GuiState(Config& c, Clicker& l, Clicker& r, const UInputDevice& dev)
    : config(c), left(l), right(r), macro_runner(dev) {}
};

void gui_run(GuiState& state);
