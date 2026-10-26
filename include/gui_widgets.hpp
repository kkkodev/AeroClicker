#pragma once
#include "imgui.h"
#include "clicker_config.hpp"
#include <GLFW/glfw3.h>

// --- setup / input ---
void setup_key_callbacks(GLFWwindow* window);
bool ui_pointer_over_window();   // cursor is over AeroClicker's own content area
const char* key_name(int code);
void set_big_font(ImFont* font);   // crisp large readout font (loaded at ~30px)

bool ox_input_text(const char* id, char* buf, int buf_n, const char* hint, float w);
bool ox_input_double(const char* id, double* v, double step, double step_fast, const char* fmt, float w);
bool ox_input_int(const char* id, int* v, int step, int step_fast, float w);
void ox_plot_lines(const char* id, const float* values, int count, int offset,
                   const char* overlay, float y0, float y1, ImVec2 size);

// --- core widgets ---
void oxygen_scrollbar();
bool oxygen_button(const char* label, ImVec2 size_arg = ImVec2(0, 0), ImU32 c1_override = 0, ImU32 c2_override = 0, bool force_colored = false);
bool oxygen_checkbox(const char* label, bool* v, bool label_clickable = true);
bool oxygen_list_item(const char* id, const char* label, bool selected, bool separator = false);
bool combo_framed(const char* id, int* idx, const char* items);
void field_background(ImDrawList* dl, ImVec2 r0, ImVec2 r1, float rounding, bool border = true, int fill_alpha = 230);
void field_gloss_last_item();
bool oxygen_field_button(const char* label, ImVec2 size_arg = ImVec2(0, 0), bool armed = false);
ImVec2 click_indicator_size(const char* label);

// table-style rows (label left, control right) — for wide panels
bool row_slider(const char* label, double* v, double lo, double hi, const char* fmt);
bool row_combo(const char* label, int* idx, const char* items);
bool row_check(const char* label, bool* v);
bool row_color3(const char* label, float* rgb);   // themed swatch + R/G/B fields
void keybind_row(const char* label, int& key_code);

// stacked rows (label above control) — for narrow cards
bool stacked_slider(const char* label, double* v, double lo, double hi, const char* fmt, float w);
bool stacked_combo(const char* label, int* idx, const char* items, float w);
void stacked_keybind(const char* label, int& key_code, float w);

// --- flat panels (cards): begin_panel ... content ... end_panel ---
void begin_panel(const char* label, float width = -1.0f, int status = -1); // status: -1 none, 0 off, 1 on
void end_panel();
float panel_inner();   // usable content width inside current panel

// --- status ---
bool click_indicator(const char* label, bool on);   // returns true when clicked
// note: when set while `on`, the readout says "Paused" and shows this line instead of a
// meaningless "0.0 CPS" (e.g. "cursor over app", "hold the mouse button").
void stat_readout(bool on, float cps, uint32_t clicks, float inner_w, const char* note = nullptr);
void segmented_tabs(const char* const* names, int count, int& current_tab);
