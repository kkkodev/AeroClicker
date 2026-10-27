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

ImVec4 ox_tint(ImVec4 c, float t) {
    if (t >= 0.0f)
        return ImVec4(c.x + (1.0f - c.x) * t, c.y + (1.0f - c.y) * t,
                      c.z + (1.0f - c.z) * t, c.w);
    float d = 1.0f + t;
    return ImVec4(c.x * d, c.y * d, c.z * d, c.w);
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

ImVec4 g_tb_edge_focused   = ImVec4(0, 0, 0, 0);
ImVec4 g_tb_edge_unfocused = ImVec4(0, 0, 0, 0);
bool   g_tb_edge_valid     = false;

// Only the titlebar's *lower edge* colour is read, never its gradient: Oxygen paints a radial
// highlight that peaks mid-titlebar, and that shape is not exposed to the client, so anything
// drawn in here could only approximate it. The edge colour is knowable though, and starting
// the body on it is all a KDE app does — the decoration keeps owning the gradient.
void load_kde_titlebar_edges() {
    float r, g, b;
    g_tb_edge_valid = false;
    if (!read_kde_color("WM", "activeBackground", r, g, b)) return;
    g_tb_edge_focused = ImVec4(r, g, b, 1.0f);
    g_tb_edge_valid   = true;

    // Focused, Oxygen's gradient ends on activeBackground. Unfocused it runs
    // inactiveBackground -> inactiveBlend, so the edge is inactiveBlend; inactiveBlend=0,0,0
    // is KDE's "no blend" sentinel, in which case the titlebar is flat inactiveBackground.
    float ir, ig, ib, br, bg, bb;
    const bool have_bg  = read_kde_color("WM", "inactiveBackground", ir, ig, ib);
    const bool have_bl  = read_kde_color("WM", "inactiveBlend", br, bg, bb);
    if (have_bl && br + bg + bb > 0.0f)   g_tb_edge_unfocused = ImVec4(br, bg, bb, 1.0f);
    else if (have_bg)                     g_tb_edge_unfocused = ImVec4(ir, ig, ib, 1.0f);
    else                                  g_tb_edge_unfocused = g_tb_edge_focused;
}

static bool read_kde_accent_color(float &r, float &g, float &b) {
    return read_kde_color("Colors:Selection", "BackgroundNormal", r, g, b);
}

// ---------------------------------------------------------------------------
// Oxygen window background
//
// Ported from KDE's KColorSpaces::KHCY, KColorUtils::shade/darken, KColorScheme::shade and
// Oxygen's Helper::renderWindowBackground. Working in KHCY (gamma 2.2 luma) matters: a
// plain sRGB blend of the same colours gives a visibly different top shade.
// ---------------------------------------------------------------------------

static float kclamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static float kgamma(float v) { return powf(kclamp01(v), 2.2f); }
static float kigamma(float v) { return powf(kclamp01(v), 1.0f / 2.2f); }

struct Khcy { float h, c, y; };
static const float k_yc[3] = { 0.2126f, 0.7152f, 0.0722f };

static Khcy khcy_from(const ImVec4& col) {
    Khcy out;
    const float r = kgamma(col.x), g = kgamma(col.y), b = kgamma(col.z);
    out.y = r * k_yc[0] + g * k_yc[1] + b * k_yc[2];
    const float p = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const float n = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const float d = 6.0f * (p - n);
    if (n == p)          out.h = 0.0f;
    else if (r == p)     out.h = (g - b) / d;
    else if (g == p)     out.h = (b - r) / d + 1.0f / 3.0f;
    else                 out.h = (r - g) / d + 2.0f / 3.0f;
    if (r == g && g == b) out.c = 0.0f;
    else if (out.y <= 0.0f) out.c = 0.0f;
    else out.c = fmaxf((out.y - n) / out.y, (p - out.y) / (1.0f - out.y));
    return out;
}

static ImVec4 khcy_to_rgb(const Khcy& in) {
    float h = in.h - floorf(in.h);
    float c = kclamp01(in.c);
    float y = kclamp01(in.y);
    const float hs = h * 6.0f;
    float th, tm;
    if (hs < 1.0f)      { th = hs;       tm = k_yc[0] + k_yc[1] * th; }
    else if (hs < 2.0f) { th = 2.0f - hs; tm = k_yc[1] + k_yc[0] * th; }
    else if (hs < 3.0f) { th = hs - 2.0f; tm = k_yc[1] + k_yc[2] * th; }
    else if (hs < 4.0f) { th = 4.0f - hs; tm = k_yc[2] + k_yc[1] * th; }
    else if (hs < 5.0f) { th = hs - 4.0f; tm = k_yc[2] + k_yc[0] * th; }
    else                { th = 6.0f - hs; tm = k_yc[0] + k_yc[2] * th; }

    float tp, to, tn;
    if (tm >= y) {
        const float den = tm > 0.0f ? tm : 1e-4f;
        tp = y + y * c * (1.0f - tm) / den;
        to = y + y * c * (th - tm) / den;
        tn = y - (y * c);
    } else {
        const float den = (1.0f - tm) > 0.0f ? (1.0f - tm) : 1e-4f;
        tp = y + (1.0f - y) * c;
        to = y + (1.0f - y) * c * (th - tm) / den;
        tn = y - (1.0f - y) * c * tm / den;
    }
    float r, g, b;
    if (hs < 1.0f)      { r = tp; g = to; b = tn; }
    else if (hs < 2.0f) { r = to; g = tp; b = tn; }
    else if (hs < 3.0f) { r = tn; g = tp; b = to; }
    else if (hs < 4.0f) { r = tn; g = to; b = tp; }
    else if (hs < 5.0f) { r = to; g = tn; b = tp; }
    else                { r = tp; g = tn; b = to; }
    return ImVec4(kigamma(r), kigamma(g), kigamma(b), 1.0f);
}

// KColorUtils::shade/darken operate on luma and chroma, not RGB.
static ImVec4 kshade(const ImVec4& col, float ky, float kc = 0.0f) {
    Khcy c = khcy_from(col);
    c.y = kclamp01(c.y + ky);
    c.c = kclamp01(c.c + kc);
    return khcy_to_rgb(c);
}
static ImVec4 kdarken(const ImVec4& col, float ky, float kc = 1.0f) {
    Khcy c = khcy_from(col);
    c.y = kclamp01(c.y * (1.0f - ky));
    c.c = kclamp01(c.c * kc);
    return khcy_to_rgb(c);
}

enum ShadeRole { ShadeLight, ShadeMidlight, ShadeMid, ShadeDark, ShadeShadow };

// KColorScheme::shade(color, role, contrast)
static ImVec4 scheme_shade(const ImVec4& col, ShadeRole role, float contrast) {
    contrast = contrast < -1.0f ? -1.0f : (contrast > 1.0f ? 1.0f : contrast);
    const float y  = khcy_from(col).y;
    const float yi = 1.0f - y;

    if (y < 0.006f) {
        switch (role) {
        case ShadeLight:  return kshade(col, 0.05f + 0.95f * contrast);
        case ShadeMid:    return kshade(col, 0.01f + 0.20f * contrast);
        case ShadeDark:   return kshade(col, 0.02f + 0.40f * contrast);
        default:          return kshade(col, 0.03f + 0.60f * contrast);
        }
    }
    if (y > 0.93f) {
        switch (role) {
        case ShadeMidlight: return kshade(col, -0.02f - 0.20f * contrast);
        case ShadeDark:     return kshade(col, -0.06f - 0.60f * contrast);
        case ShadeShadow:   return kshade(col, -0.10f - 0.90f * contrast);
        default:            return kshade(col, -0.04f - 0.40f * contrast);
        }
    }

    const float light_amount = (0.05f + y * 0.55f) * (0.25f + contrast * 0.75f);
    const float dark_amount  = (-y) * (0.55f + contrast * 0.35f);
    switch (role) {
    case ShadeLight:    return kshade(col, light_amount);
    case ShadeMidlight: return kshade(col, (0.15f + 0.35f * yi) * light_amount);
    case ShadeMid:      return kshade(col, (0.35f + 0.15f * y) * dark_amount);
    case ShadeDark:     return kshade(col, dark_amount);
    default:            return kdarken(kshade(col, dark_amount), 0.5f + 0.3f * y);
    }
}

static bool oxygen_low_threshold(const ImVec4& c) {
    return khcy_from(scheme_shade(c, ShadeMid, 0.5f)).y > khcy_from(c).y;
}
static bool oxygen_high_threshold(const ImVec4& c) {
    return khcy_from(scheme_shade(c, ShadeLight, 0.5f)).y < khcy_from(c).y;
}

// Contrast used for the window background. This deliberately does NOT read [KDE] contrast
// from the colour scheme: the Oxygen decoration and widget style build their Helper from
// oxygenrc (KSharedConfig::openConfig("oxygenrc")), which has no [KDE] group, so KColorScheme
// falls back to its default contrast of 7 (0.7) no matter what the scheme says. The background
// shades must use the same value or the client gradient will not line up with the titlebar.
// This is exactly why Oxygen schemes (which happen to set contrast=7) merged perfectly while
// schemes such as Breeze Dark (contrast=4) showed a step at the seam.
static const float OXYGEN_CONTRAST = 0.7f;

static ImVec4 oxygen_bg_top(const ImVec4& c, float bgc) {
    if (oxygen_low_threshold(c))
        return scheme_shade(c, ShadeMidlight, 0.0f);
    const float my = khcy_from(scheme_shade(c, ShadeLight, 0.0f)).y;
    const float by = khcy_from(c).y;
    return kshade(c, (my - by) * bgc);
}

static ImVec4 oxygen_bg_bottom(const ImVec4& c, float bgc) {
    const ImVec4 mid = scheme_shade(c, ShadeMid, 0.0f);
    if (oxygen_low_threshold(c))
        return mid;
    const float by = khcy_from(c).y;
    const float my = khcy_from(mid).y;
    return kshade(c, (my - by) * bgc);
}

static ImVec4 oxygen_bg_radial(const ImVec4& c, float bgc) {
    if (oxygen_low_threshold(c))
        return scheme_shade(c, ShadeLight, 0.0f);
    if (oxygen_high_threshold(c))
        return c;
    return scheme_shade(c, ShadeLight, bgc);
}

OxygenBackground oxygen_background(float client_h) {
    const float contrast = OXYGEN_CONTRAST;
    const float bgc = fminf(1.0f, 0.9f * contrast / 0.7f);

    OxygenBackground bg;
    bg.base    = g_body_color;
    bg.top     = oxygen_bg_top(bg.base, bgc);
    bg.bottom  = oxygen_bg_bottom(bg.base, bgc);
    bg.radial  = oxygen_bg_radial(bg.base, bgc);
    bg.y_shift = -23.0f;   // Oxygen style default: client gradient joins a 23px titlebar
    // The style derives splitY from the *client* rect height, so the result is independent
    // of the decoration's real titlebar height (which Wayland does not expose).
    bg.split_y = fminf(300.0f, 3.0f * client_h / 4.0f);
    return bg;
}

static ImU32 oxygen_rgba(const ImVec4& c, int a) {
    return IM_COL32((int)(kclamp01(c.x) * 255.0f + 0.5f),
                    (int)(kclamp01(c.y) * 255.0f + 0.5f),
                    (int)(kclamp01(c.z) * 255.0f + 0.5f), a);
}

static ImVec4 oxygen_mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, 1.0f);
}

