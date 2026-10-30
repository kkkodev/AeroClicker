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
static ImVec2 g_binding_rect_min(0, 0), g_binding_rect_max(0, 0);
static double g_bind_block_until = 0.0;
static ImFont* g_font_big = nullptr;

// Used to pause synthesized clicks while the cursor is over AeroClicker's own window: a
// synthetic press/release stream landing on our UI arrives faster than the user's own press,
// so ImGui keeps seeing the button already held and swallows every real click.
// GLFW's enter/leave callback is used instead of io.MousePos because the latter stops
// updating once the pointer leaves the window, which would leave the clicker paused forever.
static bool g_cursor_in_window = false;

static void cursor_enter_callback(GLFWwindow*, int entered) {
    g_cursor_in_window = (entered != 0);
}

// Panel layout constants
static const float OXY_R     = 4.0f;
static const float PANEL_R   = 6.0f;
static const float PANEL_PAD = 12.0f;
static const float PANEL_GAP = 14.0f;

// Alignment constants for row widgets
static const float LABEL_COL_W = 118.0f;
static const float COL_GAP     = 8.0f;
static const float VALUE_W     = 52.0f;

// State variables
static float s_panel_inner = 0.0f;
static float s_panel_x     = 0.0f;
static float s_panel_w     = 0.0f;
static ImGuiID s_panel_hid = 0;

void set_big_font(ImFont* font) { g_font_big = font; }

// Control column helpers for row layout
static float ctrl_x0()       { return s_panel_x + PANEL_PAD + LABEL_COL_W + COL_GAP; }
static float ctrl_w_full()   { return s_panel_inner - (LABEL_COL_W + COL_GAP); }
static float ctrl_w_slider() { return ctrl_w_full() - VALUE_W - ImGui::GetStyle().ItemSpacing.x; }

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

// ---------- oxygen theme-aware helpers ----------

static const float OX_INK_HI = 0.70f;
static const float OX_INK_LO = 0.25f;

static float ox_scale(){ ImVec2 s = ImGui::GetIO().DisplayFramebufferScale; return s.x > 0.0f ? s.x : 1.0f; }

static float fb_scale() {
    float s = ImGui::GetIO().DisplayFramebufferScale.x;
    return s > 0.0f ? s : 1.0f;
}

// Snap bounds to the physical pixel grid (outwards).
// ImGui 1.93 draws AddRect() strokes *inside* the given rect by default, so borders must
// be handed pixel-aligned OUTER bounds. (Older code added a half-pixel inset, which is
// only correct for the pre-1.93 centered stroke — with the new inside stroke it puts the
// line on a half pixel and smears it over two pixels: the "blurry 2px border".)
static float crisp_lo(float v) { float s = fb_scale(); return floorf(v * s) / s; }
static float crisp_hi(float v) { float s = fb_scale(); return ceilf(v * s) / s; }
static ImVec2 crisp_min(ImVec2 p) { return ImVec2(crisp_lo(p.x), crisp_lo(p.y)); }
static ImVec2 crisp_max(ImVec2 p) { return ImVec2(crisp_hi(p.x), crisp_hi(p.y)); }
static ImU32 ox_fill_top(){ return OxCol(ox_is_light() ? mulc(g_body_color,1.25f) : mulc(g_body_color,2.20f)); }
static ImU32 ox_fill_bot(){ return OxCol(ox_is_light() ? mulc(g_body_color,0.92f) : mulc(g_body_color,1.45f)); }
static ImU32 ox_rim_neutral(int a){
    return OxCol(lerp4(g_body_color, g_text_color, ox_is_light() ? 0.45f : 0.40f), a);
}
static ImU32 ox_accent_rim(){ return ox_is_light() ? OxCol(OxDark(g_accent_color,0.65f),220)
                                                   : OxCol(OxLight(g_accent_color),220); }
// Light-theme surface shades. mulc() cannot describe a light theme: the body colour is
// already ~0.84, so *2.2 / *3.6 clamp to white (flat white fields, invisible borders) while
// *0.8 becomes a heavy grey slab. Blend toward white/black instead so the shades keep their
// intended step. The dark-theme branches are byte-for-byte the old values.
static ImU32 ox_surf_top() { return ox_is_light() ? OxCol(ox_tint(g_body_color, 0.90f)) : OxCol(mulc(g_body_color, 2.20f)); }
static ImU32 ox_surf_bot() { return ox_is_light() ? OxCol(ox_tint(g_body_color, 0.34f)) : OxCol(mulc(g_body_color, 1.45f)); }
static ImU32 ox_rim_field(int a) { return ox_is_light() ? OxCol(lerp4(g_body_color, g_text_color, 0.40f), a)
                                                        : OxCol(mulc(g_body_color, 3.60f), a); }
// Secondary / disabled text: a fixed leftover alpha reads as ~nothing on a light background.
static ImU32 ox_text_dim() { return ox_is_light() ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : OxCol(g_text_color, 110); }
static ImU32 ox_accent_text() { return ox_is_light() ? OxCol(mulc(g_accent_color, 0.72f)) : OxCol(OxLight(g_accent_color)); }
static ImU32 ox_ink_luma(float l){
    if (ox_is_light()) return l < OX_INK_LO ? IM_COL32(240,246,255,255) : IM_COL32(20,24,20,255);
    return l > OX_INK_HI ? IM_COL32(20,24,20,255) : IM_COL32(240,246,255,255);
}
static ImU32 ox_ink_fill(ImVec4 c, float extra_luma){ return ox_ink_luma(ox_luma(c) + extra_luma); }
static ImU32 ox_ink_on(ImU32 fill){
    int r=(fill>>IM_COL32_R_SHIFT)&255, g=(fill>>IM_COL32_G_SHIFT)&255, b=(fill>>IM_COL32_B_SHIFT)&255;
    return ox_ink_luma((0.2126f*r+0.7152f*g+0.0722f*b)/255.0f);
}
static void ox_outline(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float R, ImU32 col){
    float s = ox_scale();
    ImVec2 a(crisp_lo(mn.x), crisp_lo(mn.y));
    ImVec2 b(crisp_hi(mx.x), crisp_hi(mx.y));
    if (b.x - a.x <= 0.0f || b.y - a.y <= 0.0f) return;
    // 1 physical pixel, stroke inside the bounds: renders as exactly one crisp pixel row/column.
    dl->AddRect(a, b, col, R, 1.0f / s, ImDrawFlags_StrokeInside);
}

// True optical vertical centering. ImGui stores Descent NEGATIVE, and RenderText
// places the baseline at pos.y + Ascent, so the ink box height is Ascent + |Descent|.
// Centering that box (not CalcTextSize's em box) gives pixel-true alignment.
static float vcenter_top(float y0, float y1, float font_size = 0.0f) {
    ImFont* font = ImGui::GetFont();
    float fs = font_size > 0.0f ? font_size : ImGui::GetFontSize();
    ImFontBaked* baked = font->GetFontBaked(fs);
    float scale = fs / baked->Size;
    float ink = (baked->Ascent + fabsf(baked->Descent)) * scale;
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

// Four-corner variant: the fill can run horizontally as well as vertically, which is how the
// slot chips show which side (left or right clicker) owns them.
static void AddRoundedRectFilledGrad4(ImDrawList* dl, ImVec2 a, ImVec2 b,
                                      ImU32 c_tl, ImU32 c_tr, ImU32 c_bl, ImU32 c_br,
                                      float rounding) {
    if (b.x <= a.x || b.y <= a.y) return;
    if (rounding <= 0.0f) { dl->AddRectFilledMultiColor(a, b, c_tl, c_tr, c_br, c_bl); return; }
    int start_vtx = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding);
    int end_vtx = dl->VtxBuffer.Size;
    ImDrawVert* vtx = dl->VtxBuffer.Data + start_vtx;
    for (int i = 0; i < end_vtx - start_vtx; i++) {
        float tx = (vtx[i].pos.x - a.x) / (b.x - a.x);
        float ty = (vtx[i].pos.y - a.y) / (b.y - a.y);
        if (tx < 0.0f) tx = 0.0f; else if (tx > 1.0f) tx = 1.0f;
        if (ty < 0.0f) ty = 0.0f; else if (ty > 1.0f) ty = 1.0f;
        const ImU32 col = lerp_u32(lerp_u32(c_tl, c_tr, tx), lerp_u32(c_bl, c_br, tx), ty);
        int va = (vtx[i].col >> IM_COL32_A_SHIFT) & 0xFF;
        int al = ((col >> IM_COL32_A_SHIFT) & 0xFF) * va / 255;
        vtx[i].col = (col & ~IM_COL32_A_MASK) | ((ImU32)al << IM_COL32_A_SHIFT);
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

static void OxFrameGrad4(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float R,
                         ImU32 c_tl, ImU32 c_tr, ImU32 c_bl, ImU32 c_br,
                         ImU32 border_col, int gloss = 100, int shadow = 60) {
    ImVec2 b0(crisp_lo(mn.x), crisp_lo(mn.y));
    ImVec2 b1(crisp_hi(mx.x), crisp_hi(mx.y));
    if (b1.x - b0.x < 2.0f) b1.x = b0.x + 2.0f;
    if (b1.y - b0.y < 2.0f) b1.y = b0.y + 2.0f;
    AddRoundedRectFilledGrad4(dl, b0, b1, c_tl, c_tr, c_bl, c_br, R);
    ImVec2 f0(b0.x + 1.0f, b0.y + 1.0f), f1(b1.x - 1.0f, b1.y - 1.0f);
    float fr = R > 1.0f ? R - 1.0f : 0.0f;
    if (gloss > 0)
        AddRoundedRectFilledGradV(dl, f0, ImVec2(f1.x, f0.y + (f1.y - f0.y) * 0.45f),
                                  IM_COL32(255, 255, 255, gloss), IM_COL32(255, 255, 255, 0), fr);
    if (shadow > 0)
        AddRoundedRectFilledGradV(dl, ImVec2(f0.x, f0.y + (f1.y - f0.y) * 0.55f), f1,
                                  IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, shadow), fr);
    ox_outline(dl, b0, b1, R, border_col);
}

static void OxFrame(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float R,
                    ImU32 c_top, ImU32 c_bot, ImU32 border_col,
                    int gloss = 100, int shadow = 60) {
    OxFrameGrad4(dl, mn, mx, R, c_top, c_top, c_bot, c_bot, border_col, gloss, shadow);
}

static void glass_gloss(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rr, float botY, int aTop) {
    if (mx.x <= mn.x || botY <= mn.y || aTop <= 0) return;
    // A single rounded vertical gradient instead of stacked horizontal bands. The old bands
    // inset themselves by the *widest* corner curvature inside each band, so the first band
    // ate 2-3px off both sides and never covered the area under the corner arc: that dark
    // sliver along the upper edge/corners of every glass field is the "black top corners".
    // Shape-matching the top corners exactly and fading alpha to 0 before the (invisible)
    // bottom corners of this temporary rect removes it.
    AddRoundedRectFilledGradV(dl, mn, ImVec2(mx.x, botY),
                              IM_COL32(255, 255, 255, aTop), IM_COL32(255, 255, 255, 0),
                              rr > 0.0f ? rr : 0.0f);
}

// Soft accent halo used by the tab hover. A blurred sprite is not available on an ImDrawList,
// so stack slightly shrinking rounded rects with alpha rising inward: the tint builds up in
// the middle and fades to almost nothing at the edges (an Oxygen-style glow), instead of the
// flat translucent rectangle that reads as a pressed button.
static void accent_glow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float amount) {
    if (amount <= 0.01f || b.x <= a.x || b.y <= a.y) return;
    const ImU32 acc = OxCol(g_accent_color);
    const float h = b.y - a.y;
    // Alpha starts at zero on the outermost ring and rises as the rings shrink inward, so the
    // halo dissolves into the tab track instead of ending on a visible rectangle edge.
    // The first rings sit slightly outside the tab, which makes the bloom read larger without
    // touching the neighbours (the caller keeps 2px of inset inside the track).
    const float outset = 2.0f;
    const float feather = h * 0.34f;
    const int steps = 20;
    const float peak = ox_is_light() ? 12.0f : 13.0f;
    for (int s = 0; s < steps; s++) {
        const float f = (float)s / (float)(steps - 1);
        const float inset = -outset + f * (feather + outset);
        const int alpha = (int)(peak * powf(f, 1.35f) * amount);
        if (alpha <= 0) continue;
        float r = rounding - inset * 0.5f;
        if (r < 1.0f) r = 1.0f;
        dl->AddRectFilled(ImVec2(a.x + inset, a.y + inset), ImVec2(b.x - inset, b.y - inset),
                          (acc & 0x00FFFFFFu) | ((ImU32)alpha << IM_COL32_A_SHIFT), r);
    }
}

