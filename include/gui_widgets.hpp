#pragma once
#include "imgui.h"
#include "clicker_config.hpp"
#include "gui_icons.hpp"
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
// Custom scrollbar for the current window's vertical scroll. track_top optionally starts the
// track below a fixed header instead of at the window top.
void oxygen_scrollbar(float track_top = -1.0f);
bool oxygen_button(const char* label, ImVec2 size_arg = ImVec2(0, 0), ImU32 c1_override = 0, ImU32 c2_override = 0, bool force_colored = false,
                   OxIcon icon = OxIcon::None);
// Two-line choice tile (name over range) used for the preset grids: equal-width cells that
// fill the row exactly, so a wrapped grid never looks ragged or off-centre.
bool ox_preset_tile(const char* id, const char* title, const char* sub, ImVec2 size, bool selected);
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
void begin_panel(const char* label, float width = -1.0f, int status = -1,
                 OxIcon icon = OxIcon::None); // status: -1 none, 0 off, 1 on, 2 paused
void end_panel();
float panel_inner();   // usable content width inside current panel

// --- status ---
// Read-only Oxygen surface for the Home page's hotbar chips: neutral face when the slot is
// not mapped, the same accent face as a selected preset when it is, and a 2px accent ring on
// the slot the game currently has selected. `sides` is 1 (left clicker), 2 (right), 3 (both):
// the accent is strongest on the owning side, so the fill shows who maps the slot.
void oxygen_slot_chip(ImVec2 mn, ImVec2 mx, const char* label, int sides, bool current);
bool click_indicator(const char* label, bool on, bool paused = false);   // returns true when clicked
// note: when set while `on`, the readout says "Paused" and shows this line instead of a
// meaningless "0.0 CPS" (e.g. "cursor over app", "hold the mouse button").
void stat_readout(bool on, float cps, uint32_t clicks, float inner_w, const char* note = nullptr);
// icons may be null, or an array of `count` entries; OxIcon::None leaves a tab text-only.
// max_width is the space the bar may occupy (it shares its row with the L/R pills); the bar
// tightens its padding and then drops the icons to fit, and clips as a last resort.
void segmented_tabs(const char* const* names, int count, int& current_tab,
                    const OxIcon* icons = nullptr, float max_width = -1.0f);