void draw_oxygen_background(ImDrawList* dl, float w, float h) {
    if (w < 1.0f || h < 1.0f) return;
    const OxygenBackground bg = oxygen_background(h);
    const float ys    = bg.y_shift;
    const float split = bg.split_y > 1.0f ? bg.split_y : 1.0f;

    // Colour of the vertical gradient at client row y. Above y_shift Qt's QLinearGradient
    // pad-spreads the first stop, and from y_shift to y_shift+split_y it runs
    // top -> base (at 50%) -> bottom.
    auto grad_at = [&](float y) {
        const float t = (y - ys) / split;
        if (t <= 0.0f) return bg.top;
        if (t < 0.5f)  return oxygen_mix(bg.top, bg.base, t * 2.0f);
        if (t < 1.0f)  return oxygen_mix(bg.base, bg.bottom, (t - 0.5f) * 2.0f);
        return bg.bottom;
    };

    const float y_mid = ys + split * 0.5f;
    const float y_end = ys + split;

    // Upper half of the gradient, then the lower half. Two-stop quads reproduce Qt's linear
    // interpolation exactly; clamping to h handles very short windows.
    const float mid = y_mid < h ? y_mid : h;
    if (mid > 0.0f) {
        const ImVec4 c0 = grad_at(0.0f);
        const ImVec4 c1 = grad_at(mid);
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, mid),
                                    oxygen_rgba(c0, 255), oxygen_rgba(c0, 255),
                                    oxygen_rgba(c1, 255), oxygen_rgba(c1, 255));
    }
    if (h > y_mid) {
        const float bot = y_end < h ? y_end : h;
        const ImVec4 c1 = grad_at(y_mid);
        const ImVec4 c2 = grad_at(bot);
        dl->AddRectFilledMultiColor(ImVec2(0, y_mid), ImVec2(w, bot),
                                    oxygen_rgba(c1, 255), oxygen_rgba(c1, 255),
                                    oxygen_rgba(c2, 255), oxygen_rgba(c2, 255));
        if (h > y_end) dl->AddRectFilled(ImVec2(0, y_end), ImVec2(w, h), oxygen_rgba(bg.bottom, 255));
    }

    // Wide radial highlight at the top centre. Qt draws a 128 x (64+yShift) tile stretched
    // to min(600, width), so the highlight is an ellipse: rx = width/2, ry = 64, centred
    // 23px above the client top (i.e. exactly where the titlebar's own highlight is).
    const float radw = fminf(600.0f, w);
    const float radh = fminf(64.0f + ys, h);
    const float rx = radw * 0.5f;
    const float ry = 64.0f;
    if (radw > 1.0f && radh > 0.0f && rx > 0.0f) {
        const float x0 = (w - radw) * 0.5f;
        const float cy = ys;
        auto alpha_at = [&](float px, float py) -> int {
            const float dx = (px - (x0 + rx)) / rx;
            const float dy = (py - cy) / ry;
            const float r = sqrtf(dx * dx + dy * dy);
            float a;
            if (r <= 0.5f)       a = 255.0f + (101.0f - 255.0f) * (r / 0.5f);
            else if (r <= 0.75f) a = 101.0f + (37.0f - 101.0f) * ((r - 0.5f) / 0.25f);
            else if (r < 1.0f)   a = 37.0f * (1.0f - (r - 0.75f) / 0.25f);
            else                 a = 0.0f;
            return (int)(a + 0.5f);
        };
        // A coarse vertex grid is enough: the highlight is a soft 41px-tall glow and the
        // Gouraud interpolation between vertices is visually indistinguishable from the
        // per-pixel radial. Keeping it on the draw list avoids a texture + GL state.
        const int cols = 32;
        const int rows = 8;
        int va[9][33];
        for (int iy = 0; iy <= rows; iy++) {
            const float py = radh * (float)iy / (float)rows;
            for (int ix = 0; ix <= cols; ix++) {
                const float px = x0 + radw * (float)ix / (float)cols;
                va[iy][ix] = alpha_at(px, py);
            }
        }
        for (int iy = 0; iy < rows; iy++) {
            const float ya = radh * (float)iy / (float)rows;
            const float yb = radh * (float)(iy + 1) / (float)rows;
            for (int ix = 0; ix < cols; ix++) {
                const int a00 = va[iy][ix], a10 = va[iy][ix + 1];
                const int a01 = va[iy + 1][ix], a11 = va[iy + 1][ix + 1];
                if ((a00 | a10 | a01 | a11) == 0) continue;
                const float xa = x0 + radw * (float)ix / (float)cols;
                const float xb = x0 + radw * (float)(ix + 1) / (float)cols;
                dl->AddRectFilledMultiColor(
                    ImVec2(xa, ya), ImVec2(xb, yb),
                    oxygen_rgba(bg.radial, a00), oxygen_rgba(bg.radial, a10),
                    oxygen_rgba(bg.radial, a11), oxygen_rgba(bg.radial, a01));
            }
        }
    }
}