static void ox_sunken(ImDrawList* dl, ImVec2 r0, ImVec2 r1, float R, float anim) {
    ImVec2 b0(crisp_lo(r0.x), crisp_lo(r0.y));
    ImVec2 b1(crisp_hi(r1.x), crisp_hi(r1.y));
    // Light theme: a light recessed field (dark at the top edge, bottom catching the light)
    // instead of a 0.65 grey slab, so slider tracks / text boxes / value boxes read as the
    // same control family as the combos. Dark theme keeps its old values.
    AddRoundedRectFilledGradV(dl, b0, b1,
        ox_is_light() ? OxCol(ox_tint(g_body_color, -0.07f)) : OxCol(mulc(g_body_color, 0.55f)),
        ox_is_light() ? OxCol(ox_tint(g_body_color, 0.40f)) : OxCol(mulc(g_body_color, 1.05f)), R);
    if (R > 1.0f)
        AddRoundedRectFilledGradV(dl, ImVec2(b0.x + 1, b0.y + 1), ImVec2(b1.x - 1, b0.y + (b1.y - b0.y) * 0.55f),
                                  IM_COL32(0, 0, 0, ox_is_light() ? 18 : 64), IM_COL32(0, 0, 0, 0), R - 1.0f);
    ox_outline(dl, b0, b1, R,
               lerp_u32(ox_is_light() ? OxCol(lerp4(g_body_color, g_text_color, 0.40f), 210)
                                      : ox_rim_neutral(160),
                        ox_accent_rim(), anim));
}

static void push_transparent_frame() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0, 0, 0, 0));
}
static void pop_transparent_frame() { ImGui::PopStyleColor(3); ImGui::PopStyleVar(); }

bool ox_input_text(const char* id, char* buf, int buf_n, const char* hint, float w) {
    ImVec2 f0 = ImGui::GetCursorScreenPos();
    ImVec2 f1(f0.x + w, f0.y + ImGui::GetFrameHeight());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float R = ImGui::GetStyle().FrameRounding;
    ImGuiID wid = ImGui::GetID(id);
    float th = widget_anim(wid, ImGui::IsMouseHoveringRect(f0, f1));
    ox_sunken(dl, f0, f1, R, th * 0.6f);
    push_transparent_frame();
    ImGui::SetNextItemWidth(w);
    bool res = hint ? ImGui::InputTextWithHint(id, hint, buf, buf_n) : ImGui::InputText(id, buf, buf_n);
    // Active (focused) state: same accent rim as hover. Drawn after the field so the
    // 1px rim lands on top of the widget's own pixels; it never covers the text.
    float ta = widget_anim(wid + 1, ImGui::IsItemActive());
    if (ta > 0.01f)
        ox_outline(dl, f0, f1, R, lerp_u32(ox_rim_neutral(160), ox_accent_rim(), ta));
    pop_transparent_frame();
    return res;
}

bool ox_input_double(const char* id, double* v, double step, double step_fast, const char* fmt, float w) {
    ImVec2 f0 = ImGui::GetCursorScreenPos();
    ImVec2 f1(f0.x + w, f0.y + ImGui::GetFrameHeight());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float R = ImGui::GetStyle().FrameRounding;
    ImGuiID wid = ImGui::GetID(id);
    float th = widget_anim(wid, ImGui::IsMouseHoveringRect(f0, f1));
    ox_sunken(dl, f0, f1, R, th * 0.6f);
    push_transparent_frame();
    ImGui::SetNextItemWidth(w);
    bool res = ImGui::InputDouble(id, v, step, step_fast, fmt);
    float ta = widget_anim(wid + 1, ImGui::IsItemActive());
    if (ta > 0.01f)
        ox_outline(dl, f0, f1, R, lerp_u32(ox_rim_neutral(160), ox_accent_rim(), ta));
    pop_transparent_frame();
    return res;
}

bool ox_input_int(const char* id, int* v, int step, int step_fast, float w) {
    ImVec2 f0 = ImGui::GetCursorScreenPos();
    ImVec2 f1(f0.x + w, f0.y + ImGui::GetFrameHeight());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float R = ImGui::GetStyle().FrameRounding;
    ImGuiID wid = ImGui::GetID(id);
    float th = widget_anim(wid, ImGui::IsMouseHoveringRect(f0, f1));
    ox_sunken(dl, f0, f1, R, th * 0.6f);
    push_transparent_frame();
    ImGui::SetNextItemWidth(w);
    bool res = ImGui::InputInt(id, v, step, step_fast);
    float ta = widget_anim(wid + 1, ImGui::IsItemActive());
    if (ta > 0.01f)
        ox_outline(dl, f0, f1, R, lerp_u32(ox_rim_neutral(160), ox_accent_rim(), ta));
    pop_transparent_frame();
    return res;
}

