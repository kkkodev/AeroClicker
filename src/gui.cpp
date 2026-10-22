#include "gui.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <cstring>
#include <linux/input-event-codes.h>
#include <cfloat>
#include <cstdlib>
#include <cmath>

// Global accent color state
static ImVec4 g_accent_color(0.24f, 0.49f, 0.78f, 1.0f);  // Default Oxygen blue
static ImVec4 g_accent_hover(0.33f, 0.58f, 0.86f, 1.0f);
static ImVec4 g_accent_active(0.17f, 0.37f, 0.62f, 1.0f);

// KDE scheme colors: window background + foreground (all neutral grays derive from these)
static ImVec4 g_body_color(0.125f, 0.128f, 0.135f, 1.0f);
static ImVec4 g_text_color(0.95f, 0.95f, 0.95f, 1.0f);

// Generic kdeglobals reader with exact [Section] matching
static bool read_kde_color(const char* section, const char* key, float &r, float &g, float &b) {
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
                    fclose(f);
                    return true;
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

static ImVec4 mulc(ImVec4 c, float k, float a = 1.0f) {
    return ImVec4(c.x * k > 1.0f ? 1.0f : c.x * k,
                  c.y * k > 1.0f ? 1.0f : c.y * k,
                  c.z * k > 1.0f ? 1.0f : c.z * k, a);
}

// Apply complete Oxygen-style theme with custom accent
static void setup_oxygen_theme(int preset, float custom_r, float custom_g, float custom_b) {
    ImGuiStyle& style = ImGui::GetStyle();

    if (preset == 0) {
        g_accent_color = ImVec4(0.24f, 0.49f, 0.78f, 1.0f); // Oxygen Blue
    } else if (preset == 1) {
        g_accent_color = ImVec4(0.80f, 0.20f, 0.20f, 1.0f); // Red
    } else if (preset == 2) {
        g_accent_color = ImVec4(0.20f, 0.80f, 0.30f, 1.0f); // Green
    } else if (preset == 3) {
        g_accent_color = ImVec4(0.60f, 0.30f, 0.90f, 1.0f); // Purple
    } else if (preset == 4) {
        g_accent_color = ImVec4(custom_r, custom_g, custom_b, 1.0f); // Custom
    } else if (preset == 5) {
        float kr, kg, kb;
        if (read_kde_accent_color(kr, kg, kb)) {
            g_accent_color = ImVec4(kr, kg, kb, 1.0f); // Follow KDE
        } else {
            g_accent_color = ImVec4(0.24f, 0.49f, 0.78f, 1.0f); // Fallback
        }
    }

    g_accent_hover = ImVec4(
        g_accent_color.x + (1.0f - g_accent_color.x) * 0.15f,
        g_accent_color.y + (1.0f - g_accent_color.y) * 0.15f,
        g_accent_color.z + (1.0f - g_accent_color.z) * 0.15f,
        1.0f
    );
    g_accent_active = ImVec4(
        g_accent_color.x * 0.75f,
        g_accent_color.y * 0.75f,
        g_accent_color.z * 0.75f,
        1.0f
    );

    ImVec4* colors = style.Colors;

    // All neutral grays derived from the KDE window background so the UI matches the scheme
    ImVec4 frame_bg   = mulc(g_body_color, 0.80f, 0.90f);
    ImVec4 frame_hov  = mulc(g_body_color, 1.25f, 0.95f);
    ImVec4 frame_act  = mulc(g_body_color, 1.45f, 1.00f);
    ImVec4 win_bg     = g_body_color;
    ImVec4 pop_bg     = mulc(g_body_color, 0.85f, 1.00f);
    ImVec4 title_bg   = mulc(g_body_color, 0.75f, 1.00f);
    ImVec4 text_dis   = ImVec4(g_text_color.x * 0.55f + g_body_color.x * 0.45f,
                               g_text_color.y * 0.55f + g_body_color.y * 0.45f,
                               g_text_color.z * 0.55f + g_body_color.z * 0.45f, 1.00f);

    colors[ImGuiCol_Text]                   = g_text_color;
    colors[ImGuiCol_TextDisabled]           = text_dis;
    colors[ImGuiCol_WindowBg]               = win_bg;
    colors[ImGuiCol_ChildBg]                = ImVec4(frame_bg.x, frame_bg.y, frame_bg.z, 0.0f);
    colors[ImGuiCol_PopupBg]                = pop_bg;
    colors[ImGuiCol_Border]                 = mulc(g_body_color, 3.60f, 0.55f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = frame_bg;
    colors[ImGuiCol_FrameBgHovered]         = frame_hov;
    colors[ImGuiCol_FrameBgActive]          = frame_act;
    colors[ImGuiCol_TitleBg]                = title_bg;
    colors[ImGuiCol_TitleBgActive]          = title_bg;
    colors[ImGuiCol_TitleBgCollapsed]       = mulc(g_body_color, 0.60f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = mulc(g_body_color, 0.80f, 0.90f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_CheckMark]              = mulc(g_body_color, 6.50f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0,0,0,0);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0,0,0,0);
    colors[ImGuiCol_Button]                 = g_accent_active;
    colors[ImGuiCol_ButtonHovered]          = g_accent_color;
    colors[ImGuiCol_ButtonActive]           = g_accent_hover;
    colors[ImGuiCol_Header]                 = mulc(g_body_color, 1.80f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = mulc(g_body_color, 2.20f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = mulc(g_body_color, 2.60f, 1.00f);
    colors[ImGuiCol_Separator]              = mulc(g_body_color, 2.80f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]       = mulc(g_body_color, 2.80f, 0.60f);
    colors[ImGuiCol_SeparatorActive]        = mulc(g_body_color, 2.80f, 0.60f);
    colors[ImGuiCol_ResizeGrip]             = mulc(g_body_color, 3.60f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]      = g_accent_color;
    colors[ImGuiCol_ResizeGripActive]       = g_accent_hover;
    colors[ImGuiCol_Tab]                    = mulc(g_body_color, 1.10f, 0.90f);
    colors[ImGuiCol_TabHovered]             = g_accent_color;
    colors[ImGuiCol_TabActive]              = g_accent_active;
    colors[ImGuiCol_TabUnfocused]           = mulc(g_body_color, 0.95f, 0.90f);
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

    // Oxygen-style rounding and sizing — uniform vertical rhythm
    style.WindowPadding         = ImVec2(12, 10);
    style.FramePadding          = ImVec2(8, 5);
    style.ItemSpacing           = ImVec2(8, 8);
    style.ItemInnerSpacing      = ImVec2(6, 4);
    style.TouchExtraPadding     = ImVec2(0, 0);
    style.IndentSpacing         = 20.0f;
    style.ScrollbarSize         = 14.0f;
    style.GrabMinSize           = 12.0f;

    style.WindowBorderSize      = 0.0f;
    style.ChildBorderSize       = 0.0f;
    style.PopupBorderSize       = 1.0f;
    style.FrameBorderSize       = 1.0f;
    style.TabBorderSize         = 0.0f;

    style.WindowRounding   = 4.0f;
    style.ChildRounding    = 4.0f;
    style.FrameRounding    = 4.0f;
    style.PopupRounding    = 4.0f;
    style.ScrollbarRounding= 4.0f;
    style.GrabRounding     = 4.0f;
    style.TabRounding      = 4.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;

    style.WindowTitleAlign      = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition   = ImGuiDir_Right;

    // Global scale bump (safe on re-call: everything above is assigned absolutely)
    style.ScaleAllSizes(1.12f);
}

static const char* key_name(int code) {
    static char buf[32];
    switch (code) {
        case KEY_ESC: return "Escape";
        case KEY_1: return "1"; case KEY_2: return "2"; case KEY_3: return "3";
        case KEY_4: return "4"; case KEY_5: return "5"; case KEY_6: return "6";
        case KEY_7: return "7"; case KEY_8: return "8"; case KEY_9: return "9";
        case KEY_0: return "0";
        case KEY_Q: return "Q"; case KEY_W: return "W"; case KEY_E: return "E";
        case KEY_R: return "R"; case KEY_T: return "T"; case KEY_Y: return "Y";
        case KEY_U: return "U"; case KEY_I: return "I"; case KEY_O: return "O";
        case KEY_P: return "P"; case KEY_A: return "A"; case KEY_S: return "S";
        case KEY_D: return "D"; case KEY_F: return "F"; case KEY_G: return "G";
        case KEY_H: return "H"; case KEY_J: return "J"; case KEY_K: return "K";
        case KEY_L: return "L"; case KEY_Z: return "Z"; case KEY_X: return "X";
        case KEY_C: return "C"; case KEY_V: return "V"; case KEY_B: return "B";
        case KEY_N: return "N"; case KEY_M: return "M";
        case KEY_F1: return "F1"; case KEY_F2: return "F2"; case KEY_F3: return "F3";
        case KEY_F4: return "F4"; case KEY_F5: return "F5"; case KEY_F6: return "F6";
        case KEY_F7: return "F7"; case KEY_F8: return "F8"; case KEY_F9: return "F9";
        case KEY_F10: return "F10"; case KEY_F11: return "F11"; case KEY_F12: return "F12";
        case KEY_SPACE: return "Space";
        case KEY_LEFTSHIFT: return "LShift"; case KEY_RIGHTSHIFT: return "RShift";
        case KEY_LEFTCTRL: return "LCtrl"; case KEY_RIGHTCTRL: return "RCtrl";
        case KEY_LEFTALT: return "LAlt"; case KEY_RIGHTALT: return "RAlt";
        case KEY_TAB: return "Tab"; case KEY_CAPSLOCK: return "CapsLock";
        case KEY_ENTER: return "Enter"; case KEY_BACKSPACE: return "Backspace";
        case KEY_DELETE: return "Delete"; case KEY_INSERT: return "Insert";
        case KEY_HOME: return "Home"; case KEY_END: return "End";
        case KEY_PAGEUP: return "PageUp"; case KEY_PAGEDOWN: return "PageDown";
        case BTN_LEFT: return "Mouse L"; case BTN_RIGHT: return "Mouse R";
        case BTN_MIDDLE: return "Mouse M"; case BTN_SIDE: return "Mouse Side";
        case BTN_EXTRA: return "Mouse Extra";
        case KEY_KP0: return "KP 0"; case KEY_KP1: return "KP 1"; case KEY_KP2: return "KP 2";
        case KEY_KP3: return "KP 3"; case KEY_KP4: return "KP 4"; case KEY_KP5: return "KP 5";
        case KEY_KP6: return "KP 6"; case KEY_KP7: return "KP 7"; case KEY_KP8: return "KP 8";
        case KEY_KP9: return "KP 9"; case KEY_KPDOT: return "KP .";
        case KEY_KPENTER: return "KP Enter"; case KEY_KPPLUS: return "KP +";
        case KEY_KPMINUS: return "KP -"; case KEY_KPASTERISK: return "KP *";
        case KEY_KPSLASH: return "KP /";
        default:
            snprintf(buf, sizeof(buf), "Key %d", code);
            return buf;
    }
}

// Map GLFW key codes to Linux evdev codes
static int glfw_to_evdev(int glfw_key) {
    if (glfw_key >= GLFW_KEY_A && glfw_key <= GLFW_KEY_Z)
        return KEY_A + (glfw_key - GLFW_KEY_A);
    if (glfw_key >= GLFW_KEY_0 && glfw_key <= GLFW_KEY_9)
        return (glfw_key == GLFW_KEY_0) ? KEY_0 : KEY_1 + (glfw_key - GLFW_KEY_1);
    if (glfw_key >= GLFW_KEY_F1 && glfw_key <= GLFW_KEY_F12)
        return KEY_F1 + (glfw_key - GLFW_KEY_F1);

    switch (glfw_key) {
        case GLFW_KEY_ESCAPE: return KEY_ESC;
        case GLFW_KEY_TAB: return KEY_TAB;
        case GLFW_KEY_CAPS_LOCK: return KEY_CAPSLOCK;
        case GLFW_KEY_LEFT_SHIFT: return KEY_LEFTSHIFT;
        case GLFW_KEY_RIGHT_SHIFT: return KEY_RIGHTSHIFT;
        case GLFW_KEY_LEFT_CONTROL: return KEY_LEFTCTRL;
        case GLFW_KEY_RIGHT_CONTROL: return KEY_RIGHTCTRL;
        case GLFW_KEY_LEFT_ALT: return KEY_LEFTALT;
        case GLFW_KEY_RIGHT_ALT: return KEY_RIGHTALT;
        case GLFW_KEY_SPACE: return KEY_SPACE;
        case GLFW_KEY_ENTER: return KEY_ENTER;
        case GLFW_KEY_BACKSPACE: return KEY_BACKSPACE;
        case GLFW_KEY_DELETE: return KEY_DELETE;
        case GLFW_KEY_INSERT: return KEY_INSERT;
        case GLFW_KEY_HOME: return KEY_HOME;
        case GLFW_KEY_END: return KEY_END;
        case GLFW_KEY_PAGE_UP: return KEY_PAGEUP;
        case GLFW_KEY_PAGE_DOWN: return KEY_PAGEDOWN;
        case GLFW_KEY_KP_ENTER: return KEY_KPENTER;
        case GLFW_KEY_KP_DECIMAL: return KEY_KPDOT;
        case GLFW_KEY_KP_DIVIDE: return KEY_KPSLASH;
        case GLFW_KEY_KP_MULTIPLY: return KEY_KPASTERISK;
        case GLFW_KEY_KP_SUBTRACT: return KEY_KPMINUS;
        case GLFW_KEY_KP_ADD: return KEY_KPPLUS;
        case GLFW_KEY_KP_EQUAL: return KEY_KPEQUAL;
        case GLFW_KEY_KP_0: return KEY_KP0;
        case GLFW_KEY_KP_1: return KEY_KP1;
        case GLFW_KEY_KP_2: return KEY_KP2;
        case GLFW_KEY_KP_3: return KEY_KP3;
        case GLFW_KEY_KP_4: return KEY_KP4;
        case GLFW_KEY_KP_5: return KEY_KP5;
        case GLFW_KEY_KP_6: return KEY_KP6;
        case GLFW_KEY_KP_7: return KEY_KP7;
        case GLFW_KEY_KP_8: return KEY_KP8;
        case GLFW_KEY_KP_9: return KEY_KP9;
        default: return -1;
    }
}

static int* g_binding_target = nullptr;
static GLFWwindow* g_window = nullptr;

static void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (!g_binding_target || action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) {
        *g_binding_target = 0;
    } else {
        int evdev = glfw_to_evdev(key);
        if (evdev >= 0)
            *g_binding_target = evdev;
    }
    g_binding_target = nullptr;
}

static void mouse_button_callback(GLFWwindow*, int button, int action, int) {
    if (!g_binding_target || action != GLFW_PRESS) return;
    int evdev = -1;
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT: evdev = BTN_LEFT; break;
        case GLFW_MOUSE_BUTTON_RIGHT: evdev = BTN_RIGHT; break;
        case GLFW_MOUSE_BUTTON_MIDDLE: evdev = BTN_MIDDLE; break;
        case GLFW_MOUSE_BUTTON_4: evdev = BTN_SIDE; break;
        case GLFW_MOUSE_BUTTON_5: evdev = BTN_EXTRA; break;
        default:
            if (button >= 6) evdev = BTN_FORWARD + (button - 6);
            break;
    }
    if (evdev >= 0) {
        *g_binding_target = evdev;
    }
    g_binding_target = nullptr;
}

static const float LABEL_W = 175.0f;
static const float OXY_R = 4.0f;

// ---------- color + animation helpers ----------

static ImU32 lerp_u32(ImU32 a, ImU32 b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int ar = (a >> IM_COL32_R_SHIFT) & 0xFF, ag = (a >> IM_COL32_G_SHIFT) & 0xFF,
        ab = (a >> IM_COL32_B_SHIFT) & 0xFF, aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    int br = (b >> IM_COL32_R_SHIFT) & 0xFF, bg = (b >> IM_COL32_G_SHIFT) & 0xFF,
        bb = (b >> IM_COL32_B_SHIFT) & 0xFF, ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t),
                    ab + (int)((bb - ab) * t), aa + (int)((ba - aa) * t));
}

