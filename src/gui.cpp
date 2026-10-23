#include "gui.hpp"
#include "gui_theme.hpp"
#include "gui_widgets.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <cstring>
#include <cstdlib>

static const char* TAB_NAMES[] = { "Home", "Left", "Right", "Macros", "Settings", "Analytics" };
static const int TAB_COUNT = 6;

static int s_applied_preset[2] = { -1, -1 };
static char s_font_path[256]   = "";
static float s_font_scale      = 0.0f;

static void preset_flow(const ClickerPreset* presets, ClickerProfile& profile, int side, float inner_w) {
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float gap = ImGui::GetStyle().ItemSpacing.y;
    const float row_x = ImGui::GetCursorPosX();
    const float btn_h = ImGui::GetFrameHeight();
    float line_y = ImGui::GetCursorPosY();
    float x_used = 0.0f;
    for (int i = 0; i < PRESET_COUNT; i++) {
        ImVec2 pls = ImGui::CalcTextSize(presets[i].name, nullptr, true);
        float bw = pls.x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (i > 0) {
            if (x_used + sp + bw > inner_w) {
                line_y += btn_h + gap;
                ImGui::SetCursorPos(ImVec2(row_x, line_y));
                x_used = 0.0f;
            } else { ImGui::SameLine(); x_used += sp; }
        }
        bool applied = (s_applied_preset[side] == i);
        if (oxygen_button(presets[i].name, ImVec2(0, 0), 0, 0, applied)) {
            int saved = profile.mouse_button;
            profile = presets[i].profile;
            profile.mouse_button = saved;
            s_applied_preset[side] = i;
        }
        x_used += bw;
    }
    ImGui::SetCursorPosY(line_y + btn_h);   // no trailing gap: bottom pad == top pad
}

static int mode_to_idx(ActivationMode m) { return m == ActivationMode::TOGGLE ? 0 : (m == ActivationMode::HOLD ? 1 : 2); }
static ActivationMode idx_to_mode(int i) { return i == 0 ? ActivationMode::TOGGLE : (i == 1 ? ActivationMode::HOLD : ActivationMode::MOUSE); }

// ---------------- Home ----------------

static void draw_clicker_card(const char* title, ClickerProfile& profile, ActivationMode& mode,
                              Clicker& clicker, int& key, const ClickerPreset* presets, int side,
                              float card_w, bool on) {
    begin_panel(title, card_w, on ? 1 : 0);
    float inner = panel_inner();
    stat_readout(on, clicker.tracker().avg_cps(), clicker.click_count(), inner);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    stacked_keybind("Toggle key", key, inner);
    int idx = mode_to_idx(mode);
    if (stacked_combo("Activation", &idx, "Toggle\0Hold\0Mouse\0", inner)) mode = idx_to_mode(idx);
    stacked_slider("Speed (CPS)", &profile.cps_mean, CPS_MEAN_LO, CPS_MEAN_HI, "%.1f", inner);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    preset_flow(presets, profile, side, inner);
    end_panel();
                              }

static void draw_home_tab(GuiState& state) {
    ImGui::Spacing();
    const float gap = 12.0f;
    float card_w = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    ImVec2 c0 = ImGui::GetCursorScreenPos();
    bool l_on = state.left.is_enabled() || state.left_armed;
    bool r_on = state.right.is_enabled() || state.right_armed;
    draw_clicker_card("Left clicker", state.config.left, state.config.left_mode,
                      state.left, state.config.key_left, LEFT_PRESETS, 0, card_w, l_on);
    float y_left = ImGui::GetCursorScreenPos().y;
    ImGui::SetCursorScreenPos(ImVec2(c0.x + card_w + gap, c0.y));
    draw_clicker_card("Right clicker", state.config.right, state.config.right_mode,
                      state.right, state.config.key_right, RIGHT_PRESETS, 1, card_w, r_on);
    float y_right = ImGui::GetCursorScreenPos().y;

    // fresh full-width row below BOTH cards, tip centered
    float y = y_left > y_right ? y_left : y_right;
    const char* tip = "Tip: press the toggle key anywhere in-game to start or stop clicking.";
    ImVec2 ts = ImGui::CalcTextSize(tip, nullptr, true);
    float full_w = card_w * 2.0f + gap;
    float x = c0.x + (full_w - ts.x) * 0.5f;
    if (x < c0.x) x = c0.x;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::TextDisabled("%s", tip);
}

