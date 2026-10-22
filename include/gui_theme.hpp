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

void setup_oxygen_theme(int preset, float custom_r, float custom_g, float custom_b);
ImVec4 mulc(ImVec4 c, float k, float a = 1.0f);
ImU32 OxCol(ImVec4 c, int a = 255);
ImVec4 OxLight(ImVec4 c);
ImVec4 OxDark(ImVec4 c, float k = 0.4f);
bool read_kde_color(const char* section, const char* key, float &r, float &g, float &b);
void setup_kde_cursor();
