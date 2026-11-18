#include "gui_icons.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdint>

namespace {

// 16px masks baked from the Oxygen and Breeze symbolic sets (see gui_icons_symbolic.inc).
#include "gui_icons_symbolic.inc"

constexpr int kMaskSize = 16;
// One transparent texel around every mask. Without it, sampling just outside the icon rect
// (which happens when the quad lands on a fractional framebuffer pixel, e.g. 125 % scaling)
// picks the neighbouring atlas tile or clamps to the icon's own edge row, drawing a stray
// line along the top or left of the glyph.
constexpr int kGutter = 1;
constexpr int kCell = kMaskSize + kGutter * 2;
constexpr int kAtlasCols = 8;
constexpr int kAtlasRows = 4;

struct MaskSlot { int col = -1; int row = -1; };
MaskSlot g_mask_slots[(int)OxIcon::Count];
GLuint g_mask_tex = 0;

// White RGB + symbolic alpha, tinted by the vertex colour at draw time. One atlas keeps
// the whole set in a single ImGui draw command.
void ox_build_atlas() {
    constexpr int W = kAtlasCols * kCell;
    constexpr int H = kAtlasRows * kCell;
    static_assert((int)OxIcon::Count <= kAtlasCols * kAtlasRows, "atlas too small");
    unsigned char pixels[W * H * 4] = {};
    int slot = 0;
    for (const OxMaskEntry& m : kOxMasks) {
        const int cx = slot % kAtlasCols;
        const int cy = slot / kAtlasCols;
        g_mask_slots[(int)m.icon] = MaskSlot{cx, cy};
        for (int y = 0; y < kMaskSize; y++) {
            for (int x = 0; x < kMaskSize; x++) {
                const int px = cx * kCell + kGutter + x;
                const int py = cy * kCell + kGutter + y;
                const int i = (py * W + px) * 4;
                pixels[i + 0] = 255;
                pixels[i + 1] = 255;
                pixels[i + 2] = 255;
                pixels[i + 3] = m.px[y * kMaskSize + x];
            }
        }
        slot++;
    }
    glGenTextures(1, &g_mask_tex);
    glBindTexture(GL_TEXTURE_2D, g_mask_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

} // namespace

void ox_draw_icon(ImDrawList* dl, OxIcon icon, ImVec2 center, float size, ImU32 col) {
    if (!dl || icon == OxIcon::None || size <= 0.0f) return;
    if (g_mask_tex == 0) ox_build_atlas();
    const MaskSlot& s = g_mask_slots[(int)icon];
    if (s.col < 0) return;
    constexpr float AW = (float)(kAtlasCols * kCell);
    constexpr float AH = (float)(kAtlasRows * kCell);
    const float u0 = (float)(s.col * kCell + kGutter) / AW;
    const float v0 = (float)(s.row * kCell + kGutter) / AH;
    const float u1 = (float)(s.col * kCell + kGutter + kMaskSize) / AW;
    const float v1 = (float)(s.row * kCell + kGutter + kMaskSize) / AH;
    // Snap to the physical pixel grid rather than the logical one: at 125 % a whole logical
    // pixel is 1.25 device pixels, so the quad edges would otherwise straddle texels even
    // with the transparent gutter above.
    const float s_px = ImGui::GetIO().DisplayFramebufferScale.x;
    const float d = s_px > 0.0f ? s_px : 1.0f;
    const float x0 = std::floor((center.x - size * 0.5f) * d) / d;
    const float y0 = std::floor((center.y - size * 0.5f) * d) / d;
    dl->AddImage((ImTextureID)(intptr_t)g_mask_tex, ImVec2(x0, y0), ImVec2(x0 + size, y0 + size),
                 ImVec2(u0, v0), ImVec2(u1, v1), col);
}