void ox_plot_lines(const char* id, const float* values, int count, int offset,
                   const char* overlay, float y0, float y1, ImVec2 size) {
    ImVec2 f0 = ImGui::GetCursorScreenPos();
    ImVec2 f1(f0.x + size.x, f0.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ox_sunken(dl, f0, f1, ImGui::GetStyle().FrameRounding, 0.0f);

    // Own item + own trace drawing: ImGui's PlotLines() opens a stock tooltip window, which
    // inherits PopupBorderSize 0 and the flat PopupBg (no border, no gradient). Drawing the
    // trace here lets the hover readout use the same framed surface as every other field.
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiStyle& st = ImGui::GetStyle();
    const ImVec2 in0(f0.x + st.FramePadding.x, f0.y + st.FramePadding.y);
    const ImVec2 in1(f1.x - st.FramePadding.x, f1.y - st.FramePadding.y);

    const int segments = count - 1;
    int hovered_idx = -1;
    if (count >= 2 && in1.x > in0.x && in1.y > in0.y) {
        const ImVec2 mp = ImGui::GetIO().MousePos;
        if (hovered && mp.x >= in0.x && mp.x <= in1.x && mp.y >= in0.y && mp.y <= in1.y) {
            const float t = (mp.x - in0.x) / (in1.x - in0.x);
            hovered_idx = (int)(t * (float)segments);
            if (hovered_idx < 0) hovered_idx = 0;
            if (hovered_idx > segments - 1) hovered_idx = segments - 1;
        }
        const float inv = (y0 == y1) ? 0.0f : 1.0f / (y1 - y0);
        auto sample = [&](int i) { return values[(i + offset) % count]; };
        auto at = [&](int i, float v) {
            float t = (v - y0) * inv; if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
            return ImVec2(in0.x + (in1.x - in0.x) * ((float)i / (float)segments),
                          in1.y - (in1.y - in0.y) * t);
        };
        const ImU32 base = ImGui::GetColorU32(ImGuiCol_PlotLines);
        const ImU32 hi   = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
        // ImGui's AA polyline: a 1px trace that follows fractional Y positions smoothly
        // instead of snapping to whole rows. Non-AA AddLine() per sample is a hard staircase
        // (the samples are only ~2.5px apart, so every slope reads as pixel-by-pixel steps).
        //
        // AA is wrong for horizontal runs though: a 1px line at a fractional Y is feathered
        // over two rows, so an idle/zero trace turned into a 2px blurry band at the baseline.
        // Flat runs are therefore snapped to a single physical row; sloped runs keep the AA
        // polyline.
        ImVec2 pts[256];
        int n = segments + 1;
        if (n > (int)(sizeof(pts) / sizeof(pts[0]))) n = (int)(sizeof(pts) / sizeof(pts[0]));
        for (int i = 0; i < n; i++) pts[i] = at(i, sample(i));

        const float s_px = fb_scale();
        auto flat_seg = [&](int i) { return fabsf(pts[i + 1].y - pts[i].y) * s_px < 0.5f; };
        auto draw_runs = [&](int a, int b, ImU32 col) {
            int i = a;
            while (i < b) {
                if (flat_seg(i)) {
                    int j = i + 1;
                    while (j < b && flat_seg(j)) j++;
                    float y = 0.0f;
                    for (int k = i; k <= j; k++) y += pts[k].y;
                    y /= (float)(j - i + 1);
                    // The row that contains the run, clamped to the plot's inner rect so a
                    // zero trace sits on the last row instead of straddling the frame.
                    int row = (int)floorf(y * s_px);
                    const int last_row  = (int)ceilf(in1.y * s_px) - 1;
                    const int first_row = (int)floorf(in0.y * s_px);
                    if (row > last_row) row = last_row;
                    if (row < first_row) row = first_row;
                    const float ty = (float)row / s_px;
                    dl->AddRectFilled(ImVec2(pts[i].x, ty), ImVec2(pts[j].x, ty + 1.0f / s_px), col);
                    i = j;
                } else {
                    int j = i + 1;
                    while (j < b && !flat_seg(j)) j++;
                    dl->AddPolyline(&pts[i], j - i + 1, col, ImDrawFlags_None, 1.0f);
                    i = j;
                }
            }
        };
        if (n >= 2) draw_runs(0, n - 1, base);
        if (hovered_idx >= 0 && hovered_idx + 1 < n)
            draw_runs(hovered_idx, hovered_idx + 1, hi);

        if (hovered_idx >= 0) {
            char buf[96];
            snprintf(buf, sizeof(buf), "%d: %.2f\n%d: %.2f",
                     hovered_idx, sample(hovered_idx),
                     hovered_idx + 1, sample(hovered_idx + 1));
            // Same glass surface as combo/fields, but on the foreground list so it floats
            // above the panels instead of being clipped by the panel/scroll child.
            ImDrawList* fdl = ImGui::GetForegroundDrawList();
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            const float px = 8.0f, py = 5.0f;
            ImVec2 m = ImGui::GetIO().MousePos;
            // Same mouse offset as ImGui's own tooltip (TOOLTIP_DEFAULT_OFFSET_MOUSE 16,10) so
            // the readout sits where users expect, but drawn as a framed glass surface.
            ImVec2 t0(m.x + 16.0f, m.y + 10.0f);
            ImVec2 t1(t0.x + ts.x + px * 2.0f, t0.y + ts.y + py * 2.0f);
            const ImVec2 ds = ImGui::GetIO().DisplaySize;
            if (t1.x > ds.x - 4.0f) { t0.x = m.x - 16.0f - (t1.x - t0.x); t1.x = t0.x + ts.x + px * 2.0f; }
            if (t1.y > ds.y - 4.0f) { t0.y = m.y - 10.0f - (t1.y - t0.y); t1.y = t0.y + ts.y + py * 2.0f; }
            OxFrame(fdl, t0, t1, 4.0f, ox_surf_top(), ox_surf_bot(), ox_rim_field(220), 90, 70);
            fdl->AddText(ImVec2(crisp_lo(t0.x) + px, crisp_lo(t0.y) + py),
                         ImGui::GetColorU32(ImGuiCol_Text), buf);
        }
    }
    if (overlay && overlay[0]) {
        const ImVec2 ts = ImGui::CalcTextSize(overlay);
        dl->AddText(ImVec2(crisp_lo(f0.x + (size.x - ts.x) * 0.5f), crisp_lo(f0.y + st.FramePadding.y)),
                    ImGui::GetColorU32(ImGuiCol_Text), overlay);
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
        case KEY_GRAVE: return "`"; case KEY_MINUS: return "-"; case KEY_EQUAL: return "=";
        case KEY_LEFTBRACE: return "["; case KEY_RIGHTBRACE: return "]";
        case KEY_BACKSLASH: return "\\"; case KEY_SEMICOLON: return ";";
        case KEY_APOSTROPHE: return "'"; case KEY_COMMA: return ",";
        case KEY_DOT: return "."; case KEY_SLASH: return "/";
        case KEY_SYSRQ: return "Print"; case KEY_SCROLLLOCK: return "ScrollLock";
        case KEY_NUMLOCK: return "NumLock"; case KEY_PAUSE: return "Pause";
        case KEY_MENU: return "Menu"; case KEY_KPEQUAL: return "KP =";
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
    // Evdev codes are NOT contiguous in GLFW order: KEY_A=30 but KEY_B=48, KEY_Z=44, and
    // F11/F12 sit far above F10. The old arithmetic (KEY_A + offset) bound most letters to
    // the wrong key, so use explicit tables for the alphabetic and function key ranges.
    static const int letters[26] = {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
        KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
    };
    if (glfw_key >= GLFW_KEY_A && glfw_key <= GLFW_KEY_Z)
        return letters[glfw_key - GLFW_KEY_A];
    if (glfw_key >= GLFW_KEY_0 && glfw_key <= GLFW_KEY_9)
        return (glfw_key == GLFW_KEY_0) ? KEY_0 : KEY_1 + (glfw_key - GLFW_KEY_1);
    if (glfw_key >= GLFW_KEY_F1 && glfw_key <= GLFW_KEY_F12) {
        static const int fkeys[12] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
                                       KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12 };
        return fkeys[glfw_key - GLFW_KEY_F1];
    }
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
        case GLFW_KEY_GRAVE_ACCENT: return KEY_GRAVE;
        case GLFW_KEY_MINUS: return KEY_MINUS; case GLFW_KEY_EQUAL: return KEY_EQUAL;
        case GLFW_KEY_LEFT_BRACKET: return KEY_LEFTBRACE; case GLFW_KEY_RIGHT_BRACKET: return KEY_RIGHTBRACE;
        case GLFW_KEY_BACKSLASH: return KEY_BACKSLASH; case GLFW_KEY_SEMICOLON: return KEY_SEMICOLON;
        case GLFW_KEY_APOSTROPHE: return KEY_APOSTROPHE; case GLFW_KEY_COMMA: return KEY_COMMA;
        case GLFW_KEY_PERIOD: return KEY_DOT; case GLFW_KEY_SLASH: return KEY_SLASH;
        case GLFW_KEY_PRINT_SCREEN: return KEY_SYSRQ; case GLFW_KEY_SCROLL_LOCK: return KEY_SCROLLLOCK;
        case GLFW_KEY_NUM_LOCK: return KEY_NUMLOCK; case GLFW_KEY_PAUSE: return KEY_PAUSE;
        case GLFW_KEY_MENU: return KEY_MENU;
        default: return -1;
    }
}

static void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (!g_binding_target || action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) *g_binding_target = 0;
    else { int evdev = glfw_to_evdev(key); if (evdev >= 0) *g_binding_target = evdev; }
    g_binding_target = nullptr;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int) {
    if (!g_binding_target || action != GLFW_PRESS) return;
    // Left/right clicks cannot be used as a toggle/activation key: the clicker listens on the
    // keyboard path, and in MOUSE mode those are the buttons it is supposed to synthesise.
    // Clicking the armed field is ignored (with a short inline note); clicking anywhere else
    // still cancels the capture as before.
    if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) {
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(window, &mx, &my);
        if (mx >= g_binding_rect_min.x && mx <= g_binding_rect_max.x &&
            my >= g_binding_rect_min.y && my <= g_binding_rect_max.y) {
            g_bind_block_until = glfwGetTime() + 1.6;
        } else {
            g_binding_target = nullptr;
        }
        return;
    }
    int evdev = -1;
    switch (button) {
        case GLFW_MOUSE_BUTTON_MIDDLE: evdev = BTN_MIDDLE; break;
        case GLFW_MOUSE_BUTTON_4: evdev = BTN_SIDE; break;
        case GLFW_MOUSE_BUTTON_5: evdev = BTN_EXTRA; break;
        default: if (button >= 6) evdev = BTN_FORWARD + (button - 6); break;
    }
    if (evdev >= 0) *g_binding_target = evdev;
    g_binding_target = nullptr;
}

void setup_key_callbacks(GLFWwindow* window) {
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorEnterCallback(window, cursor_enter_callback);
}


void field_background(ImDrawList* dl, ImVec2 r0, ImVec2 r1, float rounding, bool border, int fill_alpha) {
    if (r1.x <= r0.x || r1.y <= r0.y) return;
    ImVec2 a(crisp_lo(r0.x), crisp_lo(r0.y));
    ImVec2 b(crisp_hi(r1.x), crisp_hi(r1.y));
    if (ox_is_light()) {
        // Raised light surface. A flat fill plus a white gloss does not read as a gradient on
        // an already light background, so ramp light->shade like the Oxygen menu/button faces.
        AddRoundedRectFilledGradV(dl, a, b,
            OxCol(ox_tint(g_body_color, 0.94f), fill_alpha),
            OxCol(ox_tint(g_body_color, 0.10f), fill_alpha), rounding);
    } else {
        dl->AddRectFilled(a, b, OxCol(mulc(g_body_color, 0.80f), fill_alpha), rounding);
    }
    glass_gloss(dl, a, b, rounding, a.y + (b.y - a.y) * 0.55f, 36);
    if (border)
        ox_outline(dl, a, b, rounding, ox_is_light() ? ox_rim_field(200)
                                                     : OxCol(mulc(g_body_color, 3.60f), 140));
}

void field_gloss_last_item() {
    ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
    glass_gloss(ImGui::GetWindowDrawList(), r0, r1, ImGui::GetStyle().FrameRounding,
                r0.y + (r1.y - r0.y) * 0.55f, 36);
}

// ---------- scrollbar ----------

void oxygen_scrollbar(float track_top) {
    ImGuiStyle& st = ImGui::GetStyle();
    const float smax = ImGui::GetScrollMaxY();
    if (smax <= 0.0f) return;
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    const float sbw = st.ScrollbarSize;
    float s = fb_scale();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), false);

    // The track normally spans the whole window. A sticky header passes its bottom edge so
    // the scrollbar only runs beside the scrolling content instead of through the tab bar.
    const float ty0 = (track_top > wp.y) ? track_top : wp.y;
    // track: snapped to whole physical pixels so its 1px border never smears
    ImVec2 t0(floorf((wp.x + ws.x - sbw) * s) / s, floorf(ty0 * s) / s);
    ImVec2 t1(ceilf((wp.x + ws.x) * s) / s, ceilf((wp.y + ws.y) * s) / s);
    AddRoundedRectGradCenter(dl, t0, t1,
                             OxCol(ox_is_light() ? ox_tint(g_body_color, 0.08f) : g_body_color),
                             OxCol(ox_is_light() ? ox_tint(g_body_color, -0.16f) : mulc(g_body_color, 0.80f)),
                             st.ScrollbarRounding, 10);
    ox_outline(dl, t0, t1, st.ScrollbarRounding,
               ox_is_light() ? OxCol(ox_tint(g_body_color, -0.34f), 150) : OxCol(mulc(g_body_color, 0.70f), 140));

    const float sy = ImGui::GetScrollY();
    const float track_h = t1.y - t0.y;
    float thumb_h = track_h * (track_h / (track_h + smax));
    if (thumb_h < st.GrabMinSize) thumb_h = st.GrabMinSize;
    if (thumb_h > track_h) thumb_h = track_h;
    float ty = t0.y + (sy / smax) * (track_h - thumb_h);

    // thumb: 2px narrower than the track on each side (keeps a clear gap, looks lighter)
    const float inset = 2.0f;
    // One extra pixel at the top: at scroll 0 the thumb would otherwise fuse with the 1px
    // rule drawn over the track's top edge.
    ImVec2 g0(crisp_lo(t0.x + inset), crisp_lo(ty + inset + 1.0f));
    ImVec2 g1(crisp_hi(t1.x - inset), crisp_hi(ty + thumb_h - inset));
    float gr = st.ScrollbarRounding - 1.0f; if (gr < 1.0f) gr = 1.0f;

    ImVec2 mp = ImGui::GetMousePos();
    bool in_track = mp.x >= t0.x && mp.x <= t1.x && mp.y >= t0.y && mp.y <= t1.y;
    bool in_thumb = mp.x >= g0.x && mp.x <= g1.x && mp.y >= g0.y && mp.y <= g1.y;
    static bool s_thumb_drag = false;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) s_thumb_drag = false;
    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && in_track) s_thumb_drag = true;
    bool lit = in_thumb || s_thumb_drag;
    float t = widget_anim(ImGui::GetID("##sb_thumb"), lit);
    ImU32 center_c = OxCol(ox_is_light() ? ox_tint(g_body_color, 0.72f) : mulc(g_body_color, 1.85f));
    ImU32 edge_c   = OxCol(ox_is_light() ? ox_tint(g_body_color, 0.28f) : mulc(g_body_color, 1.45f));
    ImU32 rim_c    = lerp_u32(ox_is_light() ? OxCol(lerp4(g_body_color, g_text_color, 0.45f), 220)
                                            : OxCol(mulc(g_body_color, 4.30f), 200),
                              ox_accent_rim(), t);
    AddRoundedRectGradCenter(dl, g0, g1, center_c, edge_c, gr, 10);
    ox_outline(dl, g0, g1, gr, rim_c);
    dl->PopClipRect();
}