// ---------------- Left / Right (advanced) ----------------

static void draw_side_tab(ClickerProfile& p) {
    ImGui::Spacing();
    begin_panel("Rate");
    row_slider("Speed (CPS)", &p.cps_mean, CPS_MEAN_LO, CPS_MEAN_HI, "%.1f");
    row_slider("CPS Variation", &p.cps_stddev, 0.0, 5.0, "%.2f");
    row_slider("Min CPS", &p.cps_min, CPS_MEAN_LO, CPS_MIN_HI, "%.1f");
    row_slider("Max CPS", &p.cps_max, 5.0, CPS_MAX_HI, "%.1f");
    int d = (int)p.distribution;
    if (row_combo("Distribution", &d, "Gaussian\0Poisson\0Uniform\0")) p.distribution = (DistributionType)d;
    row_slider("Start Delay (ms)", &p.start_delay_ms, 0.0, 500.0, "%.0f");
    end_panel();

    begin_panel("Humanization");
    row_slider("Spike Chance", &p.spike_chance, 0.0, 0.2, "%.3f");
    if (p.spike_chance > 0) { row_slider("Spike Min", &p.spike_min, 0.0, 5.0, "%.1f"); row_slider("Spike Max", &p.spike_max, 0.0, 8.0, "%.1f"); }
    row_slider("Stutter Chance", &p.stutter_chance, 0.0, 0.2, "%.3f");
    if (p.stutter_chance > 0) { row_slider("Stutter Min", &p.stutter_min, 0.0, 8.0, "%.1f"); row_slider("Stutter Max", &p.stutter_max, 0.0, 10.0, "%.1f"); }
    row_slider("Rhythm Volatility", &p.rhythm_volatility, 0.0, 2.0, "%.2f");
    row_slider("Rhythm Tension", &p.rhythm_tension, 0.0, 0.2, "%.3f");
    row_slider("CPS Fallout", &p.cps_minmax_fallout, 0.0, 3.0, "%.2f");
    end_panel();

    begin_panel("Hold timing");
    row_slider("Hold Mean (ms)", &p.hold_ms_mean, 10.0, 100.0, "%.1f");
    row_slider("Hold StdDev (ms)", &p.hold_ms_stddev, 0.0, 20.0, "%.1f");
    row_slider("Hold Min (ms)", &p.hold_ms_min, 5.0, 50.0, "%.1f");
    row_slider("Hold Max (ms)", &p.hold_ms_max, 20.0, 150.0, "%.1f");
    row_slider("Heavy Hold Chance", &p.hold_ms_heavy_chance, 0.0, 0.1, "%.3f");
    if (p.hold_ms_heavy_chance > 0) { row_slider("Heavy Hold Min", &p.hold_ms_heavy_min, 5.0, 50.0, "%.1f"); row_slider("Heavy Hold Max", &p.hold_ms_heavy_max, 10.0, 80.0, "%.1f"); }
    end_panel();

    begin_panel("Double click");
    row_slider("Chance", &p.double_click_chance, 0.0, 0.3, "%.3f");
    if (p.double_click_chance > 0) {
        row_slider("Delay Min (ms)", &p.double_click_delay_min, 5.0, 80.0, "%.0f");
        row_slider("Delay Max (ms)", &p.double_click_delay_max, 10.0, 120.0, "%.0f");
    }
    end_panel();

    begin_panel("Fatigue breaks");
    row_check("Enable Breaks", &p.breaks_enabled);
    if (p.breaks_enabled) {
        row_slider("Interval Min (s)", &p.break_interval_min, 5.0, 120.0, "%.1f");
        row_slider("Interval Max (s)", &p.break_interval_max, 5.0, 120.0, "%.1f");
        row_slider("Duration Min (s)", &p.break_duration_min, 0.1, 5.0, "%.1f");
        row_slider("Duration Max (s)", &p.break_duration_max, 0.1, 5.0, "%.1f");
    }
    end_panel();
}