// Eased (ease-in-out) transition progress stored per widget id. ~200ms feel.
static float widget_anim(ImGuiID id, bool target) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float t = st->GetFloat(id, 0.0f);
    float goal = target ? 1.0f : 0.0f;
    float k = 1.0f - expf(-12.0f * ImGui::GetIO().DeltaTime);
    t += (goal - t) * k;
    if (t < 0.001f) t = 0.0f; else if (t > 0.999f) t = 1.0f;
    st->SetFloat(id, t);
    return t * t * (3.0f - 2.0f * t);   // smoothstep
}

// Rounded rect + true vertical gradient (alpha included), full bleed.
static void AddRoundedRectFilledGradV(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col_upr, ImU32 col_bot, float rounding) {
    if (b.x <= a.x || b.y <= a.y) return;
    if (rounding <= 0.0f) {
        dl->AddRectFilledMultiColor(a, b, col_upr, col_upr, col_bot, col_bot);
        return;
    }
    int start_vtx = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding);
    int end_vtx = dl->VtxBuffer.Size;
    ImDrawVert* vtx = dl->VtxBuffer.Data + start_vtx;
    for (int i = 0; i < end_vtx - start_vtx; i++) {
        float t = (vtx[i].pos.y - a.y) / (b.y - a.y);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        int r0 = (col_upr >> IM_COL32_R_SHIFT) & 0xFF;
        int g0 = (col_upr >> IM_COL32_G_SHIFT) & 0xFF;
        int b0 = (col_upr >> IM_COL32_B_SHIFT) & 0xFF;
        int a0 = (col_upr >> IM_COL32_A_SHIFT) & 0xFF;
        int r1 = (col_bot >> IM_COL32_R_SHIFT) & 0xFF;
        int g1 = (col_bot >> IM_COL32_G_SHIFT) & 0xFF;
        int b1 = (col_bot >> IM_COL32_B_SHIFT) & 0xFF;
        int a1 = (col_bot >> IM_COL32_A_SHIFT) & 0xFF;
        int va = (vtx[i].col >> IM_COL32_A_SHIFT) & 0xFF;
        int al = (a0 + (int)((a1 - a0) * t)) * va / 255;
        vtx[i].col = IM_COL32(r0 + (int)((r1-r0)*t), g0 + (int)((g1-g0)*t),
                              b0 + (int)((b1-b0)*t), al);
    }
}