// ---------- buttons ----------

bool oxygen_button(const char* label, ImVec2 size_arg, ImU32 c1_override, ImU32 c2_override,
                   bool force_colored, OxIcon icon) {
    const float R = OXY_R;
    ImVec2 ls = ImGui::CalcTextSize(label, nullptr, true);
    const bool has_icon = icon != OxIcon::None;
    // Oxygen symbolic icons are designed on a 16px grid; drawing them at exactly that
    // size keeps every pixel of the baked mask on screen without resampling.
    constexpr float isz = 16.0f;
    const float igap = 6.0f;
    const float extra = has_icon ? isz + (ls.x > 0.0f ? igap : 0.0f) : 0.0f;
    ImVec2 sz = size_arg;
    if (sz.x <= 0) sz.x = ls.x + extra + ImGui::GetStyle().FramePadding.x * 2.0f;
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
        bc = ox_accent_rim();
        gloss = ox_is_light() ? 60 : 120;
    } else {
        // Light theme: the label sits in the middle of the vertical ramp, where the accent is
        // already deep, so use a slightly deeper face and white ink (see below) to keep the
        // selected/pressed label legible. Dark theme keeps its accepted accent fill.
        const ImVec4 face = ox_is_light() ? mulc(g_accent_color, 0.80f) : g_accent_color;
        c1 = lerp_u32(ox_fill_top(), OxCol(face), t_a);
        c2 = lerp_u32(ox_fill_bot(), OxCol(OxDark(face)), t_a);
        bc = lerp_u32(ox_rim_neutral(180), ox_accent_rim(), t_h > t_a ? t_h : t_a);
        gloss = ox_is_light() ? 20 : (60 + (int)(60 * t_a));
    }
    int shadow = ox_is_light() ? 26 : 60;
    OxFrame(dl, mn, mx, R, c1, c2, bc, gloss, shadow);
    const char* tend = strstr(label, "##");
    // ox_ink_on() picks near-black for a mid-luma accent, which reads muddy over the darker
    // half of the gradient. Fade to white as the accent fill comes in (the ON pills and the
    // dark theme already use white there).
    ImU32 ink = ox_ink_on(c1);
    if (ox_is_light() && t_a > 0.01f)
        ink = lerp_u32(ink, IM_COL32(255, 255, 255, 255), t_a);
    const float content_w = ls.x + extra;
    const float tx = mn.x + (sz.x - content_w) * 0.5f;
    if (has_icon) {
        ox_draw_icon(dl, icon,
                     ImVec2(tx + isz * 0.5f, (mn.y + mx.y) * 0.5f), isz, ink);
        dl->AddText(ImVec2(tx + isz + igap, vcenter_top(mn.y, mx.y)), ink,
                    label, tend ? tend : nullptr);
    } else {
        dl->AddText(ImVec2(mn.x + (sz.x - ls.x) * 0.5f, vcenter_top(mn.y, mx.y)),
                    ink, label, tend ? tend : nullptr);
    }
    return clicked;
}

bool ox_preset_tile(const char* id, const char* title, const char* sub, ImVec2 size, bool selected) {
    const float R = OXY_R;
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 mx(mn.x + size.x, mn.y + size.y);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hov = ImGui::IsItemHovered();
    const bool act = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiID wid = ImGui::GetID(id);
    const float t_h = widget_anim(wid, hov);
    const float t_a = widget_anim(wid + 1, act || selected);

    // Same face/rim language as oxygen_button(), so a tile reads as the same control family:
    // neutral button gradient when unselected, accent fill when it is the active preset.
    const ImVec4 face = ox_is_light() ? mulc(g_accent_color, 0.80f) : g_accent_color;
    const ImU32 c1 = lerp_u32(ox_fill_top(), OxCol(face), t_a);
    const ImU32 c2 = lerp_u32(ox_fill_bot(), OxCol(OxDark(face)), t_a);
    const ImU32 bc = lerp_u32(ox_rim_neutral(180), ox_accent_rim(), t_h > t_a ? t_h : t_a);
    const int gloss = ox_is_light() ? 20 : (60 + (int)(60 * t_a));
    const int shadow = ox_is_light() ? 26 : 60;
    OxFrame(dl, mn, mx, R, c1, c2, bc, gloss, shadow);

    ImU32 ink = ox_ink_on(c1);
    if (ox_is_light() && t_a > 0.01f)
        ink = lerp_u32(ink, IM_COL32(255, 255, 255, 255), t_a);
    // The range is supporting text: same ink, faded, so the name stays the anchor.
    const ImU32 ink_dim = (ink & ~IM_COL32_A_MASK) |
                          ((ImU32)((float)((ink >> IM_COL32_A_SHIFT) & 0xFF) * 0.62f) << IM_COL32_A_SHIFT);

    const float half = size.y * 0.5f;
    const ImVec2 ts = ImGui::CalcTextSize(title, nullptr, true);
    const ImVec2 ss = ImGui::CalcTextSize(sub, nullptr, true);
    dl->AddText(ImVec2(crisp_lo(mn.x + (size.x - ts.x) * 0.5f), vcenter_top(mn.y + 1.0f, mn.y + half)),
                ink, title);
    dl->AddText(ImVec2(crisp_lo(mn.x + (size.x - ss.x) * 0.5f), vcenter_top(mn.y + half, mx.y - 1.0f)),
                ink_dim, sub);
    return clicked;
}

bool oxygen_field_button(const char* label, ImVec2 size_arg, bool armed) {
    const float R = OXY_R;
    const char* tend = strstr(label, "##");
    ImVec2 ls = ImGui::CalcTextSize(label, tend, true);
    ImVec2 sz = size_arg;
    if (sz.x <= 0) sz.x = ls.x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (sz.y <= 0) sz.y = ImGui::GetFrameHeight();
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 mx(mn.x + sz.x, mn.y + sz.y);
    bool clicked = ImGui::InvisibleButton(label, sz);
    bool hov = ImGui::IsItemHovered();
    float t_h = widget_anim(ImGui::GetID(label), hov || armed);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    field_background(dl, mn, mx, R, true);
    if (t_h > 0.01f)
        ox_outline(dl, mn, mx, R,
                   lerp_u32(IM_COL32(0, 0, 0, 0), ox_accent_rim(), t_h));

    ImVec2 a = crisp_min(mn), b = crisp_max(mx);
    dl->AddText(ImVec2(a.x + ((b.x - a.x) - ls.x) * 0.5f, vcenter_top(a.y, b.y)),
                armed ? ox_accent_text() : ImGui::GetColorU32(ImGuiCol_Text),
                label, tend ? tend : nullptr);
    return clicked;
}

// ---------- checkbox ----------

bool oxygen_checkbox(const char* label, bool* v, bool label_clickable) {
    const float sz = ImGui::GetFrameHeight();
    const float R = OXY_R;
    const char* tend = strstr(label, "##");
    ImVec2 ls = ImGui::CalcTextSize(label, tend, true);
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 total(sz + (ls.x > 0.0f ? ImGui::GetStyle().ItemInnerSpacing.x + ls.x : 0.0f), sz);
    bool clicked_item = ImGui::InvisibleButton(label, total);
    const bool in_box = ImGui::GetIO().MousePos.x <= mn.x + sz;
    bool clicked = clicked_item && (label_clickable || in_box);
    if (clicked) *v = !*v;
    bool hov = ImGui::IsItemHovered() && (label_clickable || in_box);
    bool act = ImGui::IsItemActive() && (label_clickable || in_box);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Identity comes from the "##id" suffix when the label has one, so a checkbox whose visible
    // text changes with its state ("Enabled##inv" / "Disabled##inv") keeps its animation state
    // instead of restarting from zero — and popping — on every click.
    const char* id_part = strstr(label, "##");
    ImGuiID id = ImGui::GetID(id_part ? id_part : label);
    float t_h = widget_anim(id, hov);
    float t_c = widget_anim(id + 1, *v);
    ImVec2 bx1(mn.x + sz, mn.y + sz);
    ImVec2 cmn = crisp_min(mn);
    ImVec2 cbx1 = crisp_max(bx1);
    ImU32 fill = lerp_u32(OxCol(mulc(g_body_color, 1.90f), 235),
                          OxCol(act ? g_accent_hover : g_accent_active), t_c);
    ImU32 rimc = lerp_u32(ox_rim_neutral(200), ox_accent_rim(), t_h > t_c ? t_h : t_c);
    if (ox_is_light()) {
        // Both states get a real vertical ramp: unchecked = light->shade, checked = accent
        // light->dark, so the box has the same depth as the buttons and fields next to it.
        ImU32 top_c = lerp_u32(OxCol(ox_tint(g_body_color, 0.96f), 245), OxCol(g_accent_hover), t_c);
        ImU32 bot_c = lerp_u32(OxCol(ox_tint(g_body_color, 0.26f), 245), OxCol(g_accent_active), t_c);
        AddRoundedRectFilledGradV(dl, cmn, cbx1, top_c, bot_c, R);
        glass_gloss(dl, cmn, cbx1, R, cmn.y + sz * 0.58f, 46);
    } else {
        dl->AddRectFilled(cmn, cbx1, fill, R);
        glass_gloss(dl, cmn, cbx1, R, cmn.y + sz * 0.55f, 36 + (int)(44 * t_c));
    }
    ox_outline(dl, cmn, cbx1, R, rimc);
    if (t_c > 0.01f) {
        const float th = (sz * 0.09f < 1.4f) ? 1.4f : (sz * 0.09f);
        ImVec2 pts[3] = {
            ImVec2(cmn.x + sz * 0.30f, cmn.y + sz * 0.52f),
            ImVec2(cmn.x + sz * 0.45f, cmn.y + sz * 0.66f),
            ImVec2(cmn.x + sz * 0.70f, cmn.y + sz * 0.36f),
        };
        // The tick fades with the fill but on a steeper curve: it is gone once the box is a
        // third of the way back to its unchecked shade, instead of hanging over a grey box for
        // the whole fade. It also always faded by alpha, never by hard-cutting (which is what
        // made it look like it vanished in one glitchy frame).
        const float tick_t = (t_c - 0.35f) / 0.65f;
        const int tick_a = (int)(255.0f * (tick_t < 0.0f ? 0.0f : (tick_t > 1.0f ? 1.0f : tick_t)) + 0.5f);
        // Ink is picked from the *checked* fill and never the animating one: ox_ink_luma() is a
        // hard luminance threshold, so choosing from the lerped fill made the tick flip
        // white<->black part-way through the animation. Light theme uses white on the accent.
        if (tick_a > 0) {
            const ImU32 ink = ((ox_is_light() ? IM_COL32(255, 255, 255, 0)
                                             : ox_ink_on(OxCol(g_accent_active))) & ~IM_COL32_A_MASK)
                              | ((ImU32)tick_a << IM_COL32_A_SHIFT);
            dl->AddPolyline(pts, 3, ink, 0, th);
        }
    }
    if (ls.x > 0.0f)
        dl->AddText(ImVec2(cbx1.x + ImGui::GetStyle().ItemInnerSpacing.x, vcenter_top(mn.y, mn.y + sz)),
                    ImGui::GetColorU32(ImGuiCol_Text), label, tend ? tend : nullptr);
    return clicked;
}

