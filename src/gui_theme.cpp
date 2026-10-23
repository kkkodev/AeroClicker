#include "gui_theme.hpp"
#include "imgui.h"
#include <cmath>
#include <strings.h>

ImVec4 g_accent_color(0.24f, 0.49f, 0.78f, 1.0f);
ImVec4 g_accent_hover(0.33f, 0.58f, 0.86f, 1.0f);
ImVec4 g_accent_active(0.17f, 0.37f, 0.62f, 1.0f);
ImVec4 g_body_color(0.125f, 0.128f, 0.135f, 1.0f);
ImVec4 g_text_color(0.95f, 0.95f, 0.95f, 1.0f);

ImVec4 mulc(ImVec4 c, float k, float a) {
    return ImVec4(c.x * k > 1.0f ? 1.0f : c.x * k,
                  c.y * k > 1.0f ? 1.0f : c.y * k,
                  c.z * k > 1.0f ? 1.0f : c.z * k, a);
}

static ImVec4 mix4(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

bool  g_light_theme = false;
float ox_luma(ImVec4 c){ return 0.2126f*c.x + 0.7152f*c.y + 0.0722f*c.z; }
bool  ox_is_light(){ return g_light_theme; }
ImU32 ox_thumb_rim(){ return OxCol(mulc(g_body_color, g_light_theme ? 0.45f : 3.10f), 220); }

bool read_kde_color(const char* section, const char* key, float &r, float &g, float &b) {
    const char* home = getenv("HOME");
    if (!home) return false;
    std::string path = std::string(home) + "/.config/kdeglobals";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char line[256];
    bool in = false;
    size_t slen = strlen(section);
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            in = end && (size_t)(end - line - 1) == slen && strncmp(line + 1, section, slen) == 0;
            continue;
        }
        if (in) {
            char* eq = strchr(line, '=');
            if (eq && strstr(line, key)) {
                int cr, cg, cb;
                if (sscanf(eq + 1, "%d,%d,%d", &cr, &cg, &cb) == 3) {
                    r = cr / 255.0f; g = cg / 255.0f; b = cb / 255.0f;
                    fclose(f); return true;
                }
            }
        }
    }
    fclose(f);
    return false;
}

static bool read_kde_accent_color(float &r, float &g, float &b) {
    return read_kde_color("Colors:Selection", "BackgroundNormal", r, g, b);
}