// Concentric gradient: light center, darker toward all sides.
// edge_bias < 1 pushes the dark tone out to just the rim.
static void AddRoundedRectGradCenter(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 center_col, ImU32 edge_col, float rounding, int bands = 10, float edge_bias = 0.45f) {
    if (b.x <= a.x || b.y <= a.y) return;
    float w = b.x - a.x, h = b.y - a.y;
    float max_inset = (w < h ? w : h) * 0.5f;
    int cr = (center_col >> IM_COL32_R_SHIFT) & 0xFF, cg = (center_col >> IM_COL32_G_SHIFT) & 0xFF,
        cb = (center_col >> IM_COL32_B_SHIFT) & 0xFF, ca = (center_col >> IM_COL32_A_SHIFT) & 0xFF;
    int er = (edge_col >> IM_COL32_R_SHIFT) & 0xFF, eg = (edge_col >> IM_COL32_G_SHIFT) & 0xFF,
        eb = (edge_col >> IM_COL32_B_SHIFT) & 0xFF, ea = (edge_col >> IM_COL32_A_SHIFT) & 0xFF;
    for (int s = 0; s < bands; s++) {
        float f = (bands > 1) ? (float)s / (float)(bands - 1) : 1.0f;   // 0 = rim .. 1 = center
        float inset = max_inset * f;
        float t = powf(f, edge_bias);                                    // dark hugs the rim
        ImU32 col = IM_COL32(er + (int)((cr - er) * t), eg + (int)((cg - eg) * t),
                             eb + (int)((cb - eb) * t), ea + (int)((ca - ea) * t));
        float r = rounding - inset; if (r < 1.0f) r = 1.0f;
        dl->AddRectFilled(ImVec2(a.x + inset, a.y + inset), ImVec2(b.x - inset, b.y - inset), col, r);
    }
}

static void OxFrame(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float R,
                    ImU32 c_top, ImU32 c_bot, ImU32 border_col,
                    int gloss = 100, int shadow = 60) {
    ImVec2 b0(floorf(mn.x), floorf(mn.y));
    ImVec2 b1(floorf(mx.x), floorf(mx.y));
    if (b1.x - b0.x < 2.0f) b1.x = b0.x + 2.0f;
    if (b1.y - b0.y < 2.0f) b1.y = b0.y + 2.0f;

    AddRoundedRectFilledGradV(dl, b0, b1, c_top, c_bot, R);

    ImVec2 f0(b0.x + 1.0f, b0.y + 1.0f), f1(b1.x - 1.0f, b1.y - 1.0f);
    float fr = R > 1.0f ? R - 1.0f : 0.0f;
    if (gloss > 0)
        AddRoundedRectFilledGradV(dl, f0, ImVec2(f1.x, f0.y + (f1.y - f0.y) * 0.45f),
                                  IM_COL32(255, 255, 255, gloss), IM_COL32(255, 255, 255, 0), fr);
    if (shadow > 0)
        AddRoundedRectFilledGradV(dl, ImVec2(f0.x, f0.y + (f1.y - f0.y) * 0.55f), f1,
                                  IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, shadow), fr);

    dl->AddRect(ImVec2(b0.x + 0.5f, b0.y + 0.5f), ImVec2(b1.x - 0.5f, b1.y - 0.5f),
                border_col, R > 0.5f ? R - 0.5f : 0.0f, 0, 1.0f);
}

static ImU32 OxCol(ImVec4 c, int a = 255) {
    return IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), a);
}

static ImVec4 OxLight(ImVec4 c) {
    return ImVec4(c.x*1.3f > 1.0f ? 1.0f : c.x*1.3f,
                  c.y*1.3f > 1.0f ? 1.0f : c.y*1.3f,
                  c.z*1.3f > 1.0f ? 1.0f : c.z*1.3f, 1.0f);
}

static ImVec4 OxDark(ImVec4 c, float k = 0.4f) {
    return ImVec4(c.x*k, c.y*k, c.z*k, 1.0f);
}

static void glass_gloss(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rr, float topY, float botY, int aTop) {
    const int N = 8;
    const float arcC = mn.y + rr;
    auto narrow = [&](float y) {
        if (y >= arcC) return 0.0f;
        float d = arcC - y;
        return rr - sqrtf(rr * rr - d * d);
    };
    for (int i = 0; i < N; i++) {
        float ta = topY + (botY - topY) * i / N;
        float tb = topY + (botY - topY) * (i + 1) / N;
        float na = narrow(ta), nb = narrow(tb);
        float inset = (na > nb ? na : nb) + 0.75f;
        ImU32 ca = IM_COL32(255, 255, 255, (int)(aTop * (1.0f - i / (float)N)));
        ImU32 cb = IM_COL32(255, 255, 255, (int)(aTop * (1.0f - (i + 1) / (float)N)));
        dl->AddRectFilledMultiColor(ImVec2(mn.x + inset, ta), ImVec2(mx.x - inset, tb), ca, ca, cb, cb);
    }
}

// Scrollbar: scheme-gray center-light thumb (dark only at rim); accent eases in on
// hover and STAYS for the whole click-hold-move (sticky flag, raw rect tests).
static void oxygen_scrollbar() {
    ImGuiStyle& st = ImGui::GetStyle();
    const float smax = ImGui::GetScrollMaxY();
    if (smax <= 0.0f) return;
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    const float sbw = st.ScrollbarSize;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->PushClipRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), false);

    ImVec2 t0(wp.x + ws.x - sbw, wp.y);
    ImVec2 t1(wp.x + ws.x,       wp.y + ws.y);

    // Track: static scheme gray, no hover effect
    AddRoundedRectGradCenter(dl, t0, t1, OxCol(mulc(g_body_color, 1.15f)), OxCol(mulc(g_body_color, 0.80f)), st.ScrollbarRounding, 8);
    dl->AddRect(t0, t1, OxCol(mulc(g_body_color, 2.10f), 140), st.ScrollbarRounding, 0, 1.0f);

    const float sy = ImGui::GetScrollY();
    const float track_h = t1.y - t0.y;
    float thumb_h = track_h * (track_h / (track_h + smax));
    if (thumb_h < st.GrabMinSize) thumb_h = st.GrabMinSize;
    if (thumb_h > track_h) thumb_h = track_h;
    float ty = t0.y + (sy / smax) * (track_h - thumb_h);
    ImVec2 g0(t0.x + 2.0f, ty + 1.0f), g1(t1.x - 2.0f, ty + thumb_h - 1.0f);
    float gr = st.ScrollbarRounding - 2.0f; if (gr < 1.0f) gr = 1.0f;

    ImVec2 mp = ImGui::GetMousePos();
    bool in_track = mp.x >= t0.x && mp.x <= t1.x && mp.y >= t0.y && mp.y <= t1.y;
    bool in_thumb = mp.x >= g0.x - 1.0f && mp.x <= g1.x + 1.0f && mp.y >= g0.y - 1.0f && mp.y <= g1.y + 1.0f;

    static bool s_thumb_drag = false;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))                    s_thumb_drag = false;
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && in_track) s_thumb_drag = true;
    bool lit = in_thumb || s_thumb_drag;

    float t = widget_anim(ImGui::GetID("##sb_thumb"), lit);

    ImU32 center_c = lerp_u32(OxCol(mulc(g_body_color, 2.60f)), OxCol(g_accent_hover),  t);
    ImU32 edge_c   = lerp_u32(OxCol(mulc(g_body_color, 2.20f)), OxCol(g_accent_active), t);
    ImU32 rim_c    = lerp_u32(OxCol(mulc(g_body_color, 3.20f), 140), OxCol(OxLight(g_accent_color), 200), t);

    AddRoundedRectGradCenter(dl, g0, g1, center_c, edge_c, gr, 10);
    dl->AddRect(g0, g1, rim_c, gr, 0, 1.0f);

    dl->PopClipRect();
}