// ---------------- Macros ----------------

static void draw_macros_tab(GuiState& state) {
    ImGui::Spacing();
    begin_panel("Global");
    oxygen_checkbox("Enable macros", &state.config.macros_enabled, false);
    ImGui::TextDisabled("Master switch for all macro keybinds and playback");
    end_panel();

    begin_panel("Sequence");
    for (int m = 0; m < MAX_MACROS; m++) {
        if (m > 0) ImGui::SameLine();
        char mlabel[16]; snprintf(mlabel, sizeof(mlabel), "%d##mtab", m + 1);
        bool active = (state.selected_macro == m);
        if (oxygen_button(mlabel, ImVec2(40, 0), 0, 0, active))
            state.selected_macro = m;
    }
    auto& mac = state.config.macros[state.selected_macro];
    ImGui::SetNextItemWidth(panel_inner() * 0.5f);
    ImGui::InputText("##macname", mac.name, sizeof(mac.name));
    field_gloss_last_item();  // Add slider-style sheen
    ImGui::SameLine();
    oxygen_checkbox("Loop", &mac.loop, false);
    keybind_row("Macro Key", mac.key_bind);
    end_panel();

    begin_panel("Steps");
    ImGui::TextDisabled("%d / %d steps", mac.step_count, MAX_MACRO_STEPS);
    int remove_idx = -1;
    for (int s = 0; s < mac.step_count; s++) {
        ImGui::PushID(s);
        auto& step = mac.steps[s];
        int action_idx = static_cast<int>(step.action);
        ImGui::SetNextItemWidth(120);
        if (combo_framed("##action", &action_idx, "Left Click\0Right Click\0Delay\0Switch Slot\0"))
            step.action = static_cast<MacroAction>(action_idx);
        ImGui::SameLine();
        switch (step.action) {
            case MacroAction::LEFT_CLICK: case MacroAction::RIGHT_CLICK:
                ImGui::SetNextItemWidth(60); ImGui::InputInt("##cnt", &step.count, 0); if (step.count < 1) step.count = 1;
                ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::InputDouble("ms##intv", &step.delay_ms, 0, 0, "%.0f"); if (step.delay_ms < 1) step.delay_ms = 1;
                break;
            case MacroAction::DELAY:
                ImGui::SetNextItemWidth(100); ImGui::InputDouble("ms##del", &step.delay_ms, 0, 0, "%.0f"); if (step.delay_ms < 1) step.delay_ms = 1;
                break;
            case MacroAction::SWITCH_SLOT:
                ImGui::SetNextItemWidth(60); ImGui::InputInt("Slot##sw", &step.slot, 0); if (step.slot < 1) step.slot = 1; if (step.slot > 9) step.slot = 9;
                break;
        }
        ImGui::SameLine();
        if (oxygen_button("X##del", ImVec2(24, 0))) remove_idx = s;
        ImGui::PopID();
    }
    if (remove_idx >= 0) {
        for (int s = remove_idx; s < mac.step_count - 1; s++) mac.steps[s] = mac.steps[s + 1];
        mac.step_count--;
    }
    if (mac.step_count < MAX_MACRO_STEPS) {
        if (oxygen_button("+ Add Step")) mac.steps[mac.step_count++] = MacroStep{};
    }
    end_panel();

    begin_panel("Playback");
    ImGui::BeginDisabled(!state.config.macros_enabled);
    if (state.macro_runner.is_running()) {
        ImGui::TextColored(ImVec4(0.55f, 0.90f, 0.65f, 1.0f), "Running (step %d/%d, loop %d)",
                           state.macro_runner.current_step() + 1, mac.step_count, state.macro_runner.loop_count());
        if (oxygen_button("Stop", ImVec2(100, 0))) state.macro_runner.stop();
    } else {
        ImGui::TextDisabled("Idle");
        if (mac.step_count > 0) {
            if (oxygen_button("Run", ImVec2(100, 0))) state.macro_runner.start(mac, &state.active_slot);
        }
    }
    ImGui::EndDisabled();
    end_panel();
}