// ---------- sliders ----------

static bool slider_widget(const char* id, double* v, double lo, double hi) {
    push_transparent_frame();
    bool changed = ImGui::SliderScalar(id, ImGuiDataType_Double, v, &lo, &hi, "", ImGuiSliderFlags_NoInput);
    pop_transparent_frame();
    ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float FR = ImGui::GetStyle().FrameRounding;
    bool hov = ImGui::IsItemHovered(); bool act = ImGui::IsItemActive();
    float ta = widget_anim(ImGui::GetID(id), hov || act);
    ox_sunken(dl, r0, r1, FR, ta * 0.6f);
    float grab_w = ImGui::GetStyle().GrabMinSize;
    float t = (float)((*v - lo) / (hi - lo)); if (t < 0) t = 0; if (t > 1) t = 1;
    // 3px from the frame's outer edge: the frame's own 1px border consumes one of them, so
    // the visible gap between border and thumb is exactly 2px at either end (was 1px).
    const float pad = 3.0f;
    float gx = r0.x + pad + ((r1.x - r0.x) - grab_w - pad * 2.0f) * t;
    float gs = fb_scale();
    float gx0 = floorf(gx * gs) / gs;
    float gw = ceilf(grab_w * gs) / gs;
    ImVec2 g0(gx0, crisp_lo(r0.y + 3.0f)), g1(gx0 + gw, crisp_hi(r1.y - 3.0f));
    ImU32 rim_c    = lerp_u32(ox_thumb_rim(), ox_accent_rim(), ta);
    float gr = (g1.x - g0.x) * 0.5f; if (gr > OXY_R) gr = OXY_R;
    if (ox_is_light()) {
        // Light handle: a vertical light->shade ramp (the horizontal center->edge ramp used
        // on dark reads as a flat capsule against a light track).
        ImU32 top_c = lerp_u32(OxCol(ox_tint(g_body_color, 0.96f)), OxCol(g_accent_hover), ta);
        ImU32 bot_c = lerp_u32(OxCol(ox_tint(g_body_color, 0.16f)), OxCol(g_accent_active), ta);
        AddRoundedRectFilledGradV(dl, g0, g1, top_c, bot_c, gr);
    } else {
        ImU32 center_c = lerp_u32(OxCol(mulc(g_body_color, 1.85f)), OxCol(g_accent_hover), ta);
        ImU32 edge_c   = lerp_u32(OxCol(mulc(g_body_color, 1.45f)), OxCol(g_accent_active), ta);
        AddRoundedRectGradCenter(dl, g0, g1, center_c, edge_c, gr, 8);
    }
    ox_outline(dl, g0, g1, gr, rim_c);
    return changed;
}

bool row_slider(const char* label, double* v, double lo, double hi, const char* fmt) {
    ImGui::AlignTextToFramePadding();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(ctrl_x0(), start.y));
    ImGui::SetNextItemWidth(ctrl_w_slider());
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    bool changed = slider_widget(id, v, lo, hi);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
    char vid[112]; snprintf(vid, sizeof(vid), "##%s_val", label);
    if (ox_input_double(vid, v, 0.0, 0.0, fmt, VALUE_W)) {
        if (*v < lo) *v = lo; else if (*v > hi) *v = hi;
        changed = true;
    }
    field_gloss_last_item();
    return changed;
}

bool stacked_slider(const char* label, double* v, double lo, double hi, const char* fmt, float w) {
    ImGui::TextDisabled("%s", label);
    ImGui::SetNextItemWidth(w - VALUE_W - ImGui::GetStyle().ItemSpacing.x);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    bool changed = slider_widget(id, v, lo, hi);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
    char vid[112]; snprintf(vid, sizeof(vid), "##%s_val", label);
    if (ox_input_double(vid, v, 0.0, 0.0, fmt, VALUE_W)) {
        if (*v < lo) *v = lo; else if (*v > hi) *v = hi;
        changed = true;
    }
    field_gloss_last_item();
    return changed;
}

// ---------- combos ----------

bool combo_framed(const char* id, int* idx, const char* items) {
    int count = 0;
    for (const char* p = items; *p; p += strlen(p) + 1) count++;
    const char* preview = items;
    for (int i = 0; i < *idx && *preview; i++) preview += strlen(preview) + 1;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    // Suppress ImGui's default popup frame (background + border + border size) while the
    // popup is open. These must be pushed/popped in the PARENT window context (i.e. around
    // BeginCombo/EndCombo), otherwise ImGui reports unbalanced style stacks between windows.
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const float FR = ImGui::GetStyle().FrameRounding;
    const float arrow_sz = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImVec2 f0 = ImGui::GetCursorScreenPos();
    const float fw = ImGui::CalcItemWidth();
    const float fhgt = ImGui::GetFrameHeight();
    ImVec2 f1(f0.x + fw, f0.y + fhgt);
    ImGuiID wid = ImGui::GetID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;
    // HeightLarge (20 items) instead of the default HeightRegular (8): with the regular cap
    // ImGui clamps the auto-fitted popup height, and as soon as the content is taller than
    // that cap a vertical scrollbar appears. That scrollbar (a) makes the auto-fitted popup
    // exactly ScrollbarSize wider than the combo and (b) shrinks the popup's InnerClipRect by
    // the same amount, so our background/border drawing gets clipped before reaching the right
    // edge: the missing right border + stray strip of panel next to this wide dropdown.
    bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge);
    ImGui::PopStyleVar();   // WindowPadding
    bool fhov = ImGui::IsMouseHoveringRect(f0, f1);
    float t_h = widget_anim(wid, fhov);
    float t_o = widget_anim(wid + 1, open);
    ImU32 c1 = lerp_u32(ox_surf_top(), OxCol(g_accent_color), t_o);
    ImU32 c2 = lerp_u32(ox_surf_bot(), OxCol(OxDark(g_accent_color)), t_o);
    ImU32 bc = lerp_u32(ox_rim_field(200), ox_accent_rim(), t_h > t_o ? t_h : t_o);
    // While the popup is open, square off the combo's corners so its rounded bottom
    // corners don't leave dark notches right above the (flush) popup top edge.
    OxFrame(dl, f0, f1, FR * (1.0f - t_o), c1, c2, bc, 60 + (int)(80 * t_o), 60);
    dl->AddText(ImVec2(f0.x + ImGui::GetStyle().FramePadding.x, vcenter_top(f0.y, f1.y)),
                lerp_u32(OxCol(g_text_color, 205), IM_COL32(238, 246, 255, 255), t_o), preview, nullptr);
    float ax = f1.x - arrow_sz * 0.5f, ay = (f0.y + f1.y) * 0.5f;
    // The arrow used to be hard-coded white: invisible on a light combo frame.
    const ImU32 arrow_col = (ox_is_light() && !open)
        ? OxCol(lerp4(g_body_color, g_text_color, 0.62f)) : IM_COL32(255, 255, 255, 220);
    if (open) {
        float as = 6.0f;
        dl->AddTriangleFilled(ImVec2(ax - as, ay + as * 0.5f), ImVec2(ax + as, ay + as * 0.5f), ImVec2(ax, ay - as * 0.5f), arrow_col);
    } else {
        float as = 6.0f;
        dl->AddTriangleFilled(ImVec2(ax - as, ay - as * 0.5f), ImVec2(ax + as, ay - as * 0.5f), ImVec2(ax, ay + as * 0.5f), arrow_col);
    }
    if (open) {
        // The popup's own WindowPadding.x is forced by ImGui to FramePadding.x, and it is
        // added to the auto-fitted width. Using the same value as the item inset makes the
        // auto-fitted popup exactly as wide as the combo frame (no right-side overhang).
        const float PPAD = ImGui::GetStyle().FramePadding.x;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        ImVec2 ppos = ImGui::GetWindowPos();
        ImVec2 psize = ImGui::GetWindowSize();
        ImVec2 p1(ppos.x + psize.x, ppos.y + psize.y);

        float item_w = fw - PPAD * 2.0f;
        if (item_w < 1.0f) item_w = 1.0f;

        // The popup overlaps the combo frame when it opens upwards, so its fill must be fully
        // opaque (a translucent fill would ghost the combo's accent frame through it). One
        // opaque rounded shape + gloss + border also means there is no second square base
        // poking out past the rounded corners.
        //
        // The popup draw list is clipped to the window's InnerClipRect, which ImGui floors to
        // whole pixels. With a fractional auto-fitted width our outward pixel snapping can put
        // the right/bottom border just past that clip, which is why only this (wider) dropdown
        // was missing its right border. Clamp the painted rect to the clip rect first.
        ImVec2 cl = pdl->GetClipRectMin(), ch = pdl->GetClipRectMax();
        ImVec2 s0 = crisp_min(ppos), s1 = crisp_max(p1);
        if (s0.x < cl.x) s0.x = cl.x;
        if (s0.y < cl.y) s0.y = cl.y;
        if (s1.x > ch.x) s1.x = ch.x;
        if (s1.y > ch.y) s1.y = ch.y;
        if (s1.x > s0.x && s1.y > s0.y)
            field_background(pdl, s0, s1, ImGui::GetStyle().PopupRounding, true, 255);

        float row_y = ppos.y + PPAD;
        float last_bottom = row_y;
        const char* p = items;
        for (int i = 0; i < count; i++) {
            ImGui::SetCursorScreenPos(ImVec2(ppos.x + PPAD, row_y));
            ImVec2 p0(ppos.x + PPAD, row_y);
            float h = ImGui::GetFrameHeight();
            ImVec2 r1(p0.x + item_w, p0.y + h);
            last_bottom = r1.y;
            char oid[32]; snprintf(oid, sizeof(oid), "##opt%d", i);
            bool clicked = ImGui::InvisibleButton(oid, ImVec2(item_w, h));
            bool hov = ImGui::IsItemHovered();
            bool down = hov && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            ImGuiID oid_id = ImGui::GetID(oid);
            float o_h = widget_anim(oid_id, hov);
            float o_s = widget_anim(oid_id + 1, i == *idx);
            ImVec2 cp0 = crisp_min(p0);
            ImVec2 cr1 = crisp_max(r1);
            if (o_s > 0.01f) {
                pdl->AddRectFilled(cp0, cr1, OxCol(g_accent_color, (int)(70 * o_s)), OXY_R);
                glass_gloss(pdl, cp0, cr1, OXY_R, cp0.y + (cr1.y - cp0.y) * 0.55f, (int)(26 * o_s));
            }
            if (down) pdl->AddRectFilled(cp0, cr1, OxCol(g_accent_active, 60), OXY_R);
            float b_t = o_h > o_s ? o_h : o_s;
            if (b_t > 0.01f)
                ox_outline(pdl, p0, r1, OXY_R,
                           lerp_u32(IM_COL32(0, 0, 0, 0), ox_accent_rim(), b_t));
            ImU32 tcol = (i == *idx)
                ? (ox_is_light() ? ImGui::GetColorU32(ImGuiCol_Text) : IM_COL32(240, 246, 255, 255))
                : ImGui::GetColorU32(ImGuiCol_Text);
            pdl->AddText(ImVec2(p0.x + ImGui::GetStyle().FramePadding.x, vcenter_top(p0.y, p0.y + h)), tcol, p, nullptr);
            // subtle separator between items
            if (i + 1 < count) {
                float sy = crisp_lo(r1.y + 2.0f);
                pdl->AddLineH(crisp_lo(p0.x + 4.0f), crisp_hi(r1.x - 4.0f), sy,
                              ox_is_light() ? OxCol(lerp4(g_body_color, g_text_color, 0.35f), 90)
                                            : ox_rim_neutral(55),
                              1.0f / fb_scale());
            }
            if (clicked) { *idx = i; changed = true; ImGui::CloseCurrentPopup(); }
            row_y = r1.y + 4.0f;
            p += strlen(p) + 1;
        }
        ImGui::SetCursorScreenPos(ImVec2(ppos.x + PPAD, last_bottom + PPAD));
        ImGui::Dummy(ImVec2(item_w, 0.0f));
        ImGui::PopStyleVar();
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();      // PopupBorderSize
    ImGui::PopStyleColor(2);   // PopupBg, Border
    ImGui::PopStyleVar();      // FrameBorderSize
    return changed;
}

