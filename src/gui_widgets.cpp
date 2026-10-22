#include "gui_widgets.hpp"
#include "gui_theme.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <linux/input-event-codes.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <cctype>

static int* g_binding_target = nullptr;
static GLFWwindow* g_window = nullptr;
static ImFont* g_font_big = nullptr;

static const float LABEL_W   = 150.0f;
static const float OXY_R     = 4.0f;
static const float PANEL_R   = 6.0f;
static const float PANEL_PAD = 12.0f;   // inner padding of cards
static const float PANEL_GAP = 14.0f;   // outer margin between cards
static const float VALUE_W   = 64.0f;   // reserved readout slot right of sliders

static float s_panel_inner = 0.0f;
static float s_panel_x     = 0.0f;
static float s_panel_w     = 0.0f;
static ImGuiID s_panel_hid = 0;

void set_big_font(ImFont* font) { g_font_big = font; }

// ---------- low-level draw helpers ----------

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

static ImVec4 lerp4(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

// True optical vertical centering. ImGui stores Descent NEGATIVE, and RenderText
// places the baseline at pos.y + Ascent, so the ink box height is Ascent + |Descent|.
// Centering that box (not CalcTextSize's em box) gives pixel-true alignment.
static float vcenter_top(float y0, float y1, float font_size = 0.0f) {
    ImFont* font = ImGui::GetFont();
    float fs = font_size > 0.0f ? font_size : ImGui::GetFontSize();
    float scale = fs / font->FontSize;
    float ink = (font->Ascent + fabsf(font->Descent)) * scale;
    return y0 + ((y1 - y0) - ink) * 0.5f;
}

static float widget_anim(ImGuiID id, bool target) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float t = st->GetFloat(id, 0.0f);
    float goal = target ? 1.0f : 0.0f;
    float k = 1.0f - expf(-12.0f * ImGui::GetIO().DeltaTime);
    t += (goal - t) * k;
    if (t < 0.001f) t = 0.0f; else if (t > 0.999f) t = 1.0f;
    st->SetFloat(id, t);
    return t * t * (3.0f - 2.0f * t);
}

static void AddRoundedRectFilledGradV(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col_upr, ImU32 col_bot, float rounding) {
    if (b.x <= a.x || b.y <= a.y) return;
    if (rounding <= 0.0f) { dl->AddRectFilledMultiColor(a, b, col_upr, col_upr, col_bot, col_bot); return; }
    int start_vtx = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding);
    int end_vtx = dl->VtxBuffer.Size;
    ImDrawVert* vtx = dl->VtxBuffer.Data + start_vtx;
    for (int i = 0; i < end_vtx - start_vtx; i++) {
        float t = (vtx[i].pos.y - a.y) / (b.y - a.y);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        int r0 = (col_upr >> IM_COL32_R_SHIFT) & 0xFF, g0 = (col_upr >> IM_COL32_G_SHIFT) & 0xFF,
            b0 = (col_upr >> IM_COL32_B_SHIFT) & 0xFF, a0 = (col_upr >> IM_COL32_A_SHIFT) & 0xFF;
        int r1 = (col_bot >> IM_COL32_R_SHIFT) & 0xFF, g1 = (col_bot >> IM_COL32_G_SHIFT) & 0xFF,
            b1 = (col_bot >> IM_COL32_B_SHIFT) & 0xFF, a1 = (col_bot >> IM_COL32_A_SHIFT) & 0xFF;
        int va = (vtx[i].col >> IM_COL32_A_SHIFT) & 0xFF;
        int al = (a0 + (int)((a1 - a0) * t)) * va / 255;
        vtx[i].col = IM_COL32(r0 + (int)((r1 - r0) * t), g0 + (int)((g1 - g0) * t),
                              b0 + (int)((b1 - b0) * t), al);
    }
}