// ---------------- Settings ----------------

static void draw_settings_tab(GuiState& state, std::vector<std::string>& profiles, int& selected_profile,
                              char* profile_name_buf, char* status_msg, float& status_timer) {
    ImGui::Spacing();
    begin_panel("Keybinds");
    ImGui::BeginDisabled(!state.config.inventory_key_enabled);
    keybind_row("Inventory Key", state.config.key_inventory);
    ImGui::EndDisabled();
    ImGui::SameLine();
    oxygen_checkbox("Enabled##inv", &state.config.inventory_key_enabled);
    ImGui::TextDisabled("Pauses clicking while the inventory is open");
    end_panel();

    begin_panel("Hotbar slot filter");
    row_check("Enable Slot Filter", &state.config.slot_filter_enabled);
    ImGui::TextDisabled("Only click while holding one of the selected slots");
    if (state.config.slot_filter_enabled) {
        for (int i = 0; i < 9; i++) {
            if (i > 0) ImGui::SameLine();
            char slabel[4]; snprintf(slabel, sizeof(slabel), "%d", i + 1);
            oxygen_checkbox(slabel, &state.config.allowed_slots[i]);
        }
        ImGui::Text("Active slot: %d", state.active_slot + 1);
    }
    end_panel();

    begin_panel("Audio");
    row_check("Toggle Sound", &state.config.audio_enabled);
    ImGui::TextDisabled("Beep when a clicker toggles on or off");
    end_panel();

    begin_panel("Profiles");
    {
        ImVec2 lsz(panel_inner(), 100);
        ImVec2 lp0 = ImGui::GetCursorScreenPos();
        ImVec2 lp1(lp0.x + lsz.x, lp0.y + lsz.y);
        field_background(ImGui::GetWindowDrawList(), lp0, lp1, 4.0f, true);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        if (ImGui::BeginChild("##profiles", lsz)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 3));
            for (int i = 0; i < (int)profiles.size(); i++) {
                char iid[32]; snprintf(iid, sizeof(iid), "p%d", i);
                if (oxygen_list_item(iid, profiles[i].c_str(), selected_profile == i)) selected_profile = i;
            }
            ImGui::PopStyleVar();
            oxygen_scrollbar();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::TextDisabled("Profile name");
    ImGui::SetNextItemWidth(panel_inner() * 0.6f);
    ImGui::InputTextWithHint("##profile_name", "New profile name...", profile_name_buf, 64);
    field_gloss_last_item();  // Add slider-style sheen
    if (oxygen_button("Save", ImVec2(80, 0))) {
        std::string name = profile_name_buf[0] ? profile_name_buf : "config";
        if (ConfigFile::save(state.config, name)) {
            snprintf(status_msg, 128, "Saved: %s", name.c_str()); status_timer = 3.0f;
            profiles = ConfigFile::list_profiles();
        }
    }
    ImGui::SameLine();
    if (oxygen_button("Load", ImVec2(80, 0)) && selected_profile >= 0 && selected_profile < (int)profiles.size()) {
        if (ConfigFile::load(state.config, profiles[selected_profile])) {
            snprintf(status_msg, 128, "Loaded: %s", profiles[selected_profile].c_str()); status_timer = 3.0f;
        }
    }
    ImGui::SameLine();
    if (oxygen_button("Delete", ImVec2(80, 0)) && selected_profile >= 0 && selected_profile < (int)profiles.size()) {
        ConfigFile::remove_profile(profiles[selected_profile]);
        snprintf(status_msg, 128, "Deleted: %s", profiles[selected_profile].c_str()); status_timer = 3.0f;
        profiles = ConfigFile::list_profiles(); selected_profile = -1;
    }
    ImGui::SameLine();
    if (oxygen_button("Reset", ImVec2(80, 0))) {
        state.config = Config();
        snprintf(status_msg, 128, "Reset to defaults"); status_timer = 3.0f;
    }
    if (status_timer > 0) { ImGui::TextColored(ImVec4(0.55f, 0.90f, 0.65f, 1.0f), "%s", status_msg); status_timer -= ImGui::GetIO().DeltaTime; }
    end_panel();

    begin_panel("Appearance");
    bool theme_changed = false;
    if (row_combo("Accent Preset", &state.config.accent_preset, "Oxygen Blue\0Red\0Green\0Purple\0Custom\0Follow KDE\0")) theme_changed = true;
    if (state.config.accent_preset == 4) {
        if (ImGui::ColorEdit3("Custom Color", &state.config.accent_r)) theme_changed = true;
    }
    if (theme_changed) setup_oxygen_theme(state.config.accent_preset, state.config.accent_r, state.config.accent_g, state.config.accent_b);
    end_panel();

    begin_panel("Debug");
    row_check("Show Debug Info", &state.show_debug);
    if (state.show_debug) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.1f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::Text("Menu paused: %s", state.in_menu ? "Yes" : "No");
        ImGui::Text("Left armed: %s", state.left_armed ? "Yes" : "No");
        ImGui::Text("Right armed: %s", state.right_armed ? "Yes" : "No");
    }
    end_panel();
}