bool stacked_combo(const char* label, int* idx, const char* items, float w) {
    ImGui::TextDisabled("%s", label);
    ImGui::SetNextItemWidth(w);
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return combo_framed(id, idx, items);
}

bool row_combo(const char* label, int* idx, const char* items) {
    ImGui::AlignTextToFramePadding();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(ctrl_x0(), start.y));   // same start point as sliders
    ImGui::SetNextItemWidth(ctrl_w_full());                  // same right edge as slider+value
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return combo_framed(id, idx, items);
}

bool row_check(const char* label, bool* v) {
    ImGui::AlignTextToFramePadding();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(ctrl_x0(), start.y));
    char id[96]; snprintf(id, sizeof(id), "##%s", label);
    return oxygen_checkbox(id, v);
}

bool row_color3(const char* label, float* rgb) {
    ImGui::AlignTextToFramePadding();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(ctrl_x0(), start.y));

    ImGui::PushID(label);
    bool changed = false;

    // Custom swatch + popup. The stock ColorEdit3 popup is a plain flat window with no themed
    // border, so draw our own: a gradient surface with the usual rim, containing a themed
    // picker. Keeping the popup narrow lets it fit above the row when space below is tight.
    const float sq = ImGui::GetFrameHeight();
    const bool sw_clicked = ImGui::InvisibleButton("##swatch", ImVec2(sq, sq));
    const ImVec2 sw_min = ImGui::GetItemRectMin();
    const ImVec2 sw_max = ImGui::GetItemRectMax();
    const bool sw_hov = ImGui::IsItemHovered();
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rr = 3.0f;
        const ImVec2 a = crisp_min(sw_min), b = crisp_max(sw_max);
        const ImU32 c_top = ImGui::ColorConvertFloat4ToU32(ImVec4(rgb[0], rgb[1], rgb[2], 1.0f));
        const ImU32 c_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(rgb[0] * 0.82f, rgb[1] * 0.82f, rgb[2] * 0.82f, 1.0f));
        AddRoundedRectFilledGradV(dl, a, b, c_top, c_bot, rr);
        glass_gloss(dl, a, b, rr, a.y + (b.y - a.y) * 0.55f, 90);
        const float t = widget_anim(ImGui::GetID("##swatch"), sw_hov);
        ox_outline(dl, a, b, rr, lerp_u32(ox_rim_neutral(190), ox_accent_rim(), t));
    }
    if (sw_clicked) {
        ImGui::OpenPopup("##accent_picker");
        // ~280px tall; flip above the swatch when it would run past the window bottom.
        const float est_h = 280.0f;
        float py = sw_max.y + ImGui::GetStyle().ItemSpacing.y;
        if (py + est_h > ImGui::GetIO().DisplaySize.y)
            py = sw_min.y - est_h - ImGui::GetStyle().ItemSpacing.y;
        if (py < 4.0f) py = 4.0f;
        ImGui::SetNextWindowPos(ImVec2(sw_min.x, py));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, OXY_R);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(0, 0, 0, 0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(260, 0), ImVec2(260, 10000));
    if (ImGui::BeginPopup("##accent_picker")) {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        pdl->ChannelsSplit(2);
        pdl->ChannelsSetCurrent(1);
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::ColorPicker3("##picker", rgb,
                                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoSidePreview |
                                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions |
                                ImGuiColorEditFlags_PickerHueBar))
            changed = true;
        ImGui::Spacing();
        ImGui::TextDisabled("#%02X%02X%02X",
                            (int)lroundf(rgb[0] * 255.0f),
                            (int)lroundf(rgb[1] * 255.0f),
                            (int)lroundf(rgb[2] * 255.0f));
        pdl->ChannelsSetCurrent(0);
        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        const ImVec2 wb(wp.x + ws.x, wp.y + ws.y);
        // Light theme uses the same chrome ramp as other popups. On a dark theme the popup
        // surface only shows as a ~12px frame around the picker, so the ramp has to stay
        // clearly above the window colour the whole way down: the old 2.45x -> 1.28x body
        // ended almost on the page colour (#222425 vs #191a1b), which read as "no background"
        // rather than as a gradient. Ramps wider and gets the top gloss every glass surface has.
        const ImU32 ptop = ox_is_light() ? ox_surf_top() : OxCol(mulc(g_body_color, 3.00f));
        const ImU32 pbot = ox_is_light() ? ox_surf_bot() : OxCol(mulc(g_body_color, 1.75f));
        AddRoundedRectFilledGradV(pdl, wp, wb, ptop, pbot, OXY_R);
        glass_gloss(pdl, wp, wb, OXY_R, wp.y + (wb.y - wp.y) * 0.55f, ox_is_light() ? 44 : 38);
        ox_outline(pdl, wp, wb, OXY_R, ox_rim_field(240));
        pdl->ChannelsMerge();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    static const char* const tags[3] = { "R", "G", "B" };
    static const char* const ids[3] = { "##r", "##g", "##b" };
    for (int i = 0; i < 3; i++) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", tags[i]);
        ImGui::SameLine();
        int v = (int)lroundf(rgb[i] * 255.0f);
        // No +- step buttons: they would eat the whole narrow field and hide the value.
        if (ox_input_int(ids[i], &v, 0, 0, 76.0f)) {
            if (v < 0) v = 0; else if (v > 255) v = 255;
            rgb[i] = (float)v / 255.0f;
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

// ---------- keybind rows ----------

static void keybind_label_buf(char* buf, size_t n, const char* label, int& key_code) {
    if (g_binding_target == &key_code) {
        if (glfwGetTime() < g_bind_block_until) snprintf(buf, n, "L/R not allowed##%s", label);
        else                                    snprintf(buf, n, "Press a key...##%s", label);
    }
    else if (key_code == 0)            snprintf(buf, n, "Click to bind##%s", label);
    else                               snprintf(buf, n, "%s##%s", key_name(key_code), label);
}

// Called right after a bind field was clicked, so the mouse callback can tell an ignored
// left/right press on the armed field apart from a click that cancels the capture.
static void begin_binding(int& key_code) {
    // Keep a recently shown "L/R not allowed" note when the user clicks the armed field again;
    // only reset it when capture moves to a different field.
    if (g_binding_target != &key_code) g_bind_block_until = 0.0;
    g_binding_target = &key_code;
    g_binding_rect_min = ImGui::GetItemRectMin();
    g_binding_rect_max = ImGui::GetItemRectMax();
}

void keybind_row(const char* label, int& key_code) {
    ImGui::AlignTextToFramePadding();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    float lw = ImGui::CalcTextSize(label, nullptr, true).x;
    // button sits right after the label text instead of at the fixed 118px column
    ImGui::SetCursorScreenPos(ImVec2(start.x + lw + ImGui::GetStyle().ItemSpacing.x, start.y));
    char buf[64]; keybind_label_buf(buf, sizeof(buf), label, key_code);
    if (oxygen_field_button(buf, ImVec2(140, 0), g_binding_target == &key_code)) begin_binding(key_code);
}

void stacked_keybind(const char* label, int& key_code, float w) {
    ImGui::TextDisabled("%s", label);
    char buf[64]; keybind_label_buf(buf, sizeof(buf), label, key_code);
    if (oxygen_field_button(buf, ImVec2(w, 0), g_binding_target == &key_code)) begin_binding(key_code);
}

// ---------- flat panels (cards) ----------

float panel_inner() { return s_panel_inner; }

void begin_panel(const char* label, float width, int status, OxIcon icon) {
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
    dl->AddRectFilled(start, p1, OxCol(ox_is_light() ? mulc(g_body_color, 1.10f, 235) : mulc(g_body_color, 1.16f, 235)), PANEL_R);
    ox_outline(dl, start, p1, PANEL_R, ox_rim_neutral(160));

    // Optional symbolic glyph before the title. The group must start at the card's normal
    // left padding: BeginGroup() adopts the cursor x as the indent for every following line,
    // so shifting the cursor before BeginGroup() dragged the whole body to the right with it.
    // Only the title line itself is offset, and ItemSize() then snaps back to the group indent.
    constexpr float isz = 16.0f;
    const float igap = 6.0f;
    const bool has_icon = icon != OxIcon::None;
    ImGui::SetCursorScreenPos(ImVec2(start.x + PANEL_PAD, start.y + 12.0f));
    s_panel_inner = panel_w - PANEL_PAD * 2.0f;

    ImGui::BeginGroup();

    if (has_icon)
        ImGui::SetCursorScreenPos(ImVec2(start.x + PANEL_PAD + isz + igap, start.y + 12.0f));

    // The panel header was a 45% accent tint at 200 alpha: pleasant on dark, washed out on
    // a light panel. Deepen it in light mode instead of fading it out.
    const ImVec4 hdr_col = lerp4(g_text_color, g_accent_color, ox_is_light() ? 0.55f : 0.45f);
    const ImU32 hdr_ink = ox_is_light() ? OxCol(hdr_col) : OxCol(hdr_col, 200);
    ImGui::PushStyleColor(ImGuiCol_Text, hdr_ink);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 4.0f));
    char up[128]; strtoupper(up, sizeof(up), label);
    ImGui::TextUnformatted(up);
    if (has_icon) {
        const ImVec2 tmn = ImGui::GetItemRectMin(), tmx = ImGui::GetItemRectMax();
        // Full alpha: the 200-alpha header tint plus antialiasing made small glyphs look fuzzy.
        ox_draw_icon(dl, icon,
                     ImVec2(start.x + PANEL_PAD + isz * 0.5f, (tmn.y + tmx.y) * 0.5f),
                     isz, OxCol(hdr_col));
    }
    ImGui::PopStyleColor();

    if (status >= 0) {
        ImVec2 hmn = ImGui::GetItemRectMin(), hmx = ImGui::GetItemRectMax();
        float r = 5.0f;
        ImVec2 c(start.x + panel_w - PANEL_PAD - r, (hmn.y + hmx.y) * 0.5f);
        const bool L = ox_is_light();
        if (status == 2) {
            // Paused: switched on, but not clicking (pointer on this window, or a game screen
            // is open). Amber, so it cannot be mistaken for the green running dot.
            dl->AddCircleFilled(c, r + 3.5f, IM_COL32(214, 158, 46, L ? 20 : 38));
            dl->AddCircleFilled(c, r + 1.5f, IM_COL32(232, 176, 64, L ? 38 : 66));
            dl->AddCircleFilled(c, r,        IM_COL32(214, 158, 46, 255));
            dl->AddCircle(c, r, L ? IM_COL32(140, 100, 24, 210) : IM_COL32(246, 214, 140, 200), 0, 1.0f);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.35f), r * 0.45f, IM_COL32(255, 255, 255, L ? 110 : 90));
        } else if (status) {
            // Softer halo in light mode: the old 40/70 alpha rings smear into a green blur.
            dl->AddCircleFilled(c, r + 3.5f, IM_COL32(46, 148, 84, L ? 22 : 40));
            dl->AddCircleFilled(c, r + 1.5f, IM_COL32(70, 190, 110, L ? 40 : 70));
            dl->AddCircleFilled(c, r,        IM_COL32(52, 168, 96, 255));
            dl->AddCircle(c, r, L ? IM_COL32(28, 104, 58, 210) : IM_COL32(150, 230, 180, 200), 0, 1.0f);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.35f), r * 0.45f, IM_COL32(255, 255, 255, L ? 110 : 90));
        } else {
            // White alpha rings are invisible on a light panel — use a grey ring instead.
            dl->AddCircleFilled(c, r, L ? OxCol(lerp4(g_body_color, g_text_color, 0.18f), 70) : IM_COL32(255, 255, 255, 26));
            dl->AddCircle(c, r, L ? OxCol(lerp4(g_body_color, g_text_color, 0.45f), 190) : IM_COL32(255, 255, 255, 70), 0, 1.0f);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.35f), r * 0.45f, IM_COL32(255, 255, 255, L ? 70 : 24));
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
    // ImGui 1.93+: the last call in a window must be an item, not a bare SetCursorPos(),
    // otherwise End() asserts "extend window/parent boundaries". Submit a no-size item.
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopID();
}

