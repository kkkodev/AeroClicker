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
void setup_kde_cursor();