void setup_oxygen_theme(int preset, float custom_r, float custom_g, float custom_b) {
    g_light_theme = ox_luma(g_body_color) > 0.45f;
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();   // reset: repeated calls (color picker drag) must not stack ScaleAllSizes
    if (preset == 0) {
        g_accent_color = ImVec4(0.24f, 0.49f, 0.78f, 1.0f);
    } else if (preset == 1) {
        g_accent_color = ImVec4(0.80f, 0.20f, 0.20f, 1.0f);
    } else if (preset == 2) {
        g_accent_color = ImVec4(0.20f, 0.80f, 0.30f, 1.0f);
    } else if (preset == 3) {
        g_accent_color = ImVec4(0.60f, 0.30f, 0.90f, 1.0f);
    } else if (preset == 4) {
        g_accent_color = ImVec4(custom_r, custom_g, custom_b, 1.0f);
    } else if (preset == 5) {
        float kr, kg, kb;
        if (read_kde_accent_color(kr, kg, kb)) {
            g_accent_color = ImVec4(kr, kg, kb, 1.0f);
        } else {
            g_accent_color = ImVec4(0.24f, 0.49f, 0.78f, 1.0f);
        }
    }

    g_accent_hover = ImVec4(
        g_accent_color.x + (1.0f - g_accent_color.x) * 0.15f,
                            g_accent_color.y + (1.0f - g_accent_color.y) * 0.15f,
                            g_accent_color.z + (1.0f - g_accent_color.z) * 0.15f, 1.0f);
    g_accent_active = ImVec4(
        g_accent_color.x * 0.75f, g_accent_color.y * 0.75f, g_accent_color.z * 0.75f, 1.0f);

    const bool L = g_light_theme;
    ImVec4* colors = style.Colors;
    ImVec4 frame_bg   = L ? mulc(g_body_color, 1.18f, 0.95f) : mulc(g_body_color, 0.80f, 0.90f);
    ImVec4 frame_hov  = L ? mulc(g_body_color, 1.06f, 0.98f) : mulc(g_body_color, 1.25f, 0.95f);
    ImVec4 frame_act  = L ? mulc(g_body_color, 1.00f, 1.00f) : mulc(g_body_color, 1.45f, 1.00f);
    ImVec4 win_bg     = g_body_color;
    ImVec4 pop_bg     = L ? mulc(g_body_color, 1.10f, 1.00f) : mulc(g_body_color, 0.85f, 1.00f);
    ImVec4 title_bg   = L ? mulc(g_body_color, 0.95f, 1.00f) : mulc(g_body_color, 0.75f, 1.00f);

    // Uniform neutral rim color matching ox_rim_neutral(160)
    ImVec4 rim        = mix4(g_body_color, g_text_color, L ? 0.45f : 0.40f); rim.w = 160 / 255.0f;
    ImVec4 sep        = mix4(g_body_color, g_text_color, L ? 0.35f : 0.30f); sep.w = 0.60f;
    ImVec4 text_dis   = ImVec4(g_text_color.x * 0.55f + g_body_color.x * 0.45f,
                               g_text_color.y * 0.55f + g_body_color.y * 0.45f,
                               g_text_color.z * 0.55f + g_body_color.z * 0.45f, 1.00f);

    colors[ImGuiCol_Text]                   = g_text_color;
    colors[ImGuiCol_TextDisabled]           = text_dis;
    colors[ImGuiCol_WindowBg]               = win_bg;
    colors[ImGuiCol_ChildBg]                = ImVec4(frame_bg.x, frame_bg.y, frame_bg.z, 0.0f);
    colors[ImGuiCol_PopupBg]                = pop_bg;
    colors[ImGuiCol_Border]                 = rim;
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = frame_bg;
    colors[ImGuiCol_FrameBgHovered]         = frame_hov;
    colors[ImGuiCol_FrameBgActive]          = frame_act;
    colors[ImGuiCol_TitleBg]                = title_bg;
    colors[ImGuiCol_TitleBgActive]          = title_bg;
    colors[ImGuiCol_TitleBgCollapsed]       = L ? mulc(g_body_color, 0.90f, 1.00f) : mulc(g_body_color, 0.60f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = frame_bg;
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_CheckMark]              = L ? mulc(g_body_color, 0.25f, 1.0f) : mulc(g_body_color, 6.50f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Button]                 = g_accent_active;
    colors[ImGuiCol_ButtonHovered]          = g_accent_color;
    colors[ImGuiCol_ButtonActive]           = g_accent_hover;
    colors[ImGuiCol_Header]                 = L ? mulc(g_body_color, 0.90f, 1.00f) : mulc(g_body_color, 1.80f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = L ? mulc(g_body_color, 0.82f, 1.00f) : mulc(g_body_color, 2.20f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = L ? mulc(g_body_color, 0.75f, 1.00f) : mulc(g_body_color, 2.60f, 1.00f);
    colors[ImGuiCol_Separator]              = sep;
    colors[ImGuiCol_SeparatorHovered]       = sep;
    colors[ImGuiCol_SeparatorActive]        = sep;
    colors[ImGuiCol_ResizeGrip]             = ImVec4(sep.x, sep.y, sep.z, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]      = g_accent_color;
    colors[ImGuiCol_ResizeGripActive]       = g_accent_hover;
    colors[ImGuiCol_Tab]                    = L ? mulc(g_body_color, 0.95f, 0.90f) : mulc(g_body_color, 1.10f, 0.90f);
    colors[ImGuiCol_TabHovered]             = g_accent_color;
    colors[ImGuiCol_TabActive]              = g_accent_active;
    colors[ImGuiCol_TabUnfocused]           = L ? mulc(g_body_color, 0.90f, 0.90f) : mulc(g_body_color, 0.95f, 0.90f);
    colors[ImGuiCol_TabUnfocusedActive]     = g_accent_active;
    colors[ImGuiCol_PlotLines]              = g_accent_color;
    colors[ImGuiCol_PlotLinesHovered]       = g_accent_hover;
    colors[ImGuiCol_PlotHistogram]          = g_accent_color;
    colors[ImGuiCol_PlotHistogramHovered]   = g_accent_hover;
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(g_accent_color.x, g_accent_color.y, g_accent_color.z, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = g_accent_hover;
    colors[ImGuiCol_NavHighlight]           = g_accent_color;
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);

    style.WindowPadding         = ImVec2(12, 10);
    style.FramePadding          = ImVec2(8, 5);
    style.ItemSpacing           = ImVec2(8, 8);
    style.ItemInnerSpacing      = ImVec2(6, 4);
    style.TouchExtraPadding     = ImVec2(0, 0);
    style.IndentSpacing         = 20.0f;
    style.ScrollbarSize         = 13.0f;
    style.GrabMinSize           = 12.0f;
    style.WindowBorderSize      = 0.0f;
    style.ChildBorderSize       = 0.0f;
    style.PopupBorderSize       = 1.0f;
    style.FrameBorderSize       = 1.0f;
    style.TabBorderSize         = 0.0f;
    style.WindowRounding        = 4.0f;
    style.ChildRounding         = 4.0f;
    style.FrameRounding         = 4.0f;
    style.PopupRounding         = 4.0f;
    style.ScrollbarRounding     = 4.0f;
    style.GrabRounding          = 4.0f;
    style.TabRounding           = 4.0f;
    style.AntiAliasedLines      = true;
    style.AntiAliasedFill       = true;
    style.WindowTitleAlign      = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition   = ImGuiDir_Right;
    style.ScaleAllSizes(1.12f);
}

ImU32 OxCol(ImVec4 c, int a) {
    return IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), a);
}
ImVec4 OxLight(ImVec4 c) {
    return ImVec4(c.x*1.3f > 1.0f ? 1.0f : c.x*1.3f,
                  c.y*1.3f > 1.0f ? 1.0f : c.y*1.3f,
                  c.z*1.3f > 1.0f ? 1.0f : c.z*1.3f, 1.0f);
}
ImVec4 OxDark(ImVec4 c, float k) {
    return ImVec4(c.x*k, c.y*k, c.z*k, 1.0f);
}