// ---------------- Analytics ----------------

static void draw_analytics_tab(GuiState& state, char* status_msg, float& status_timer) {
    ImGui::Spacing();
    begin_panel("Live");
    ImGui::TextDisabled("Left CPS");
    { const auto& t = state.left.tracker(); char ov[32]; snprintf(ov, sizeof(ov), "%.1f CPS", t.avg_cps());
      ox_plot_lines("##lcps", t.graph_data(), t.graph_size(), t.graph_offset(), ov, 0.0f, 35.0f, ImVec2(panel_inner(), 60)); }
    ImGui::TextDisabled("Right CPS");
    { const auto& t = state.right.tracker(); char ov[32]; snprintf(ov, sizeof(ov), "%.1f CPS", t.avg_cps());
      ox_plot_lines("##rcps", t.graph_data(), t.graph_size(), t.graph_offset(), ov, 0.0f, 35.0f, ImVec2(panel_inner(), 60)); }
    end_panel();

    begin_panel("Session");
    {
        auto elapsed = std::chrono::steady_clock::now() - state.session_start;
        double session_secs = std::chrono::duration<double>(elapsed).count();
        int s_min = (int)session_secs / 60, s_sec = (int)session_secs % 60;
        int a_min = (int)state.active_time_s / 60, a_sec = (int)state.active_time_s % 60;
        double pct = session_secs > 0 ? state.active_time_s / session_secs * 100.0 : 0;
        uint32_t total = state.left.click_count() + state.right.click_count();
        double avg = state.active_time_s > 0 ? total / state.active_time_s : 0;
        ImGui::Text("Duration: %dm %02ds", s_min, s_sec);
        ImGui::Text("Active: %dm %02ds (%.0f%%)", a_min, a_sec, pct);
        ImGui::Text("Total Clicks: %u (L: %u | R: %u)", total, state.left.click_count(), state.right.click_count());
        ImGui::Text("Session Avg CPS: %.1f", avg);
        ImGui::Text("Peak CPS - L: %.1f | R: %.1f", state.peak_cps_left, state.peak_cps_right);
    }
    end_panel();

    begin_panel("Recording & export");
    bool left_rec = state.left.tracker().is_recording();
    bool right_rec = state.right.tracker().is_recording();
    if (oxygen_checkbox("Record Left", &left_rec)) state.left.tracker().set_recording(left_rec);
    ImGui::SameLine();
    if (oxygen_checkbox("Record Right", &right_rec)) state.right.tracker().set_recording(right_rec);
    ImGui::TextDisabled("Left: %d samples  |  Right: %d samples",
                        state.left.tracker().history_size(), state.right.tracker().history_size());
    if (oxygen_button("Export CSV", ImVec2(120, 0))) {
        std::string dir = ConfigFile::path();
        time_t now = time(nullptr); char ts[32]; strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", localtime(&now));
        bool any = false;
        if (state.left.tracker().history_size() > 0) { state.left.tracker().export_csv(dir + "/analytics_left_" + ts + ".csv"); any = true; }
        if (state.right.tracker().history_size() > 0) { state.right.tracker().export_csv(dir + "/analytics_right_" + ts + ".csv"); any = true; }
        if (any) { snprintf(status_msg, 128, "Exported to %s/", dir.c_str()); status_timer = 5.0f; }
        else { snprintf(status_msg, 128, "No data to export. Enable recording first."); status_timer = 3.0f; }
    }
    ImGui::SameLine();
    if (oxygen_button("Clear Data", ImVec2(120, 0))) {
        state.left.tracker().clear_history(); state.right.tracker().clear_history();
        snprintf(status_msg, 128, "Analytics data cleared"); status_timer = 3.0f;
    }
    if (status_timer > 0) ImGui::TextColored(ImVec4(0.55f, 0.90f, 0.65f, 1.0f), "%s", status_msg);
    end_panel();
}