// Buttons: scheme-gray gradient idle; hover animates ONLY the border; fill eases in when pressed/forced.
static bool oxygen_button(const char* label, ImVec2 size_arg = ImVec2(0, 0), ImU32 c1_override = 0, ImU32 c2_override = 0, bool force_colored = false) {
    const float R = OXY_R;
    ImVec2 ls = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 sz = size_arg;
    if (sz.x <= 0) sz.x = ls.x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (sz.y <= 0) sz.y = ImGui::GetFrameHeight();
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 mx(mn.x + sz.x, mn.y + sz.y);

    bool clicked = ImGui::InvisibleButton(label, sz);
    bool hov = ImGui::IsItemHovered();
    bool act = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGuiID id = ImGui::GetID(label);
    float t_h = widget_anim(id, hov);
    float t_a = widget_anim(id + 1, act || force_colored);

    ImU32 c1, c2, bc;
    int gloss;
    if (c1_override && c2_override) {
        c1 = c1_override; c2 = c2_override;
        bc = OxCol(OxLight(g_accent_color), 180);
        gloss = 120;
    } else {
        ImU32 g1 = OxCol(mulc(g_body_color, 2.20f));
        ImU32 g2 = OxCol(mulc(g_body_color, 1.45f));
        ImU32 a1 = OxCol(act ? g_accent_hover : g_accent_color);
        ImU32 a2 = OxCol(OxDark(act ? g_accent_hover : g_accent_color));
        c1 = lerp_u32(g1, a1, t_a);
        c2 = lerp_u32(g2, a2, t_a);
        bc = lerp_u32(OxCol(mulc(g_body_color, 3.60f), 180), OxCol(OxLight(g_accent_color), 220), t_h > t_a ? t_h : t_a);
        gloss = 60 + (int)(60 * t_a);
    }
    OxFrame(dl, mn, mx, R, c1, c2, bc, gloss, 60);

    const char* tend = strstr(label, "##");
    dl->AddText(ImVec2(mn.x + (sz.x - ls.x) * 0.5f, mn.y + (sz.y - ls.y) * 0.5f),
                IM_COL32(238, 246, 255, 255), label, tend ? tend : nullptr);
    return clicked;
}

static void keybind_button(const char* label, int& key_code) {
    ImGui::AlignTextToFramePadding();
    char buf[64];
    if (g_binding_target == &key_code)      snprintf(buf, sizeof(buf), "Press a key...##%s", label);
    else if (key_code == 0)                 snprintf(buf, sizeof(buf), "Click to bind##%s", label);
    else                                    snprintf(buf, sizeof(buf), "%s##%s", key_name(key_code), label);
    if (oxygen_button(buf, ImVec2(150, 0))) g_binding_target = &key_code;
    ImGui::SameLine();
    ImGui::Text("%s", label);
}

static void keybind_row(const char* label, int& key_code) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LABEL_W);
    char buf[64];
    if (g_binding_target == &key_code)      snprintf(buf, sizeof(buf), "Press a key...##%s", label);
    else if (key_code == 0)                 snprintf(buf, sizeof(buf), "Click to bind##%s", label);
    else                                    snprintf(buf, sizeof(buf), "%s##%s", key_name(key_code), label);
    if (oxygen_button(buf, ImVec2(140, 0))) g_binding_target = &key_code;
}

// Slider: gray track; thumb pill has 2px side padding and a subtle rim-dark gradient.
static bool row_slider(const char* label, double* v, double lo, double hi, const char* fmt) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LABEL_W);
    ImGui::SetNextItemWidth(-FLT_MIN);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    bool changed = ImGui::SliderScalar(id, ImGuiDataType_Double, v, &lo, &hi, fmt);

    ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float FR = ImGui::GetStyle().FrameRounding;

    glass_gloss(dl, r0, r1, FR, r0.y + 1.0f, r0.y + (r1.y - r0.y) * 0.55f, 36);

    float grab_w = ImGui::GetStyle().GrabMinSize;
    float t = (float)((*v - lo) / (hi - lo)); if (t < 0) t = 0; if (t > 1) t = 1;
    const float pad = 2.0f;   // keep the thumb off the track's rounded ends
    float gx = r0.x + pad + ((r1.x - r0.x) - grab_w - pad * 2.0f) * t;
    ImVec2 g0(gx, r0.y + 3), g1(gx + grab_w, r1.y - 3);
    bool hov = ImGui::IsItemHovered(); bool act = ImGui::IsItemActive();

    float ta = widget_anim(ImGui::GetID(id), hov || act);
    ImU32 center_c = lerp_u32(OxCol(mulc(g_body_color, 4.60f)), OxCol(g_accent_hover),  ta);
    ImU32 edge_c   = lerp_u32(OxCol(mulc(g_body_color, 3.40f)), OxCol(g_accent_active), ta);
    ImU32 rim_c    = lerp_u32(OxCol(mulc(g_body_color, 4.80f), 160), OxCol(OxLight(g_accent_color), 200), ta);

    float gr = (g1.x - g0.x) * 0.5f; if (gr > OXY_R) gr = OXY_R;
    AddRoundedRectGradCenter(dl, g0, g1, center_c, edge_c, gr, 8);
    dl->AddRect(g0, g1, rim_c, gr, 0, 1.0f);
    return changed;
}