static bool parse_kde_mouse_section(const char* path, char* theme, size_t tn, char* size, size_t sn) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    bool in_mouse = false, found = false;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '[') { in_mouse = (strncmp(line, "[Mouse]", 7) == 0); continue; }
        if (!in_mouse) continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = line; char* val = eq + 1;
        while (*key == ' ' || *key == '\t') key++;
        char* e = key + strlen(key); while (e > key && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e = '\0';
        while (*val == ' ' || *val == '\t') val++;
        e = val + strlen(val); while (e > val && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e = '\0';
        if (!strcasecmp(key, "cursorTheme") && !theme[0]) { strncpy(theme, val, tn - 1); theme[tn - 1] = 0; found = true; }
        else if (!strcasecmp(key, "cursorSize") && !size[0]) { strncpy(size, val, sn - 1); size[sn - 1] = 0; found = true; }
    }
    fclose(f);
    return found;
}

void setup_kde_cursor() {
    char theme[128] = "";
    char size[32]   = "";
    const char* home = getenv("HOME");
    if (home) {
        std::string p = std::string(home) + "/.config/kcminputrc";
        parse_kde_mouse_section(p.c_str(), theme, sizeof(theme), size, sizeof(size));
        if (!theme[0]) {   // some Plasma versions keep it in kdeglobals instead
            std::string g = std::string(home) + "/.config/kdeglobals";
            parse_kde_mouse_section(g.c_str(), theme, sizeof(theme), size, sizeof(size));
        }
    }
    if (!theme[0]) { const char* t = getenv("XCURSOR_THEME"); if (t && t[0]) { strncpy(theme, t, sizeof(theme) - 1); theme[sizeof(theme) - 1] = 0; } }
    if (!theme[0]) strncpy(theme, "breeze_cursors", sizeof(theme) - 1);   // KDE stock theme
    if (!size[0])  { const char* s = getenv("XCURSOR_SIZE");  if (s && s[0]) { strncpy(size, s, sizeof(size) - 1); size[sizeof(size) - 1] = 0; } }
    if (!size[0])  strncpy(size, "24", sizeof(size) - 1);
    setenv("XCURSOR_THEME", theme, 1);
    setenv("XCURSOR_SIZE",  size,  1);
    std::string xpath = "/usr/share/icons:/usr/local/share/icons";
    if (home) xpath += std::string(":") + home + "/.icons:" + home + "/.local/share/icons";
    setenv("XCURSOR_PATH", xpath.c_str(), 1);   // wayland-cursor must be able to *find* the theme dir
}