// ---------- list items (styled like combo dropdown options) ----------

bool oxygen_list_item(const char* id, const char* label, bool selected, bool separator) {
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
    ImVec2 cp0 = crisp_min(p0), cr1 = crisp_max(r1);
    if (o_s > 0.01f) {
        dl->AddRectFilled(cp0, cr1, OxCol(g_accent_color, (int)(70 * o_s)), OXY_R);
        glass_gloss(dl, cp0, cr1, OXY_R, cp0.y + (cr1.y - cp0.y) * 0.55f, (int)(26 * o_s));
    }
    if (hov && !selected) dl->AddRectFilled(cp0, cr1, OxCol(g_accent_color, (int)(38 * o_h)), OXY_R);
    float b_t = o_h > o_s ? o_h : o_s;
    if (b_t > 0.01f)
        ox_outline(dl, p0, r1, OXY_R,
                   lerp_u32(IM_COL32(0, 0, 0, 0), ox_accent_rim(), b_t));
    ImU32 tcol = selected
        ? (ox_is_light() ? ImGui::GetColorU32(ImGuiCol_Text) : IM_COL32(240, 246, 255, 255))
        : ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddText(ImVec2(p0.x + ImGui::GetStyle().FramePadding.x, vcenter_top(p0.y, r1.y)), tcol, label, nullptr);
    if (separator) {
        // Same hairline as the open dropdown's row separators, drawn in the middle of the
        // item gap so profile rows are visually split the way menu rows are.
        float sy = crisp_lo(r1.y + ImGui::GetStyle().ItemSpacing.y * 0.5f);
        dl->AddLineH(crisp_lo(p0.x + 4.0f), crisp_hi(r1.x - 4.0f), sy,
                     ox_is_light() ? OxCol(lerp4(g_body_color, g_text_color, 0.35f), 90)
                                   : ox_rim_neutral(55),
                     1.0f / fb_scale());
    }
    return clicked;
}

// ---------- status ----------

bool ui_pointer_over_window() { return g_cursor_in_window; }

// `sides` is a bitmask of the clickers that map this slot: 1 = left, 2 = right, 3 = both.
// The accent fills the side that owns the slot and fades to the plain button face on the other
// side, so a left-only slot reads as left-owned and a right-only one as right-owned. A slot
// both clickers share is accent across its whole width, as before.
void oxygen_slot_chip(ImVec2 mn, ImVec2 mx, const char* label, int sides, bool current) {
    const float R = 3.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Same face/rim language as the preset tiles: neutral button gradient when unmapped,
    // accent fill when the slot is part of a clicker's mapping. Only the selected slot gets
    // the full-strength face; the other mapped slots stay translucent so the active one wins.
    const bool mapped = sides != 0;
    const ImVec4 face = ox_is_light() ? mulc(g_accent_color, 0.80f) : g_accent_color;
    const ImU32 acc_t = OxCol(face, current ? 255 : 150);
    const ImU32 acc_b = OxCol(OxDark(face), current ? 255 : 120);
    const ImU32 neu_t = ox_fill_top(), neu_b = ox_fill_bot();

    // The non-owning side is the plain button face, not a faint accent wash: the previous
    // translucent tint was too close to the neutral fill to read as "left" or "right".
    ImU32 tl, tr, bl, br;
    if (sides == 1)      { tl = acc_t; bl = acc_b; tr = neu_t; br = neu_b; }
    else if (sides == 2) { tl = neu_t; bl = neu_b; tr = acc_t; br = acc_b; }
    else if (sides == 3) { tl = acc_t; bl = acc_b; tr = acc_t; br = acc_b; }
    else                 { tl = neu_t; bl = neu_b; tr = neu_t; br = neu_b; }

    const ImU32 bc = current ? ox_accent_rim()
                             : (mapped ? OxCol(face, 150) : ox_rim_neutral(150));
    const int gloss = ox_is_light() ? (current ? 26 : 20) : (current ? 70 : 50);
    const int shadow = ox_is_light() ? 22 : 40;
    OxFrameGrad4(dl, mn, mx, R, tl, tr, bl, br, bc, gloss, shadow);

    ImU32 ink;
    if (mapped) {
        // A chip both clickers map is accent across its whole width, so it needs ink picked
        // from the accent. A one-sided chip has the button face on the other half, so normal
        // text is the readable choice over the middle.
        if (current && sides == 3) {
            ink = ox_ink_on(acc_t);
            if (ox_is_light()) ink = IM_COL32(255, 255, 255, 255);
        } else {
            ink = ImGui::GetColorU32(ImGuiCol_Text);
        }
    } else {
        ink = current ? ImGui::GetColorU32(ImGuiCol_Text) : ox_text_dim();
    }
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(crisp_lo(mn.x + ((mx.x - mn.x) - ts.x) * 0.5f), vcenter_top(mn.y, mx.y)),
                ink, label);
}

ImVec2 click_indicator_size(const char* label) {    char display[32]; snprintf(display, sizeof(display), "%s: %s", label, "ON");
    char alt[32];     snprintf(alt, sizeof(alt), "%s: %s", label, "off");
    // widest of both states so the pill never resizes when toggling
    char d2[32]; snprintf(d2, sizeof(d2), "%s: %s", label, "off");
    char a2[32]; snprintf(a2, sizeof(a2), "%s: %s", label, "ON");
    ImVec2 ls = ImGui::CalcTextSize(display);
    ImVec2 ls_alt = ImGui::CalcTextSize(alt);
    ImVec2 l2 = ImGui::CalcTextSize(d2);
    ImVec2 a3 = ImGui::CalcTextSize(a2);
    float tw = ls.x; if (ls_alt.x > tw) tw = ls_alt.x; if (l2.x > tw) tw = l2.x; if (a3.x > tw) tw = a3.x;
    return ImVec2(tw + 24.0f, ls.y + 10.0f);
}