// Combo: frame rect computed BEFORE BeginCombo; hover = border only, open = gradient fill (eased).
static bool combo_framed(const char* id, int* idx, const char* items) {
    int count = 0;
    for (const char* p = items; *p; p += strlen(p) + 1) count++;
    const char* preview = items;
    for (int i = 0; i < *idx && *preview; i++) preview += strlen(preview) + 1;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    // Tight popup padding so the open list hugs its border (captured at popup Begin)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));

    const float FR = ImGui::GetStyle().FrameRounding;
    const float arrow_sz = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f;

    ImVec2 f0 = ImGui::GetCursorScreenPos();
    const float fw = ImGui::CalcItemWidth();
    const float fhgt = ImGui::GetFrameHeight();
    ImVec2 f1(f0.x + fw, f0.y + fhgt);

    ImGuiID wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();   // parent draw list, captured pre-popup

    bool changed = false;
    bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton);
    ImGui::PopStyleVar();   // WindowPadding already consumed by the popup's Begin

    bool fhov = ImGui::IsMouseHoveringRect(f0, f1);
    bool fact = fhov && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    float t_h = widget_anim(wid, fhov);
    float t_o = widget_anim(wid + 1, open);

    ImU32 c1 = lerp_u32(OxCol(mulc(g_body_color, 2.20f)), OxCol(g_accent_color), t_o);
    ImU32 c2 = lerp_u32(OxCol(mulc(g_body_color, 1.45f)), OxCol(OxDark(g_accent_color)), t_o);
    ImU32 bc = lerp_u32(OxCol(mulc(g_body_color, 3.60f), 200), OxCol(OxLight(g_accent_color), 220), t_h > t_o ? t_h : t_o);
    OxFrame(dl, f0, f1, FR, c1, c2, bc, 60 + (int)(80 * t_o), 60);

    ImVec2 ls = ImGui::CalcTextSize(preview, nullptr, true);
    dl->AddText(ImVec2(f0.x + ImGui::GetStyle().FramePadding.x,
                       f0.y + (f1.y - f0.y - ls.y) * 0.5f),
                lerp_u32(OxCol(g_text_color, 205), IM_COL32(238, 246, 255, 255), t_o), preview, nullptr);

    // Arrow: bigger down-arrow when closed (matches section headers), up-arrow when open
    float ax = f1.x - arrow_sz * 0.5f, ay = (f0.y + f1.y) * 0.5f;
    if (open) {
        float as = 4.5f;
        dl->AddTriangleFilled(ImVec2(ax - as, ay + as * 0.5f),
                              ImVec2(ax + as, ay + as * 0.5f),
                              ImVec2(ax, ay - as * 0.5f),
                              IM_COL32(255, 255, 255, 220));
    } else {
        float as = 6.0f;
        dl->AddTriangleFilled(ImVec2(ax - as, ay - as * 0.5f),
                              ImVec2(ax + as, ay - as * 0.5f),
                              ImVec2(ax, ay + as * 0.5f),
                              IM_COL32(255, 255, 255, 220));
    }

    if (open) {
        ImGui::PushStyleColor(ImGuiCol_Border, mulc(g_body_color, 4.20f, 0.75f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

        ImDrawList* pdl = ImGui::GetWindowDrawList();   // popup draw list
        const char* p = items;
        for (int i = 0; i < count; i++) {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float h = ImGui::GetFrameHeight();
            ImVec2 r1(p0.x + w, p0.y + h);

            char oid[32]; snprintf(oid, sizeof(oid), "##opt%d", i);
            bool clicked = ImGui::InvisibleButton(oid, ImVec2(w, h));
            bool hov = ImGui::IsItemHovered();
            bool down = hov && ImGui::IsMouseDown(ImGuiMouseButton_Left);

            ImGuiID oid_id = ImGui::GetID(oid);
            float o_h = widget_anim(oid_id, hov);
            float o_s = widget_anim(oid_id + 1, i == *idx);

            // selected: subtle accent tint + faint gloss (no heavy gradient)
            if (o_s > 0.01f) {
                pdl->AddRectFilled(p0, r1, OxCol(g_accent_color, (int)(70 * o_s)), OXY_R);
                glass_gloss(pdl, p0, r1, OXY_R, p0.y + 1.0f, p0.y + (r1.y - p0.y) * 0.55f, (int)(26 * o_s));
            }
            // pressed: small extra tint
            if (down)
                pdl->AddRectFilled(p0, r1, OxCol(g_accent_active, 60), OXY_R);
            // hover = border only (selected keeps its border too)
            float b_t = o_h > o_s ? o_h : o_s;
            if (b_t > 0.01f)
                pdl->AddRect(ImVec2(p0.x + 0.5f, p0.y + 0.5f), ImVec2(r1.x - 0.5f, r1.y - 0.5f),
                             lerp_u32(IM_COL32(0,0,0,0), OxCol(OxLight(g_accent_color), 200), b_t), OXY_R, 0, 1.0f);

            ImVec2 tls = ImGui::CalcTextSize(p, nullptr, true);
            ImU32 tcol = (i == *idx) ? IM_COL32(240, 246, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text);
            pdl->AddText(ImVec2(p0.x + ImGui::GetStyle().FramePadding.x, p0.y + (h - tls.y) * 0.5f), tcol, p, nullptr);

            if (clicked) { *idx = i; changed = true; ImGui::CloseCurrentPopup(); }
            p += strlen(p) + 1;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(1);
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    return changed;
}

static bool row_combo(const char* label, int* idx, const char* items) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LABEL_W);
    ImGui::SetNextItemWidth(-FLT_MIN);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return combo_framed(id, idx, items);
}

static void mode_combo(const char* label, ActivationMode& mode) {
    int idx = mode == ActivationMode::TOGGLE ? 0 : (mode == ActivationMode::HOLD ? 1 : 2);
    if (combo_framed(label, &idx, "Toggle\0Hold\0Mouse\0")) {
        mode = idx == 0 ? ActivationMode::TOGGLE : (idx == 1 ? ActivationMode::HOLD : ActivationMode::MOUSE);
    }
}

// Checkbox: gray idle; hover = accent border only; checked = accent fill + check fades in.
static bool oxygen_checkbox(const char* label, bool* v) {
    const float sz = ImGui::GetFrameHeight();
    const float R  = OXY_R;
    const char* tend = strstr(label, "##");
    ImVec2 ls = ImGui::CalcTextSize(label, nullptr, true, tend ? (float)(tend - label) : -1.0f);
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 total(sz + (ls.x > 0.0f ? ImGui::GetStyle().ItemInnerSpacing.x + ls.x : 0.0f), sz);

    bool clicked = ImGui::InvisibleButton(label, total);
    if (clicked) *v = !*v;
    bool hov = ImGui::IsItemHovered();
    bool act = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGuiID id = ImGui::GetID(label);
    float t_h = widget_anim(id, hov);
    float t_c = widget_anim(id + 1, *v);

    ImVec2 bx1(mn.x + sz, mn.y + sz);
    ImU32 fill = lerp_u32(OxCol(mulc(g_body_color, 1.90f), 235), OxCol(act ? g_accent_hover : g_accent_active), t_c);
    ImU32 rimc = lerp_u32(OxCol(mulc(g_body_color, 3.60f), 200), OxCol(OxLight(g_accent_color), 220), t_h > t_c ? t_h : t_c);

    dl->AddRectFilled(mn, bx1, fill, R);
    glass_gloss(dl, mn, bx1, R, mn.y + 1.0f, mn.y + sz * 0.55f, 36 + (int)(44 * t_c));
    dl->AddRect(mn, bx1, rimc, R, 0, 1.0f);

    if (t_c > 0.01f) {
        const float t = (sz * 0.14f < 2.0f) ? 2.0f : (sz * 0.14f);
        ImVec2 pts[3] = {
            ImVec2(mn.x + sz * 0.24f, mn.y + sz * 0.52f),
            ImVec2(mn.x + sz * 0.42f, mn.y + sz * 0.70f),
            ImVec2(mn.x + sz * 0.78f, mn.y + sz * 0.30f),
        };
        dl->AddPolyline(pts, 3, IM_COL32(245, 250, 255, (int)(255 * t_c)), 0, t);
    }

    if (ls.x > 0.0f)
        dl->AddText(ImVec2(bx1.x + ImGui::GetStyle().ItemInnerSpacing.x, mn.y + (sz - ls.y) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_Text), label, tend ? tend : nullptr);
    return clicked;
}

static bool row_check(const char* label, bool* v) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LABEL_W);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return oxygen_checkbox(id, v);
}

// Section header: gray like buttons; hover = border only; press eases accent fill.
static bool oxygen_section(const char* label, bool default_open) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    char id[64];
    snprintf(id, sizeof(id), "##section_%s", label);

    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetFrameHeight();

    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    bool act = ImGui::IsItemActive();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID open_id = ImGui::GetID(label);
    bool is_open = storage->GetBool(open_id, default_open);
    if (clicked) {
        is_open = !is_open;
        storage->SetBool(open_id, is_open);
    }

    ImVec2 mn = ImGui::GetItemRectMin();
    ImVec2 mx = ImGui::GetItemRectMax();
    const float R = OXY_R;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGuiID wid = ImGui::GetID(id);
    float t_h = widget_anim(wid, hov);
    float t_a = widget_anim(wid + 1, act);

    ImU32 c1 = lerp_u32(OxCol(mulc(g_body_color, 2.20f)), OxCol(g_accent_color), t_a);
    ImU32 c2 = lerp_u32(OxCol(mulc(g_body_color, 1.45f)), OxCol(OxDark(g_accent_color)), t_a);
    ImU32 bc = lerp_u32(OxCol(mulc(g_body_color, 3.60f), 180), OxCol(OxLight(g_accent_color), 220), t_h > t_a ? t_h : t_a);
    OxFrame(dl, mn, mx, R, c1, c2, bc, 60 + (int)(80 * t_a), 0);

    float arrow_x = mx.x - 20.0f;
    float arrow_y = mn.y + h * 0.5f;
    float arrow_sz = 6.0f;
    if (is_open) {
        dl->AddTriangleFilled(ImVec2(arrow_x - arrow_sz, arrow_y - arrow_sz*0.5f),
                              ImVec2(arrow_x + arrow_sz, arrow_y - arrow_sz*0.5f),
                              ImVec2(arrow_x, arrow_y + arrow_sz*0.5f),
                              IM_COL32(255,255,255,220));
    } else {
        dl->AddTriangleFilled(ImVec2(arrow_x - arrow_sz*0.5f, arrow_y - arrow_sz),
                              ImVec2(arrow_x + arrow_sz*0.5f, arrow_y),
                              ImVec2(arrow_x - arrow_sz*0.5f, arrow_y + arrow_sz),
                              IM_COL32(255,255,255,220));
    }

    ImVec2 text_pos(mn.x + 10.0f, mn.y + (h - ImGui::GetTextLineHeight()) * 0.5f);
    dl->AddText(text_pos, IM_COL32(255,255,255,255), label);

    ImGui::PopStyleVar();
    return is_open;
}

static void status_chip(const char* label, bool on) {
    ImVec2 pad(9, 4);
    ImVec2 ls = ImGui::CalcTextSize(label);
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 sz(ls.x + pad.x * 2, ls.y + pad.y * 2);
    ImVec2 mx(mn.x + sz.x, mn.y + sz.y);
    const float r = OXY_R;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImU32 fill_on  = IM_COL32(46, 148, 84, 235);
    const ImU32 rim_on   = IM_COL32(150, 230, 180, 160);
    const ImU32 text_on  = IM_COL32(235, 255, 242, 255);
    const ImU32 fill_off = IM_COL32(255, 255, 255, 22);
    const ImU32 rim_off  = IM_COL32(255, 255, 255, 45);
    const ImU32 text_off = IM_COL32(200, 212, 228, 150);

    dl->AddRectFilled(mn, mx, on ? fill_on : fill_off, r);
    if (on)
        glass_gloss(dl, mn, mx, r, mn.y + 1.0f, mn.y + sz.y * 0.55f, 70);
    dl->AddRect(mn, mx, on ? rim_on : rim_off, r, 0, 1.0f);
    dl->AddText(ImVec2(mn.x + pad.x, mn.y + pad.y), on ? text_on : text_off, label);
    ImGui::Dummy(sz);
}