static void AddRoundedRectGradCenter(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 center_col, ImU32 edge_col, float rounding, int bands = 10, float edge_bias = 0.45f) {
    if (b.x <= a.x || b.y <= a.y) return;
    float w = b.x - a.x, h = b.y - a.y;
    float max_inset = (w < h ? w : h) * 0.5f;
    int cr = (center_col >> IM_COL32_R_SHIFT) & 0xFF, cg = (center_col >> IM_COL32_G_SHIFT) & 0xFF,
        cb = (center_col >> IM_COL32_B_SHIFT) & 0xFF, ca = (center_col >> IM_COL32_A_SHIFT) & 0xFF;
    int er = (edge_col >> IM_COL32_R_SHIFT) & 0xFF, eg = (edge_col >> IM_COL32_G_SHIFT) & 0xFF,
        eb = (edge_col >> IM_COL32_B_SHIFT) & 0xFF, ea = (edge_col >> IM_COL32_A_SHIFT) & 0xFF;
    for (int s = 0; s < bands; s++) {
        float f = (bands > 1) ? (float)s / (float)(bands - 1) : 1.0f;
        float inset = max_inset * f;
        float t = powf(f, edge_bias);
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

static void glass_gloss(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rr, float topY, float botY, int aTop) {
    const int N = 8;
    const float arcC = mn.y + rr;
    auto narrow = [&](float y) -> float {
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

static void strtoupper(char* dst, size_t n, const char* src) {
    size_t i = 0;
    for (; src[i] && i + 1 < n; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = 0;
}

// ---------- key binding ----------

const char* key_name(int code) {
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
        default: snprintf(buf, sizeof(buf), "Key %d", code); return buf;
    }
}

static int glfw_to_evdev(int glfw_key) {
    if (glfw_key >= GLFW_KEY_A && glfw_key <= GLFW_KEY_Z) return KEY_A + (glfw_key - GLFW_KEY_A);
    if (glfw_key >= GLFW_KEY_0 && glfw_key <= GLFW_KEY_9) return (glfw_key == GLFW_KEY_0) ? KEY_0 : KEY_1 + (glfw_key - GLFW_KEY_1);
    if (glfw_key >= GLFW_KEY_F1 && glfw_key <= GLFW_KEY_F12) return KEY_F1 + (glfw_key - GLFW_KEY_F1);
    switch (glfw_key) {
        case GLFW_KEY_ESCAPE: return KEY_ESC; case GLFW_KEY_TAB: return KEY_TAB;
        case GLFW_KEY_CAPS_LOCK: return KEY_CAPSLOCK; case GLFW_KEY_LEFT_SHIFT: return KEY_LEFTSHIFT;
        case GLFW_KEY_RIGHT_SHIFT: return KEY_RIGHTSHIFT; case GLFW_KEY_LEFT_CONTROL: return KEY_LEFTCTRL;
        case GLFW_KEY_RIGHT_CONTROL: return KEY_RIGHTCTRL; case GLFW_KEY_LEFT_ALT: return KEY_LEFTALT;
        case GLFW_KEY_RIGHT_ALT: return KEY_RIGHTALT; case GLFW_KEY_SPACE: return KEY_SPACE;
        case GLFW_KEY_ENTER: return KEY_ENTER; case GLFW_KEY_BACKSPACE: return KEY_BACKSPACE;
        case GLFW_KEY_DELETE: return KEY_DELETE; case GLFW_KEY_INSERT: return KEY_INSERT;
        case GLFW_KEY_HOME: return KEY_HOME; case GLFW_KEY_END: return KEY_END;
        case GLFW_KEY_PAGE_UP: return KEY_PAGEUP; case GLFW_KEY_PAGE_DOWN: return KEY_PAGEDOWN;
        case GLFW_KEY_KP_ENTER: return KEY_KPENTER; case GLFW_KEY_KP_DECIMAL: return KEY_KPDOT;
        case GLFW_KEY_KP_DIVIDE: return KEY_KPSLASH; case GLFW_KEY_KP_MULTIPLY: return KEY_KPASTERISK;
        case GLFW_KEY_KP_SUBTRACT: return KEY_KPMINUS; case GLFW_KEY_KP_ADD: return KEY_KPPLUS;
        case GLFW_KEY_KP_EQUAL: return KEY_KPEQUAL;
        case GLFW_KEY_KP_0: return KEY_KP0; case GLFW_KEY_KP_1: return KEY_KP1; case GLFW_KEY_KP_2: return KEY_KP2;
        case GLFW_KEY_KP_3: return KEY_KP3; case GLFW_KEY_KP_4: return KEY_KP4; case GLFW_KEY_KP_5: return KEY_KP5;
        case GLFW_KEY_KP_6: return KEY_KP6; case GLFW_KEY_KP_7: return KEY_KP7; case GLFW_KEY_KP_8: return KEY_KP8;
        case GLFW_KEY_KP_9: return KEY_KP9;
        default: return -1;
    }
}

static void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (!g_binding_target || action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) *g_binding_target = 0;
    else { int evdev = glfw_to_evdev(key); if (evdev >= 0) *g_binding_target = evdev; }
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
        default: if (button >= 6) evdev = BTN_FORWARD + (button - 6); break;
    }
    if (evdev >= 0) *g_binding_target = evdev;
    g_binding_target = nullptr;
}

void setup_key_callbacks(GLFWwindow* window) {
    g_window = window;
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
}

// ---------- scrollbar: fill stays gray, ONLY the border lights up ----------

void oxygen_scrollbar() {
    ImGuiStyle& st = ImGui::GetStyle();
    const float smax = ImGui::GetScrollMaxY();
    if (smax <= 0.0f) return;
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    const float sbw = st.ScrollbarSize;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), false);
    ImVec2 t0(wp.x + ws.x - sbw, wp.y);
    ImVec2 t1(wp.x + ws.x, wp.y + ws.y);
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
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) s_thumb_drag = false;
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && in_track) s_thumb_drag = true;
    bool lit = in_thumb || s_thumb_drag;
    float t = widget_anim(ImGui::GetID("##sb_thumb"), lit);

    // constant scheme-gray fill ...
    ImU32 center_c = OxCol(mulc(g_body_color, 2.60f));
    ImU32 edge_c   = OxCol(mulc(g_body_color, 2.20f));
    // ... only the rim eases to accent
    ImU32 rim_c    = lerp_u32(OxCol(mulc(g_body_color, 3.20f), 140), OxCol(OxLight(g_accent_color), 220), t);

    AddRoundedRectGradCenter(dl, g0, g1, center_c, edge_c, gr, 10);
    dl->AddRect(g0, g1, rim_c, gr, 0, 1.0f);
    dl->PopClipRect();
}

