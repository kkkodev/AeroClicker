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

// True when a KDE colour scheme was found (the Oxygen decoration is in use).
extern ImVec4 g_tb_edge_focused;
extern ImVec4 g_tb_edge_unfocused;
extern bool   g_tb_edge_valid;
void load_kde_titlebar_edges();

// Oxygen's window background gradient, ported from KDE's liboxygen/kstyle.
//
// Oxygen paints the *whole* window frame (titlebar included) with
// renderWindowBackground(): a vertical top->base->bottom gradient, a flat fill below it and
// a wide radial highlight at the top centre. Its widget style continues that same gradient
// inside the client area with yShift = -23, which is what makes a KDE app's menu bar blend
// into the titlebar. Reproducing the style call (not the decoration call) means the client
// always joins the titlebar no matter how tall the decoration's titlebar happens to be.
struct OxygenBackground {
    ImVec4 top, base, bottom, radial;
    float  split_y;   // height of the gradient (client height based, max 300)
    float  y_shift;   // -23, the Oxygen style default
};

OxygenBackground oxygen_background(float client_h);
// Fill the client area with the continued gradient. Uses the current KDE contrast setting.
void draw_oxygen_background(ImDrawList* dl, float w, float h);

void setup_kde_cursor();