bool click_indicator(const char* label, bool on, bool paused) {
    char display[32];
    snprintf(display, sizeof(display), "%s: %s", label, on ? "ON" : "off");
    ImVec2 sz = click_indicator_size(label);
    ImVec2 pad(12, 5);
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 mx(mn.x + sz.x, mn.y + sz.y);
    const float r = OXY_R;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // The pill doubles as the master on/off switch for that clicker.
    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##pill", sz);
    const bool hov = ImGui::IsItemHovered();
    float t_h = widget_anim(ImGui::GetID("##pill"), hov);
    ImGui::PopID();
    const bool L = ox_is_light();
    // Amber means "on but held back": an open game screen or the pointer on this window. The
    // same colour as the paused dot on the cards, so the top bar and the card agree.
    const bool amber = on && paused;
    // Dark-theme fills; the light theme draws a vertical ramp in both states (a flat pale
    // fill plus a white gloss reads as "no gradient" on an already light background).
    const ImU32 fill_on  = amber ? IM_COL32(190, 136, 34, 235) : IM_COL32(46, 148, 84, 235);
    const ImU32 rim_on   = amber ? (L ? IM_COL32(104, 70, 10, 230) : IM_COL32(248, 212, 132, 165))
                                 : (L ? IM_COL32(24, 86, 48, 230) : IM_COL32(150, 230, 180, 160));
    const ImU32 text_on  = amber ? IM_COL32(255, 248, 228, 255)
                                 : (L ? IM_COL32(245, 255, 248, 255) : IM_COL32(235, 255, 242, 255));
    const ImU32 rim_off  = L ? OxCol(lerp4(g_body_color, g_text_color, 0.42f), 200) : IM_COL32(255, 255, 255, 45);
    const ImU32 text_off = L ? OxCol(lerp4(g_body_color, g_text_color, 0.68f)) : IM_COL32(200, 212, 228, 150);
    ImVec2 cmn = crisp_min(mn), cmx = crisp_max(mx);
    if (L) {
        ImU32 top_c = on ? (amber ? IM_COL32(226, 176, 66, 255) : IM_COL32(80, 186, 120, 255))
                         : OxCol(ox_tint(g_body_color, 0.80f));
        ImU32 bot_c = on ? (amber ? IM_COL32(162, 108, 16, 255) : IM_COL32(32, 118, 62, 255))
                         : OxCol(ox_tint(g_body_color, 0.14f));
        AddRoundedRectFilledGradV(dl, cmn, cmx, top_c, bot_c, r);
        glass_gloss(dl, cmn, cmx, r, cmn.y + sz.y * 0.55f, on ? 84 : 64);
    } else if (on) {
        dl->AddRectFilled(cmn, cmx, fill_on, r);
        glass_gloss(dl, cmn, cmx, r, cmn.y + sz.y * 0.55f, 70);
    } else {
        // White-at-low-alpha over a near-black panel; a single flat alpha (no gloss) read as a
        // dead grey slab, so ramp it top->bottom like the other pills.
        AddRoundedRectFilledGradV(dl, cmn, cmx, IM_COL32(255, 255, 255, 34),
                                  IM_COL32(255, 255, 255, 6), r);
        glass_gloss(dl, cmn, cmx, r, cmn.y + sz.y * 0.55f, 42);
    }
    if (t_h > 0.01f)
        dl->AddRectFilled(cmn, cmx, IM_COL32(255, 255, 255, (int)((L ? 26.0f : 20.0f) * t_h)), r);
    ImU32 rim = on ? rim_on : rim_off;
    if (t_h > 0.01f) rim = lerp_u32(rim, ox_accent_rim(), t_h);
    ox_outline(dl, cmn, cmx, r, rim);
    dl->AddText(ImVec2(mn.x + pad.x, vcenter_top(mn.y, mx.y)), on ? text_on : text_off, display);
    return clicked;
}

void stat_readout(bool on, float cps, uint32_t clicks, float inner_w, const char* note) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = g_font_big ? g_font_big : ImGui::GetFont();   // real 30px atlas font = crisp
    float big = font->LegacySize;
    ImVec2 mn = ImGui::GetCursorScreenPos();
    // "0.0 CPS" while the clicker is deliberately idle (cursor on AeroClicker, Mouse mode
    // waiting for the button) reads as broken, so say why instead of showing an empty rate.
    char main_txt[32];
    if (on && note) snprintf(main_txt, sizeof(main_txt), "Paused");
    else if (on)    snprintf(main_txt, sizeof(main_txt), "%.1f CPS", cps);
    else            snprintf(main_txt, sizeof(main_txt), "Idle");
    char sub_txt[64];
    if (on && note) snprintf(sub_txt, sizeof(sub_txt), "%s", note);
    else            snprintf(sub_txt, sizeof(sub_txt), "%u clicks", clicks);
    // OxLight(accent) is a 1.3x clamp: on a light theme that turns the big readout into pale
    // cyan on off-white. Use the deeper accent there, and real disabled text instead of the
    // fixed 35%/43% alphas that made "Idle"/"N clicks" nearly vanish.
    ImU32 main_col = on ? ox_accent_text()
                        : (ox_is_light() ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                                         : OxCol(g_text_color, 90));
    dl->AddText(font, big, mn, main_col, main_txt);
    ImVec2 ms = font->CalcTextSizeA(big, FLT_MAX, 0.0f, main_txt, nullptr, nullptr);
    ImVec2 sp(mn.x, mn.y + ms.y + 4.0f);
    dl->AddText(sp, ox_text_dim(), sub_txt);
    ImVec2 ss = ImGui::CalcTextSize(sub_txt);
    ImGui::Dummy(ImVec2(inner_w, ms.y + 4.0f + ss.y));
}

void segmented_tabs(const char* const* names, int count, int& current_tab, const OxIcon* icons,
                    float max_width) {
    const float R = OXY_R;
    const float PR = R - 1.0f;
    const float h = ImGui::GetFrameHeight();
    // Symbolic glyphs sit a touch smaller than the line box, matching the 16px panel icons.
    // Oxygen symbolic icons are designed on a 16px grid; drawing them at exactly that
    // size keeps every pixel of the baked mask on screen without resampling.
    constexpr float isz = 16.0f;
    const float igap = 6.0f;
    const ImU32 track = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 rim = ox_rim_neutral(150);
    const ImU32 txt = ImGui::GetColorU32(ImGuiCol_Text, 0.80f);
    const ImU32 txtSel = ox_ink_fill(g_accent_active, 0.12f);
    ImVec2 ls[8]; float w[8]; float total = 0.0f;
    float pad_x = 14.0f;
    bool show_icons = true;
    auto measure = [&]() {
        total = 0.0f;
        for (int i = 0; i < count && i < 8; i++) {
            ls[i] = ImGui::CalcTextSize(names[i]);
            const bool has_icon = show_icons && icons && icons[i] != OxIcon::None;
            w[i] = ls[i].x + pad_x * 2.0f + (has_icon ? isz + igap : 0.0f);
            total += w[i];
        }
    };
    // The bar shares its row with the L/R pills, so a narrow window has to give ground: the
    // padding tightens first, and only when no padding fits do the icons go. Taking the
    // roomiest variant that fits (rather than the first that fits) keeps the bar from jumping
    // to its narrowest form while there is still space to spare. If nothing fits it is clipped
    // — it must never run underneath the pills.
    if (max_width > 0.0f) {
        static const float kPads[] = { 14.0f, 12.0f, 10.0f, 8.0f, 6.0f };
        bool found = false;
        for (int pass = 0; pass < 2 && !found; pass++) {
            show_icons = (pass == 0);
            for (float p : kPads) {
                pad_x = p;
                measure();
                if (total <= max_width) { found = true; break; }
            }
        }
        if (!found) { show_icons = false; pad_x = 6.0f; measure(); }
    } else {
        measure();
    }
    const float bar_w = (max_width > 0.0f && total > max_width) ? max_width : total;
    const bool clipped = bar_w < total - 0.5f;
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImVec2 mx(mn.x + bar_w, mn.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 smn = crisp_min(mn), smx = crisp_max(mx);
    // Tab bar: a real vertical ramp like the other raised chrome, instead of a flat slab.
    // On the light theme the stock FrameBg is already near-white, so blending it toward white
    // produced almost no visible change; build the ramp from the window body instead.
    // (Dark themes keep the old values because their base is nearly black.)
    const ImU32 track_top = ox_is_light() ? OxCol(ox_tint(g_body_color, 0.92f))
                                          : lerp_u32(track, IM_COL32(255, 255, 255, 255), 0.06f);
    const ImU32 track_bot = ox_is_light() ? OxCol(ox_tint(g_body_color, 0.10f))
                                          : lerp_u32(track, IM_COL32(0, 0, 0, 255), 0.25f);
    AddRoundedRectFilledGradV(dl, smn, smx, track_top, track_bot, R);
    ox_outline(dl, mn, mx, R, rim);
    if (clipped) dl->PushClipRect(ImVec2(mn.x, mn.y - 2.0f), ImVec2(mn.x + bar_w, mx.y + 2.0f), true);
    float x = mn.x;
    for (int i = 0; i < count; i++) {
        // A tab that would start past the clip is not drawn and gets no hit area, so it cannot
        // be clicked through the pills sitting next to the bar.
        if (clipped && x >= mn.x + bar_w - 1.0f) break;
        ImGui::SetCursorScreenPos(ImVec2(x, mn.y));
        bool sel = (current_tab == i);
        ImVec2 s0(x, mn.y), s1(x + w[i], mn.y + h);
        bool clicked = ImGui::InvisibleButton(names[i], ImVec2(w[i], h));
        bool hov = ImGui::IsItemHovered();
        ImGuiID wid = ImGui::GetID(names[i]);
        float t_h = widget_anim(wid, hov);
        float t_s = widget_anim(wid + 1, sel);
        ImVec2 i0 = crisp_min(ImVec2(s0.x + 2, s0.y + 2));
        ImVec2 i1 = crisp_max(ImVec2(s1.x - 2, s1.y - 2));
        if (t_s > 0.01f) {
            // Selected tab: real vertical ramp (accent -> accent_active) under the gloss,
            // so it matches the gradient treatment of the other accent surfaces.
            const ImU32 sel_top = lerp_u32(IM_COL32(0, 0, 0, 0), OxCol(g_accent_color), t_s);
            const ImU32 sel_bot = lerp_u32(IM_COL32(0, 0, 0, 0), OxCol(g_accent_active), t_s);
            AddRoundedRectFilledGradV(dl, i0, i1, sel_top, sel_bot, PR);
            glass_gloss(dl, i0, i1, PR, i0.y + (i1.y - i0.y) * 0.58f, (int)(80 * t_s));
            ox_outline(dl, i0, i1, PR, lerp_u32(IM_COL32(0, 0, 0, 0), ox_accent_rim(), t_s));
        } else if (t_h > 0.01f) {
            accent_glow(dl, i0, i1, PR, t_h);
        }
        if (i > 0 && !sel && current_tab != i - 1)
            dl->AddLineV(crisp_lo(s0.x), s0.y + 5, s1.y - 5, rim, 1.0f);
        const bool has_icon = show_icons && icons && icons[i] != OxIcon::None;
        const float content_w = ls[i].x + (has_icon ? isz + igap : 0.0f);
        const float cx = s0.x + (w[i] - content_w) * 0.5f;
        const ImU32 ink = sel ? txtSel : txt;
        if (has_icon)
            ox_draw_icon(dl, icons[i], ImVec2(cx + isz * 0.5f, (s0.y + s1.y) * 0.5f), isz, ink);
        dl->AddText(ImVec2(cx + (has_icon ? isz + igap : 0.0f), vcenter_top(s0.y, s1.y)),
                    ink, names[i]);
        if (clicked) current_tab = i;
        x += w[i];
    }
    if (clipped) dl->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(mn.x, mx.y));
    ImGui::Spacing();
}