// ---------- buttons ----------

bool oxygen_button(const char* label, ImVec2 size_arg, ImU32 c1_override, ImU32 c2_override, bool force_colored) {
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
    ImU32 c1, c2, bc; int gloss;
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
    dl->AddText(ImVec2(mn.x + (sz.x - ls.x) * 0.5f, vcenter_top(mn.y, mx.y)),
                IM_COL32(238, 246, 255, 255), label, tend ? tend : nullptr);
    return clicked;
}

// ---------- checkbox ----------

bool oxygen_checkbox(const char* label, bool* v) {
    const float sz = ImGui::GetFrameHeight();
    const float R = OXY_R;
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
        // deliberately small, thin checkmark
        const float th = (sz * 0.09f < 1.4f) ? 1.4f : (sz * 0.09f);
        ImVec2 pts[3] = {
            ImVec2(mn.x + sz * 0.30f, mn.y + sz * 0.52f),
            ImVec2(mn.x + sz * 0.45f, mn.y + sz * 0.66f),
            ImVec2(mn.x + sz * 0.70f, mn.y + sz * 0.36f),
        };
        dl->AddPolyline(pts, 3, IM_COL32(245, 250, 255, (int)(255 * t_c)), 0, th);
    }
    if (ls.x > 0.0f)
        dl->AddText(ImVec2(bx1.x + ImGui::GetStyle().ItemInnerSpacing.x, vcenter_top(mn.y, mn.y + sz)),
                    ImGui::GetColorU32(ImGuiCol_Text), label, tend ? tend : nullptr);
    return clicked;
}

// ---------- sliders ----------