static int g_tab = 0;
static void segmented_tabs() {
    static const char* names[] = { "Left", "Right", "General", "Macros", "Config", "Analytics" };
    const int count = 6;
    const float R  = OXY_R;
    const float PR = R - 1.0f;
    const float pad_x = 14.0f;
    const float h = ImGui::GetFrameHeight();

    const ImU32 track   = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 rim     = ImGui::GetColorU32(ImGuiCol_Separator);
    const ImU32 txtSel  = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 txt     = ImGui::GetColorU32(ImGuiCol_Text, 0.80f);

    ImVec2 ls[count]; float w[count], total = 0.0f;
    for (int i = 0; i < count; i++) {
        ls[i] = ImGui::CalcTextSize(names[i]);
        w[i] = ls[i].x + pad_x * 2.0f;
        total += w[i];
    }

    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 mx(mn.x + total, mn.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(mn, mx, track, R);
    dl->AddRect(mn, mx, rim, R, 0, 1.0f);

    float x = mn.x;
    for (int i = 0; i < count; i++) {
        ImGui::SetCursorScreenPos(ImVec2(x, mn.y));
        bool sel = (g_tab == i);
        ImVec2 s0(x, mn.y), s1(x + w[i], mn.y + h);
        bool clicked = ImGui::InvisibleButton(names[i], ImVec2(w[i], h));
        bool hov = ImGui::IsItemHovered();

        ImGuiID wid = ImGui::GetID(names[i]);
        float t_h = widget_anim(wid, hov);
        float t_s = widget_anim(wid + 1, sel);

        ImVec2 i0(s0.x + 2, s0.y + 2), i1(s1.x - 2, s1.y - 2);
        if (t_s > 0.01f) {
            dl->AddRectFilled(i0, i1, lerp_u32(IM_COL32(0,0,0,0), OxCol(g_accent_active), t_s), PR);
            glass_gloss(dl, i0, i1, PR, i0.y + 1.0f, i0.y + (i1.y - i0.y) * 0.58f, (int)(80 * t_s));
            dl->AddRect(i0, i1, lerp_u32(IM_COL32(0,0,0,0), OxCol(OxLight(g_accent_color), 200), t_s), PR, 0, 1.0f);
        } else if (t_h > 0.01f) {
            dl->AddRectFilled(i0, i1, IM_COL32(g_accent_color.x*255, g_accent_color.y*255, g_accent_color.z*255, (int)(64 * t_h)), PR);
        }
        if (i > 0 && !sel && g_tab != i - 1)
            dl->AddLine(ImVec2(s0.x, s0.y + 5), ImVec2(s0.x, s1.y - 5), rim, 1.0f);

        dl->AddText(ImVec2(s0.x + (w[i] - ls[i].x) * 0.5f, s0.y + (h - ls[i].y) * 0.5f),
                    sel ? txtSel : txt, names[i]);
        if (clicked) g_tab = i;
        x += w[i];
    }
    ImGui::SetCursorScreenPos(ImVec2(mn.x, mx.y));
    ImGui::Spacing();
}

// remembers which preset button was applied per side
static int s_applied_preset[2] = { -1, -1 };

static void draw_clicker_tab(ClickerProfile& profile, ActivationMode& mode,
                             Clicker& clicker, int& key,
                             const ClickerPreset* presets, int side) {
    if (clicker.is_enabled())
        ImGui::TextColored(ImVec4(0.55f, 0.90f, 0.65f, 1.0f),
                           "ACTIVE  -  %.1f avg CPS  -  %u clicks",
                           clicker.tracker().avg_cps(), clicker.click_count());
    else
        ImGui::TextDisabled("Idle  -  press your toggle key to start");
    ImGui::Spacing();

    if (oxygen_section("Quick Setup", true)) {
        keybind_row("Toggle Key", key);

        ImGui::TextUnformatted("Preset");
        const float sp = ImGui::GetStyle().ItemSpacing.x;
        const float gap = ImGui::GetStyle().ItemSpacing.y;
        const float row_x = ImGui::GetCursorPosX();
        const float content_w = ImGui::GetContentRegionAvail().x;
        const float btn_h = ImGui::GetFrameHeight();
        float line_y = ImGui::GetCursorPosY();
        float x_used = 0.0f;
        for (int i = 0; i < PRESET_COUNT; i++) {
            ImVec2 pls = ImGui::CalcTextSize(presets[i].name, nullptr, true);
            float bw = pls.x + ImGui::GetStyle().FramePadding.x * 2.0f;
            if (i > 0) {
                if (x_used + sp + bw > content_w) {
                    line_y += btn_h + gap;
                    ImGui::SetCursorPos(ImVec2(row_x, line_y));
                    x_used = 0.0f;
                } else {
                    ImGui::SameLine(); x_used += sp;
                }
            }
            bool is_applied = (s_applied_preset[side] == i);
            if (oxygen_button(presets[i].name, ImVec2(0, 0), 0, 0, is_applied)) {
                int saved_button = profile.mouse_button;
                profile = presets[i].profile;
                profile.mouse_button = saved_button;
                s_applied_preset[side] = i;
            }
            x_used += bw;
        }
        ImGui::SetCursorPosY(line_y + btn_h + gap);

        int idx = mode == ActivationMode::TOGGLE ? 0 : (mode == ActivationMode::HOLD ? 1 : 2);
        if (row_combo("Activation", &idx, "Toggle\0Hold\0Mouse\0"))
            mode = idx == 0 ? ActivationMode::TOGGLE : (idx == 1 ? ActivationMode::HOLD : ActivationMode::MOUSE);

        row_slider("Speed (CPS)", &profile.cps_mean, 1.0, 25.0, "%.1f");
    }

    if (oxygen_section("Advanced Tuning", false)) {
        row_slider("CPS Variation", &profile.cps_stddev, 0.0, 5.0, "%.2f");
        row_slider("Min CPS", &profile.cps_min, 1.0, 20.0, "%.1f");
        row_slider("Max CPS", &profile.cps_max, 5.0, 30.0, "%.1f");
        int dist_idx = (int)profile.distribution;
        if (row_combo("Distribution", &dist_idx, "Gaussian\0Poisson\0Uniform\0"))
            profile.distribution = (DistributionType)dist_idx;

        ImGui::Spacing(); ImGui::TextDisabled("HUMANIZATION");
        row_slider("Spike Chance", &profile.spike_chance, 0.0, 0.2, "%.3f");
        if (profile.spike_chance > 0) {
            row_slider("Spike Min", &profile.spike_min, 0.0, 5.0, "%.1f");
            row_slider("Spike Max", &profile.spike_max, 0.0, 8.0, "%.1f");
        }
        row_slider("Stutter Chance", &profile.stutter_chance, 0.0, 0.2, "%.3f");
        if (profile.stutter_chance > 0) {
            row_slider("Stutter Min", &profile.stutter_min, 0.0, 8.0, "%.1f");
            row_slider("Stutter Max", &profile.stutter_max, 0.0, 10.0, "%.1f");
        }

        ImGui::Spacing(); ImGui::TextDisabled("HOLD TIMING");
        row_slider("Hold Mean (ms)",   &profile.hold_ms_mean,   10.0, 100.0, "%.1f");
        row_slider("Hold StdDev (ms)", &profile.hold_ms_stddev,  0.0,  20.0, "%.1f");
        row_slider("Hold Min (ms)",    &profile.hold_ms_min,     5.0,  50.0, "%.1f");
        row_slider("Hold Max (ms)",    &profile.hold_ms_max,    20.0, 150.0, "%.1f");
        row_slider("Heavy Hold Chance", &profile.hold_ms_heavy_chance, 0.0, 0.1, "%.3f");
        if (profile.hold_ms_heavy_chance > 0) {
            row_slider("Heavy Hold Min", &profile.hold_ms_heavy_min, 5.0, 50.0, "%.1f");
            row_slider("Heavy Hold Max", &profile.hold_ms_heavy_max, 10.0, 80.0, "%.1f");
        }
        row_slider("Rhythm Volatility", &profile.rhythm_volatility, 0.0, 2.0, "%.2f");
        row_slider("Rhythm Tension",    &profile.rhythm_tension,    0.0, 0.2, "%.3f");
        row_slider("CPS Fallout",       &profile.cps_minmax_fallout,0.0, 3.0, "%.2f");
        row_slider("Start Delay (ms)",  &profile.start_delay_ms,    0.0, 500.0, "%.0f");

        ImGui::Spacing(); ImGui::TextDisabled("DOUBLE CLICK");
        row_slider("Chance", &profile.double_click_chance, 0.0, 0.3, "%.3f");
        if (profile.double_click_chance > 0) {
            row_slider("Delay Min (ms)", &profile.double_click_delay_min,  5.0,  80.0, "%.0f");
            row_slider("Delay Max (ms)", &profile.double_click_delay_max, 10.0, 120.0, "%.0f");
        }

        ImGui::Spacing(); ImGui::TextDisabled("FATIGUE BREAKS");
        row_check("Enable Breaks", &profile.breaks_enabled);
        if (profile.breaks_enabled) {
            row_slider("Break Every Min (s)",    &profile.break_interval_min,  5.0, 120.0, "%.1f");
            row_slider("Break Every Max (s)",    &profile.break_interval_max,  5.0, 120.0, "%.1f");
            row_slider("Break Duration Min (s)", &profile.break_duration_min,  0.1,   5.0, "%.1f");
            row_slider("Break Duration Max (s)", &profile.break_duration_max,  0.1,   5.0, "%.1f");
        }
    }
}

static void setup_kde_cursor() {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string path = std::string(home) + "/.config/kcminputrc";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;
    char line[256];
    bool in_mouse = false;
    char theme[128] = "";
    char size[32] = "";
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "[Mouse]", 7) == 0) {
            in_mouse = true;
            continue;
        }
        if (line[0] == '[') {
            in_mouse = false;
            continue;
        }
        if (in_mouse) {
            char* eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char* key = line;
                char* val = eq + 1;
                while (*key == ' ' || *key == '\t') key++;
                char* end = key + strlen(key) - 1;
                while (end > key && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
                while (*val == ' ' || *val == '\t') val++;
                end = val + strlen(val) - 1;
                while (end > val && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
                if (strcmp(key, "cursorTheme") == 0) strncpy(theme, val, sizeof(theme)-1);
                else if (strcmp(key, "cursorSize") == 0) strncpy(size, val, sizeof(size)-1);
            }
        }
    }
    fclose(f);
    if (theme[0]) setenv("XCURSOR_THEME", theme, 1);
    if (size[0]) setenv("XCURSOR_SIZE", size, 1);
}