// ---------------- main loop ----------------

void gui_run(GuiState& state) {
    setup_kde_cursor();
    {
        float r, g, b;
        if (read_kde_color("Colors:Window", "BackgroundNormal", r, g, b)) g_body_color = ImVec4(r, g, b, 1.0f);
        if (read_kde_color("Colors:Window", "ForegroundNormal", r, g, b)) g_text_color = ImVec4(r, g, b, 1.0f);
    }
    if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(760, 640, "Ghast Clicker", nullptr, nullptr);
    if (GLFWcursor* cur = glfwCreateStandardCursor(GLFW_ARROW_CURSOR))
        glfwSetCursor(window, cur);
    if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return; }
    glfwSetWindowSizeLimits(window, 560, 420, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    setup_key_callbacks(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    {
        const char* candidates[] = {
            "/usr/share/fonts/inter/Inter-Regular.otf",
            "/usr/share/fonts/truetype/inter/Inter-Regular.otf",
            "/usr/share/fonts/roboto/Roboto-Regular.ttf",
            "/usr/share/fonts/truetype/roboto/Roboto-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/cantarell/Cantarell-VF.otf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        };
        ImFont* font = nullptr;
        for (const char* p : candidates) {
            if (FILE* f = fopen(p, "rb")) { fclose(f); snprintf(s_font_path, sizeof(s_font_path), "%s", p); font = io.Fonts->AddFontFromFileTTF(p, 18.0f); break; }
        }
        if (!font) io.Fonts->AddFontDefault();
        set_big_font(s_font_path[0] ? io.Fonts->AddFontFromFileTTF(s_font_path, 30.0f) : nullptr);
    }
    setup_oxygen_theme(state.config.accent_preset, state.config.accent_r, state.config.accent_g, state.config.accent_b);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    static char profile_name_buf[64] = "";
    int selected_profile = -1;
    auto profiles = ConfigFile::list_profiles();
    char status_msg[128] = "";
    float status_timer = 0;
    int current_tab = 0;   // pin / always_on_top removed entirely

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        // ---- fractional scaling (Wayland wp-fractional-scale):
        int win_w = 1, win_h = 1, fb_w = 1, fb_h = 1;
        glfwGetWindowSize(window, &win_w, &win_h);
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (win_w < 1) win_w = 1;
        if (win_h < 1) win_h = 1;
        if (fb_w < 1) fb_w = win_w;
        if (fb_h < 1) fb_h = win_h;
        const float ui_scale = (float)fb_w / (float)win_w;
        io.DisplaySize = ImVec2((float)win_w, (float)win_h);
        io.DisplayFramebufferScale = ImVec2(ui_scale, ui_scale);

        if (s_font_scale != ui_scale) {
            s_font_scale = ui_scale;
            io.Fonts->Clear();
            ImFont* f = s_font_path[0] ? io.Fonts->AddFontFromFileTTF(s_font_path, 18.0f * ui_scale) : nullptr;
            if (!f) f = io.Fonts->AddFontDefault();
            f->Scale = 1.0f / ui_scale;
            ImFont* big = s_font_path[0] ? io.Fonts->AddFontFromFileTTF(s_font_path, 30.0f * ui_scale) : nullptr;
            if (big) big->Scale = 1.0f / ui_scale;
            set_big_font(big);
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }

        ImGui::NewFrame();

        {
            ImU32 body_col = OxCol(g_body_color);
            ImU32 drop_top = OxCol(mulc(g_body_color, 1.50f));
            const float DROP_H = 120.0f;
            ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
            bgdl->AddRectFilled(ImVec2(0, 0), ImVec2((float)win_w, (float)win_h), body_col);
            bgdl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2((float)win_w, DROP_H), drop_top, drop_top, body_col, body_col);
            bgdl->AddLine(ImVec2(0, 0), ImVec2((float)win_w, 0), OxCol(mulc(g_body_color, 2.20f), 70));
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)win_w, (float)win_h));
        ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("Ghast Clicker", nullptr, wf)) {
            // global session accounting (runs every frame, independent of tab)
            {
                const auto& lt = state.left.tracker();
                const auto& rt = state.right.tracker();
                if (state.left.is_enabled() || state.right.is_enabled()) state.active_time_s += io.DeltaTime;
                if (state.left.is_enabled())  { double c = lt.avg_cps(); if (c > state.peak_cps_left)  state.peak_cps_left = c; }
                if (state.right.is_enabled()) { double c = rt.avg_cps(); if (c > state.peak_cps_right) state.peak_cps_right = c; }
                if (!state.config.macros_enabled && state.macro_runner.is_running())
                    state.macro_runner.stop();
            }

            // top bar: tab row with L/R indicators inline, flush right (pin removed)
            bool l_on = state.left.is_enabled() || state.left_armed;
            bool r_on = state.right.is_enabled() || state.right_armed;
            ImVec2 tab_mn = ImGui::GetCursorScreenPos();
            float right = tab_mn.x + ImGui::GetContentRegionAvail().x;  // shrinks when scrollbar is reserved
            segmented_tabs(TAB_NAMES, TAB_COUNT, current_tab);
            ImVec2 after_tabs = ImGui::GetCursorScreenPos();
            {
                ImVec2 szL = click_indicator_size("L");
                ImVec2 szR = click_indicator_size("R");
                float x = right - szR.x - 6.0f - szL.x;
                float y = tab_mn.y + (ImGui::GetFrameHeight() - szL.y) * 0.5f;
                ImGui::SetCursorScreenPos(ImVec2(x, y));
                click_indicator("L", l_on);
                ImGui::SameLine(0, 6);
                click_indicator("R", r_on);
            }
            ImGui::SetCursorScreenPos(after_tabs);
            ImGui::Spacing();

            switch (current_tab) {
                case 0: draw_home_tab(state); break;
                case 1: draw_side_tab(state.config.left); break;
                case 2: draw_side_tab(state.config.right); break;
                case 3: draw_macros_tab(state); break;
                case 4: draw_settings_tab(state, profiles, selected_profile, profile_name_buf, status_msg, status_timer); break;
                case 5: draw_analytics_tab(state, status_msg, status_timer); break;
            }
            oxygen_scrollbar();
        }
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(g_body_color.x, g_body_color.y, g_body_color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ConfigFile::save(state.config);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