static bool slider_widget(const char* id, double* v, double lo, double hi, const char* fmt, float slider_w)
{
    ImGui::SetNextItemWidth(slider_w);
    bool changed = ImGui::SliderScalar(id, ImGuiDataType_Double, v, &lo, &hi, "");  // no text inside the track
    ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float FR = ImGui::GetStyle().FrameRounding;
    glass_gloss(dl, r0, r1, FR, r0.y + 1.0f, r0.y + (r1.y - r0.y) * 0.55f, 36);
    float grab_w = ImGui::GetStyle().GrabMinSize;
    float t = (float)((*v - lo) / (hi - lo)); if (t < 0) t = 0; if (t > 1) t = 1;
    const float pad = 2.0f;
    float gx = r0.x + pad + ((r1.x - r0.x) - grab_w - pad * 2.0f) * t;
    ImVec2 g0(gx, r0.y + 3), g1(gx + grab_w, r1.y - 3);
    bool hov = ImGui::IsItemHovered(); bool act = ImGui::IsItemActive();
    float ta = widget_anim(ImGui::GetID(id), hov || act);
    // thumb: constant gray fill, ONLY the rim eases to accent (same language as scrollbar)
    ImU32 center_c = OxCol(mulc(g_body_color, 3.00f));
    ImU32 edge_c   = OxCol(mulc(g_body_color, 2.40f));
    ImU32 rim_c    = lerp_u32(OxCol(mulc(g_body_color, 3.60f), 140), OxCol(OxLight(g_accent_color), 220), ta);
    float gr = (g1.x - g0.x) * 0.5f; if (gr > OXY_R) gr = OXY_R;
    AddRoundedRectGradCenter(dl, g0, g1, center_c, edge_c, gr, 8);
    dl->AddRect(g0, g1, rim_c, gr, 0, 1.0f);

    // value badge attached to the track's right end (never floats at the panel edge)
    char buf[64]; snprintf(buf, sizeof(buf), fmt, *v);
    ImVec2 ts = ImGui::CalcTextSize(buf, nullptr, true);
    ImVec2 b0(r1.x + 8.0f, r0.y + 2.0f), b1(r1.x + 8.0f + ts.x + 14.0f, r1.y - 2.0f);
    dl->AddRectFilled(b0, b1, OxCol(mulc(g_body_color, 0.85f), 235), 3.0f);
    dl->AddRect(b0, b1, OxCol(mulc(g_body_color, 2.40f), 110), 3.0f);
    dl->AddText(ImVec2(b0.x + 7.0f, vcenter_top(b0.y, b1.y)), OxCol(g_text_color, 210), buf, nullptr);
    return changed;
}

bool row_slider(const char* label, double* v, double lo, double hi, const char* fmt) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LABEL_W);
    float w = ImGui::GetContentRegionAvail().x;
    if (s_panel_inner > 0.0f) w = s_panel_inner - LABEL_W;
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return slider_widget(id, v, lo, hi, fmt, w - VALUE_W - ImGui::GetStyle().ItemInnerSpacing.x);
}

bool stacked_slider(const char* label, double* v, double lo, double hi, const char* fmt, float w) {
    ImGui::TextDisabled("%s", label);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return slider_widget(id, v, lo, hi, fmt, w - VALUE_W - ImGui::GetStyle().ItemInnerSpacing.x);
}

// ---------- combos ----------