void gui_run(GuiState& state) {
    setup_kde_cursor();

    // Follow the KDE color scheme for the window body + text (Oxygen/Breeze grays)
    {
        float r, g, b;
        if (read_kde_color("Colors:Window", "BackgroundNormal", r, g, b))
            g_body_color = ImVec4(r, g, b, 1.0f);
        if (read_kde_color("Colors:Window", "ForegroundNormal", r, g, b))
            g_text_color = ImVec4(r, g, b, 1.0f);
    }

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    // Real compositor decorations (KWin/Oxygen titlebar), like Prism Launcher relies on
    // the system decoration instead of faking a client-side titlebar.
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(700, 600, "Ghast Clicker", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return;
    }
    g_window = window;
    glfwSetWindowSizeLimits(window, 520, 420, GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // Real font, a bit larger for the global scale bump
    {
        const char* candidates[] = {
            "/usr/share/fonts/inter/Inter-Regular.otf",
            "/usr/share/fonts/truetype/inter/Inter-Regular.otf",
            "/usr/share/fonts/roboto/Roboto-Regular.ttf",
            "/usr/share/fonts/truetype/roboto/Roboto-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/cantarell/Cantarell-VF.otf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        };
        ImFont* font = nullptr;
        for (const char* p : candidates) {
            if (FILE* f = fopen(p, "rb")) { fclose(f); font = io.Fonts->AddFontFromFileTTF(p, 18.0f); break; }
        }
        if (!font) io.Fonts->AddFontDefault();
    }

    setup_oxygen_theme(state.config.accent_preset, state.config.accent_r, state.config.accent_g, state.config.accent_b);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    static char profile_name_buf[64] = "";
    int selected_profile = -1;
    auto profiles = ConfigFile::list_profiles();
    char status_msg[128] = "";
    float status_timer = 0;

    bool always_on_top = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // Scheme-gray body with an oxygen gloss "drop" from the titlebar into it
        {
            ImU32 body_col = OxCol(g_body_color);
            ImU32 drop_top = OxCol(mulc(g_body_color, 1.50f));
            const float DROP_H = 120.0f;
            ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
            bgdl->AddRectFilled(ImVec2(0, 0), ImVec2((float)display_w, (float)display_h), body_col);
            bgdl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2((float)display_w, DROP_H),
                                          drop_top, drop_top, body_col, body_col);
            bgdl->AddLine(ImVec2(0, 0), ImVec2((float)display_w, 0), OxCol(mulc(g_body_color, 2.20f), 70));
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)display_w, (float)display_h));

        ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                              ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("Ghast Clicker", nullptr, wf)) {
            bool l_on = state.left.is_enabled() || state.left_armed;
            bool r_on = state.right.is_enabled() || state.right_armed;
            status_chip(l_on ? "L: ON" : "L: off", l_on);
            ImGui::SameLine(0, 6);
            status_chip(r_on ? "R: ON" : "R: off", r_on);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 70.0f);
            if (oxygen_button(always_on_top ? "pinned" : "pin", ImVec2(70, 0), 0, 0, always_on_top)) {
                always_on_top = !always_on_top;
                glfwSetWindowAttrib(window, GLFW_FLOATING, always_on_top ? GLFW_TRUE : GLFW_FALSE);
            }
            ImGui::Spacing();
            segmented_tabs();
            ImGui::Separator();

            switch (g_tab) {
                case 0:
                    draw_clicker_tab(state.config.left, state.config.left_mode,
                                     state.left, state.config.key_left, LEFT_PRESETS, 0);
                    break;

                case 1:
                    draw_clicker_tab(state.config.right, state.config.right_mode,
                                     state.right, state.config.key_right, RIGHT_PRESETS, 1);
                    break;

                case 2: {
                    ImGui::Spacing();

                    ImGui::Text("Keybinds");
                    ImGui::Separator();
                    ImGui::BeginDisabled(!state.config.inventory_key_enabled);
                    keybind_button("Inventory Key", state.config.key_inventory);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    oxygen_checkbox("Enabled##inv", &state.config.inventory_key_enabled);
                    ImGui::TextDisabled("Pauses clicking while inventory is open");

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Hotbar Slot Filter");
                    ImGui::TextDisabled("Only click when holding certain slots (e.g. sword slot)");
                    oxygen_checkbox("Enable Slot Filter", &state.config.slot_filter_enabled);
                    if (state.config.slot_filter_enabled) {
                        for (int i = 0; i < 9; i++) {
                            if (i > 0) ImGui::SameLine();
                            char slabel[4];
                            snprintf(slabel, sizeof(slabel), "%d", i + 1);
                            oxygen_checkbox(slabel, &state.config.allowed_slots[i]);
                        }
                        ImGui::Text("Active slot: %d", state.active_slot + 1);
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Statistics");
                    const auto& lt = state.left.tracker();
                    const auto& rt = state.right.tracker();
                    ImGui::Text("Left  - CPS: %.1f  Hold: %.1f ms  Total: %u",
                                lt.avg_cps(), lt.avg_hold_ms(), state.left.click_count());
                    ImGui::Text("Right - CPS: %.1f  Hold: %.1f ms  Total: %u",
                                rt.avg_cps(), rt.avg_hold_ms(), state.right.click_count());

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Audio");
                    oxygen_checkbox("Toggle Sound", &state.config.audio_enabled);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Play a beep when clickers toggle on/off");

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Session Stats");
                    {
                        bool any_active = state.left.is_enabled() || state.right.is_enabled();
                        if (any_active)
                            state.active_time_s += io.DeltaTime;

                        if (state.left.is_enabled()) {
                            double cps = lt.avg_cps();
                            if (cps > state.peak_cps_left) state.peak_cps_left = cps;
                        }
                        if (state.right.is_enabled()) {
                            double cps = rt.avg_cps();
                            if (cps > state.peak_cps_right) state.peak_cps_right = cps;
                        }

                        auto elapsed = std::chrono::steady_clock::now() - state.session_start;
                        double session_secs = std::chrono::duration<double>(elapsed).count();
                        int s_min = (int)session_secs / 60;
                        int s_sec = (int)session_secs % 60;
                        int a_min = (int)state.active_time_s / 60;
                        int a_sec = (int)state.active_time_s % 60;
                        double pct = session_secs > 0 ? state.active_time_s / session_secs * 100.0 : 0;
                        uint32_t total = state.left.click_count() + state.right.click_count();
                        double avg = state.active_time_s > 0 ? total / state.active_time_s : 0;

                        ImGui::Text("Duration: %dm %02ds", s_min, s_sec);
                        ImGui::Text("Active: %dm %02ds (%.0f%%)", a_min, a_sec, pct);
                        ImGui::Text("Total Clicks: %u (L: %u | R: %u)",
                                    total, state.left.click_count(), state.right.click_count());
                        ImGui::Text("Session Avg CPS: %.1f", avg);
                        ImGui::Text("Peak CPS - L: %.1f | R: %.1f",
                                    state.peak_cps_left, state.peak_cps_right);
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    oxygen_checkbox("Show Debug", &state.show_debug);
                    if (state.show_debug) {
                        ImGui::Text("FPS: %.1f (%.1f ms)", io.Framerate, 1000.0f / io.Framerate);
                        ImGui::Text("Menu paused: %s", state.in_menu ? "Yes" : "No");
                        ImGui::Text("Left armed: %s", state.left_armed ? "Yes" : "No");
                        ImGui::Text("Right armed: %s", state.right_armed ? "Yes" : "No");
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Ghast Clicker - Linux/Wayland Port");
                    break;
                }

                case 3: {
                    ImGui::Spacing();
                    ImGui::Text("Macro Sequences");
                    ImGui::Separator();

                    for (int m = 0; m < MAX_MACROS; m++) {
                        if (m > 0) ImGui::SameLine();
                        char mlabel[16];
                        snprintf(mlabel, sizeof(mlabel), "%d##mtab", m + 1);
                        bool active = (state.selected_macro == m);
                        ImU32 c1 = active ? IM_COL32(180, 100, 220, 255) : 0;
                        ImU32 c2 = active ? IM_COL32(90, 40, 110, 255) : 0;
                        if (oxygen_button(mlabel, ImVec2(40, 0), c1, c2))
                            state.selected_macro = m;
                    }

                    auto& mac = state.config.macros[state.selected_macro];

                    ImGui::InputText("Name", mac.name, sizeof(mac.name));
                    oxygen_checkbox("Loop", &mac.loop);
                    ImGui::SameLine();
                    keybind_button("Macro Key", mac.key_bind);

                    ImGui::Separator();
                    ImGui::Text("Steps (%d/%d)", mac.step_count, MAX_MACRO_STEPS);

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
                            case MacroAction::LEFT_CLICK:
                            case MacroAction::RIGHT_CLICK:
                                ImGui::SetNextItemWidth(60);
                                ImGui::InputInt("##cnt", &step.count, 0);
                                if (step.count < 1) step.count = 1;
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(80);
                                ImGui::InputDouble("ms##intv", &step.delay_ms, 0, 0, "%.0f");
                                if (step.delay_ms < 1) step.delay_ms = 1;
                                break;
                            case MacroAction::DELAY:
                                ImGui::SetNextItemWidth(100);
                                ImGui::InputDouble("ms##del", &step.delay_ms, 0, 0, "%.0f");
                                if (step.delay_ms < 1) step.delay_ms = 1;
                                break;
                            case MacroAction::SWITCH_SLOT:
                                ImGui::SetNextItemWidth(60);
                                ImGui::InputInt("Slot##sw", &step.slot, 0);
                                if (step.slot < 1) step.slot = 1;
                                if (step.slot > 9) step.slot = 9;
                                break;
                        }

                        ImGui::SameLine();
                        if (oxygen_button("X##del", ImVec2(24, 0)))
                            remove_idx = s;

                        ImGui::PopID();
                    }

                    if (remove_idx >= 0) {
                        for (int s = remove_idx; s < mac.step_count - 1; s++)
                            mac.steps[s] = mac.steps[s + 1];
                        mac.step_count--;
                    }

                    if (mac.step_count < MAX_MACRO_STEPS) {
                        if (oxygen_button("+ Add Step"))
                            mac.steps[mac.step_count++] = MacroStep{};
                    }

                    ImGui::Separator();
                    if (state.macro_runner.is_running()) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                            "Running (step %d/%d, loop %d)",
                            state.macro_runner.current_step() + 1,
                            mac.step_count, state.macro_runner.loop_count());
                        if (oxygen_button("Stop", ImVec2(100, 0)))
                            state.macro_runner.stop();
                    } else {
                        ImGui::TextDisabled("Idle");
                        if (mac.step_count > 0) {
                            if (oxygen_button("Run", ImVec2(100, 0)))
                                state.macro_runner.start(mac, &state.active_slot);
                        }
                    }
                    break;
                }

                case 4: {
                    ImGui::Spacing();
                    ImGui::Text("Config Profiles");
                    ImGui::Separator();

                    if (ImGui::BeginListBox("##profiles", ImVec2(-1, 100))) {
                        for (int i = 0; i < (int)profiles.size(); i++) {
                            bool selected = (selected_profile == i);
                            if (ImGui::Selectable(profiles[i].c_str(), selected))
                                selected_profile = i;
                        }
                        ImGui::EndListBox();
                    }

                    ImGui::InputText("##profile_name", profile_name_buf, sizeof(profile_name_buf));

                    if (oxygen_button("Save", ImVec2(80, 0))) {
                        std::string name = profile_name_buf[0] ? profile_name_buf : "config";
                        if (ConfigFile::save(state.config, name)) {
                            snprintf(status_msg, sizeof(status_msg), "Saved: %s", name.c_str());
                            status_timer = 3.0f;
                            profiles = ConfigFile::list_profiles();
                        }
                    }
                    ImGui::SameLine();
                    if (oxygen_button("Load", ImVec2(80, 0)) && selected_profile >= 0 && selected_profile < (int)profiles.size()) {
                        if (ConfigFile::load(state.config, profiles[selected_profile])) {
                            snprintf(status_msg, sizeof(status_msg), "Loaded: %s", profiles[selected_profile].c_str());
                            status_timer = 3.0f;
                        }
                    }
                    ImGui::SameLine();
                    if (oxygen_button("Delete", ImVec2(80, 0)) && selected_profile >= 0 && selected_profile < (int)profiles.size()) {
                        ConfigFile::remove_profile(profiles[selected_profile]);
                        snprintf(status_msg, sizeof(status_msg), "Deleted: %s", profiles[selected_profile].c_str());
                        status_timer = 3.0f;
                        profiles = ConfigFile::list_profiles();
                        selected_profile = -1;
                    }
                    ImGui::SameLine();
                    if (oxygen_button("Reset", ImVec2(80, 0))) {
                        state.config = Config();
                        snprintf(status_msg, sizeof(status_msg), "Reset to defaults");
                        status_timer = 3.0f;
                    }

                    if (status_timer > 0) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", status_msg);
                        status_timer -= io.DeltaTime;
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Appearance");

                    bool theme_changed = false;
                    if (combo_framed("Accent Preset", &state.config.accent_preset, "Oxygen Blue\0Red\0Green\0Purple\0Custom\0Follow KDE\0"))
                        theme_changed = true;

                    if (state.config.accent_preset == 4) { // Custom
                        if (ImGui::ColorEdit3("Custom Color", &state.config.accent_r))
                            theme_changed = true;
                    }

                    if (theme_changed) {
                        setup_oxygen_theme(state.config.accent_preset, state.config.accent_r, state.config.accent_g, state.config.accent_b);
                    }
                    break;
                }

                case 5: {
                    ImGui::Spacing();
                    ImGui::Text("Click Analytics");
                    ImGui::Separator();

                    ImGui::Text("Left CPS");
                    {
                        const auto& t = state.left.tracker();
                        char overlay[32];
                        snprintf(overlay, sizeof(overlay), "%.1f CPS", t.avg_cps());
                        ImGui::PlotLines("##lcps", t.graph_data(), t.graph_size(),
                                         t.graph_offset(), overlay, 0.0f, 35.0f, ImVec2(-1, 60));
                    }
                    ImGui::Text("Right CPS");
                    {
                        const auto& t = state.right.tracker();
                        char overlay[32];
                        snprintf(overlay, sizeof(overlay), "%.1f CPS", t.avg_cps());
                        ImGui::PlotLines("##rcps", t.graph_data(), t.graph_size(),
                                         t.graph_offset(), overlay, 0.0f, 35.0f, ImVec2(-1, 60));
                    }

                    ImGui::Separator();

                    bool left_rec = state.left.tracker().is_recording();
                    bool right_rec = state.right.tracker().is_recording();

                    if (oxygen_checkbox("Record Left", &left_rec))
                        state.left.tracker().set_recording(left_rec);
                    ImGui::SameLine();
                    if (oxygen_checkbox("Record Right", &right_rec))
                        state.right.tracker().set_recording(right_rec);

                    ImGui::Text("Left: %d samples  |  Right: %d samples",
                                state.left.tracker().history_size(),
                                state.right.tracker().history_size());

                    if (oxygen_button("Export CSV", ImVec2(120, 0))) {
                        std::string dir = ConfigFile::path();
                        time_t now = time(nullptr);
                        char ts[32];
                        strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", localtime(&now));

                        bool any = false;
                        if (state.left.tracker().history_size() > 0) {
                            std::string lpath = dir + "/analytics_left_" + ts + ".csv";
                            state.left.tracker().export_csv(lpath);
                            any = true;
                        }
                        if (state.right.tracker().history_size() > 0) {
                            std::string rpath = dir + "/analytics_right_" + ts + ".csv";
                            state.right.tracker().export_csv(rpath);
                            any = true;
                        }
                        if (any) {
                            snprintf(status_msg, sizeof(status_msg), "Exported to %s/", dir.c_str());
                            status_timer = 5.0f;
                        } else {
                            snprintf(status_msg, sizeof(status_msg), "No data to export. Enable recording first.");
                            status_timer = 3.0f;
                        }
                    }
                    ImGui::SameLine();
                    if (oxygen_button("Clear Data", ImVec2(120, 0))) {
                        state.left.tracker().clear_history();
                        state.right.tracker().clear_history();
                        snprintf(status_msg, sizeof(status_msg), "Analytics data cleared");
                        status_timer = 3.0f;
                    }

                    if (status_timer > 0) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", status_msg);
                    }
                    break;
                }
            }
            oxygen_scrollbar();
        }
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);

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
