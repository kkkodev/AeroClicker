#pragma once
#include "imgui.h"
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>

extern ImVec4 g_accent_color;
extern ImVec4 g_accent_hover;
extern ImVec4 g_accent_active;
extern ImVec4 g_body_color;
extern ImVec4 g_text_color;

extern bool g_light_theme;
float ox_luma(ImVec4 c);
bool  ox_is_light();
ImU32 ox_thumb_rim();

void setup_oxygen_theme(int preset, float custom_r, float custom_g, float custom_b);
ImVec4 mulc(ImVec4 c, float k, float a = 1.0f);
// Theme-safe shade: t > 0 blends toward white, t < 0 scales toward black.
// mulc() is only usable on a dark body colour; on a light theme every factor > 1 clamps to
// pure white, which is what made light-theme fields flat white with invisible borders.
ImVec4 ox_tint(ImVec4 c, float t);
ImU32 OxCol(ImVec4 c, int a = 255);
ImVec4 OxLight(ImVec4 c);
ImVec4 OxDark(ImVec4 c, float k = 0.4f);
bool read_kde_color(const char* section, const char* key, float &r, float &g, float &b);

// Colour the window decoration's titlebar has at its lower edge (where it meets the client
// area), for the focused and unfocused window states. The app fades its body from that
// colour so the surface is continuous; the titlebar's own gradient is never re-created.
extern ImVec4 g_tb_edge_focused;
extern ImVec4 g_tb_edge_unfocused;
extern bool   g_tb_edge_valid;
void load_kde_titlebar_edges();

void setup_kde_cursor();