bool combo_framed(const char* id, int* idx, const char* items) {
    int count = 0;
    for (const char* p = items; *p; p += strlen(p) + 1) count++;
    const char* preview = items;
    for (int i = 0; i < *idx && *preview; i++) preview += strlen(preview) + 1;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));   // even margins around popup items
    const float FR = ImGui::GetStyle().FrameRounding;
    const float arrow_sz = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImVec2 f0 = ImGui::GetCursorScreenPos();
    const float fw = ImGui::CalcItemWidth();
    const float fhgt = ImGui::GetFrameHeight();
    ImVec2 f1(f0.x + fw, f0.y + fhgt);
    ImGuiID wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;
    bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton);
    ImGui::PopStyleVar();
    bool fhov = ImGui::IsMouseHoveringRect(f0, f1);
    float t_h = widget_anim(wid, fhov);
    float t_o = widget_anim(wid + 1, open);
    ImU32 c1 = lerp_u32(OxCol(mulc(g_body_color, 2.20f)), OxCol(g_accent_color), t_o);
    ImU32 c2 = lerp_u32(OxCol(mulc(g_body_color, 1.45f)), OxCol(OxDark(g_accent_color)), t_o);
    ImU32 bc = lerp_u32(OxCol(mulc(g_body_color, 3.60f), 200), OxCol(OxLight(g_accent_color), 220), t_h > t_o ? t_h : t_o);
    OxFrame(dl, f0, f1, FR, c1, c2, bc, 60 + (int)(80 * t_o), 60);
    dl->AddText(ImVec2(f0.x + ImGui::GetStyle().FramePadding.x, vcenter_top(f0.y, f1.y)),
                lerp_u32(OxCol(g_text_color, 205), IM_COL32(238, 246, 255, 255), t_o), preview, nullptr);
    float ax = f1.x - arrow_sz * 0.5f, ay = (f0.y + f1.y) * 0.5f;
    if (open) {
        float as = 6.0f;   // same size as the closed arrow
        dl->AddTriangleFilled(ImVec2(ax - as, ay + as * 0.5f), ImVec2(ax + as, ay + as * 0.5f), ImVec2(ax, ay - as * 0.5f), IM_COL32(255, 255, 255, 220));
    } else {
        float as = 6.0f;
        dl->AddTriangleFilled(ImVec2(ax - as, ay - as * 0.5f), ImVec2(ax + as, ay - as * 0.5f), ImVec2(ax, ay + as * 0.5f), IM_COL32(255, 255, 255, 220));
    }
    if (open) {
        ImGui::PushStyleColor(ImGuiCol_Border, mulc(g_body_color, 4.20f, 0.75f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImDrawList* pdl = ImGui::GetWindowDrawList();
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
            if (o_s > 0.01f) {
                pdl->AddRectFilled(p0, r1, OxCol(g_accent_color, (int)(70 * o_s)), OXY_R);
                glass_gloss(pdl, p0, r1, OXY_R, p0.y + 1.0f, p0.y + (r1.y - p0.y) * 0.55f, (int)(26 * o_s));
            }
            if (down) pdl->AddRectFilled(p0, r1, OxCol(g_accent_active, 60), OXY_R);
            float b_t = o_h > o_s ? o_h : o_s;
            if (b_t > 0.01f)
                pdl->AddRect(ImVec2(p0.x + 0.5f, p0.y + 0.5f), ImVec2(r1.x - 0.5f, r1.y - 0.5f),
                             lerp_u32(IM_COL32(0, 0, 0, 0), OxCol(OxLight(g_accent_color), 200), b_t), OXY_R);
            ImU32 tcol = (i == *idx) ? IM_COL32(240, 246, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text);
            pdl->AddText(ImVec2(p0.x + ImGui::GetStyle().FramePadding.x, vcenter_top(p0.y, p0.y + h)), tcol, p, nullptr);
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

bool row_combo(const char* label, int* idx, const char* items) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0.0f, 10.0f);          // tight: combo sits right after its label
    ImGui::SetNextItemWidth(-FLT_MIN);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return combo_framed(id, idx, items);
}

bool stacked_combo(const char* label, int* idx, const char* items, float w) {
    ImGui::TextDisabled("%s", label);
    ImGui::SetNextItemWidth(w);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return combo_framed(id, idx, items);
}

bool row_check(const char* label, bool* v) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0.0f, 10.0f);          // tight: checkbox sits right after its label
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return oxygen_checkbox(id, v);
}

// ---------- keybind rows ----------

static void keybind_label_buf(char* buf, size_t n, const char* label, int& key_code) {
    if (g_binding_target == &key_code) snprintf(buf, n, "Press a key...##%s", label);
    else if (key_code == 0)            snprintf(buf, n, "Click to bind##%s", label);
    else                               snprintf(buf, n, "%s##%s", key_name(key_code), label);
}

void keybind_row(const char* label, int& key_code) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LABEL_W);
    char buf[64]; keybind_label_buf(buf, sizeof(buf), label, key_code);
    if (oxygen_button(buf, ImVec2(140, 0))) g_binding_target = &key_code;
}

void stacked_keybind(const char* label, int& key_code, float w) {
    ImGui::TextDisabled("%s", label);
    char buf[64]; keybind_label_buf(buf, sizeof(buf), label, key_code);
    if (oxygen_button(buf, ImVec2(w, 0))) g_binding_target = &key_code;
}

// ---------- flat panels (cards) ----------

float panel_inner() { return s_panel_inner; }