void setup_oxygen_theme(int preset, float custom_r, float custom_g, float custom_b) {
    g_light_theme = ox_luma(g_body_color) > 0.45f;
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();   // reset: repeated calls (color picker drag) must not stack ScaleAllSizes
    // Preset order matches the dropdown and the value saved in config.accent_preset.
    if (preset == 0) {
        float kr, kg, kb;   // Follow KDE, blue when the scheme cannot be read
        if (read_kde_accent_color(kr, kg, kb))
            g_accent_color = ImVec4(kr, kg, kb, 1.0f);
        else
            g_accent_color = ImVec4(0.24f, 0.49f, 0.78f, 1.0f);
    } else if (preset == 1) {
        g_accent_color = ImVec4(0.24f, 0.49f, 0.78f, 1.0f);   // Blue
    } else if (preset == 2) {
        g_accent_color = ImVec4(0.80f, 0.20f, 0.20f, 1.0f);   // Red
    } else if (preset == 3) {
        g_accent_color = ImVec4(0.20f, 0.80f, 0.30f, 1.0f);   // Green
    } else if (preset == 4) {
        g_accent_color = ImVec4(0.60f, 0.30f, 0.90f, 1.0f);   // Purple
    } else if (preset == 5) {
        g_accent_color = ImVec4(0.93f, 0.53f, 0.13f, 1.0f);   // Orange
    } else {
        g_accent_color = ImVec4(custom_r, custom_g, custom_b, 1.0f);   // Custom
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
    // InputText caret: 1.93 defaults it to ImGuiCol_Text (white). On the light theme that is
    // a white blinking bar inside a white field.
    colors[ImGuiCol_InputTextCursor]        = L ? g_text_color : ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
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
    style.PopupBorderSize       = 0.0f;
    style.FrameBorderSize       = 0.0f;
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