void begin_panel(const char* label, float width, int status) {
    ImGui::PushID(label);
    s_panel_hid = ImGui::GetID("##panelh");
    ImGuiStorage* st = ImGui::GetStateStorage();
    float h = st->GetFloat(s_panel_hid, 64.0f);

    ImVec2 start = ImGui::GetCursorScreenPos();
    float panel_w = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;
    s_panel_x = start.x;
    s_panel_w = panel_w;
    ImVec2 p1(start.x + panel_w, start.y + h);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(start, p1, OxCol(mulc(g_body_color, 1.16f), 235), PANEL_R);
    dl->AddRect(start, p1, OxCol(mulc(g_body_color, 2.40f), 110), PANEL_R, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(start.x + PANEL_PAD, start.y + 12.0f));
    s_panel_inner = panel_w - PANEL_PAD * 2.0f;

    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Text, OxCol(lerp4(g_text_color, g_accent_color, 0.45f), 200));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 4.0f));
    char up[128]; strtoupper(up, sizeof(up), label);
    ImGui::TextUnformatted(up);
    ImGui::PopStyleColor();

    if (status >= 0) {
        ImVec2 hmn = ImGui::GetItemRectMin(), hmx = ImGui::GetItemRectMax();
        float r = 5.0f;
        ImVec2 c(start.x + panel_w - PANEL_PAD - r, (hmn.y + hmx.y) * 0.5f);
        if (status) {
            dl->AddCircleFilled(c, r + 3.5f, IM_COL32(46, 148, 84, 40));   // soft outer glow
            dl->AddCircleFilled(c, r + 1.5f, IM_COL32(70, 190, 110, 70));
            dl->AddCircleFilled(c, r,        IM_COL32(52, 168, 96, 255));
            dl->AddCircle(c, r, IM_COL32(150, 230, 180, 200), 0, 1.0f);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.35f), r * 0.45f, IM_COL32(255, 255, 255, 90));
        } else {
            dl->AddCircleFilled(c, r, IM_COL32(255, 255, 255, 26));
            dl->AddCircle(c, r, IM_COL32(255, 255, 255, 70), 0, 1.0f);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.35f), r * 0.45f, IM_COL32(255, 255, 255, 24));
        }
    }

    ImGui::Dummy(ImVec2(s_panel_inner, 0.0f));
    ImGui::PopStyleVar();
}

void end_panel() {
    ImGui::EndGroup();
    ImVec2 cmn = ImGui::GetItemRectMin(), cmx = ImGui::GetItemRectMax();

    float h = (cmx.y + 12.0f) - (cmn.y - 12.0f);
    ImGui::GetStateStorage()->SetFloat(s_panel_hid, h);   // cache for next frame

    s_panel_inner = 0.0f;
    ImGui::SetCursorScreenPos(ImVec2(s_panel_x, cmn.y - 12.0f + h + PANEL_GAP));
    ImGui::PopID();
}

// ---------- list items (styled like combo dropdown options) ----------

bool oxygen_list_item(const char* id, const char* label, bool selected) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetFrameHeight();
    ImVec2 r1(p0.x + w, p0.y + h);
    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    ImGuiID oid = ImGui::GetID(id);
    float o_h = widget_anim(oid, hov);
    float o_s = widget_anim(oid + 1, selected);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (o_s > 0.01f) {
        dl->AddRectFilled(p0, r1, OxCol(g_accent_color, (int)(70 * o_s)), OXY_R);
        glass_gloss(dl, p0, r1, OXY_R, p0.y + 1.0f, p0.y + (r1.y - p0.y) * 0.55f, (int)(26 * o_s));
    }
    if (hov && !selected) dl->AddRectFilled(p0, r1, OxCol(g_accent_color, (int)(38 * o_h)), OXY_R);
    float b_t = o_h > o_s ? o_h : o_s;
    if (b_t > 0.01f)
        dl->AddRect(ImVec2(p0.x + 0.5f, p0.y + 0.5f), ImVec2(r1.x - 0.5f, r1.y - 0.5f),
                    lerp_u32(IM_COL32(0, 0, 0, 0), OxCol(OxLight(g_accent_color), 200), b_t), OXY_R);
    ImU32 tcol = selected ? IM_COL32(240, 246, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddText(ImVec2(p0.x + ImGui::GetStyle().FramePadding.x, vcenter_top(p0.y, r1.y)), tcol, label, nullptr);
    return clicked;
}

// ---------- status ----------

void click_indicator(const char* label, bool on) {
    char display[32];
    snprintf(display, sizeof(display), "%s: %s", label, on ? "ON" : "off");
    ImVec2 pad(12, 5);
    ImVec2 ls = ImGui::CalcTextSize(display);
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
    if (on) glass_gloss(dl, mn, mx, r, mn.y + 1.0f, mn.y + sz.y * 0.55f, 70);
    dl->AddRect(mn, mx, on ? rim_on : rim_off, r, 0, 1.0f);
    dl->AddText(ImVec2(mn.x + pad.x, vcenter_top(mn.y, mx.y)), on ? text_on : text_off, display);
    ImGui::Dummy(sz);
}

void stat_readout(bool on, float cps, uint32_t clicks, float inner_w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = g_font_big ? g_font_big : ImGui::GetFont();   // real 30px atlas font = crisp
    float big = font->FontSize;
    ImVec2 mn = ImGui::GetCursorScreenPos();
    char main_txt[32];
    if (on) snprintf(main_txt, sizeof(main_txt), "%.1f CPS", cps);
    else    snprintf(main_txt, sizeof(main_txt), "Idle");
    char sub_txt[64]; snprintf(sub_txt, sizeof(sub_txt), "%u clicks", clicks);
    ImU32 main_col = on ? OxCol(OxLight(g_accent_color)) : OxCol(g_text_color, 90);
    dl->AddText(font, big, mn, main_col, main_txt);
    ImVec2 ms = font->CalcTextSizeA(big, FLT_MAX, 0.0f, main_txt, nullptr, nullptr);
    ImVec2 sp(mn.x, mn.y + ms.y + 4.0f);
    dl->AddText(sp, OxCol(g_text_color, 110), sub_txt);
    ImVec2 ss = ImGui::CalcTextSize(sub_txt);
    ImGui::Dummy(ImVec2(inner_w, ms.y + 4.0f + ss.y));
}

void segmented_tabs(const char* const* names, int count, int& current_tab) {
    const float R = OXY_R;
    const float PR = R - 1.0f;
    const float pad_x = 14.0f;
    const float h = ImGui::GetFrameHeight();
    const ImU32 track = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 rim = ImGui::GetColorU32(ImGuiCol_Separator);
    const ImU32 txtSel = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 txt = ImGui::GetColorU32(ImGuiCol_Text, 0.80f);
    ImVec2 ls[8]; float w[8], total = 0.0f;
    for (int i = 0; i < count && i < 8; i++) {
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
        bool sel = (current_tab == i);
        ImVec2 s0(x, mn.y), s1(x + w[i], mn.y + h);
        bool clicked = ImGui::InvisibleButton(names[i], ImVec2(w[i], h));
        bool hov = ImGui::IsItemHovered();
        ImGuiID wid = ImGui::GetID(names[i]);
        float t_h = widget_anim(wid, hov);
        float t_s = widget_anim(wid + 1, sel);
        ImVec2 i0(s0.x + 2, s0.y + 2), i1(s1.x - 2, s1.y - 2);
        if (t_s > 0.01f) {
            dl->AddRectFilled(i0, i1, lerp_u32(IM_COL32(0, 0, 0, 0), OxCol(g_accent_active), t_s), PR);
            glass_gloss(dl, i0, i1, PR, i0.y + 1.0f, i0.y + (i1.y - i0.y) * 0.58f, (int)(80 * t_s));
            dl->AddRect(i0, i1, lerp_u32(IM_COL32(0, 0, 0, 0), OxCol(OxLight(g_accent_color), 200), t_s), PR, 0, 1.0f);
        } else if (t_h > 0.01f) {
            dl->AddRectFilled(i0, i1, IM_COL32(g_accent_color.x * 255, g_accent_color.y * 255, g_accent_color.z * 255, (int)(64 * t_h)), PR);
        }
        if (i > 0 && !sel && current_tab != i - 1)
            dl->AddLine(ImVec2(s0.x, s0.y + 5), ImVec2(s0.x, s1.y - 5), rim, 1.0f);
        dl->AddText(ImVec2(s0.x + (w[i] - ls[i].x) * 0.5f, vcenter_top(s0.y, s1.y)),
                    sel ? txtSel : txt, names[i]);
        if (clicked) current_tab = i;
        x += w[i];
    }
    ImGui::SetCursorScreenPos(ImVec2(mn.x, mx.y));
    ImGui::Spacing();
}
