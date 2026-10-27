#include "gui_icons.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdint>

namespace {

// 16px masks baked from the Oxygen and Breeze symbolic sets by tools/gen_symbolic_icons.py.
#include "gui_icons_symbolic.inc"

constexpr int kMaskSize = 16;
constexpr int kAtlasCols = 8;
constexpr int kAtlasRows = 4;

struct MaskSlot { int col = -1; int row = -1; };
MaskSlot g_mask_slots[(int)OxIcon::Count];
GLuint g_mask_tex = 0;

// White RGB + symbolic alpha, tinted by the vertex colour at draw time. One atlas keeps
// the whole set in a single ImGui draw command.
void ox_build_atlas() {
    constexpr int W = kAtlasCols * kMaskSize;
    constexpr int H = kAtlasRows * kMaskSize;
    static_assert((int)OxIcon::Count <= kAtlasCols * kAtlasRows, "atlas too small");
    unsigned char pixels[W * H * 4] = {};
    int slot = 0;
    for (const OxMaskEntry& m : kOxMasks) {
        const int cx = slot % kAtlasCols;
        const int cy = slot / kAtlasCols;
        g_mask_slots[(int)m.icon] = MaskSlot{cx, cy};
        for (int y = 0; y < kMaskSize; y++) {
            for (int x = 0; x < kMaskSize; x++) {
                const int i = ((cy * kMaskSize + y) * W + cx * kMaskSize + x) * 4;
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
    constexpr float AW = (float)(kAtlasCols * kMaskSize);
    constexpr float AH = (float)(kAtlasRows * kMaskSize);
    const float u0 = (float)(s.col * kMaskSize) / AW;
    const float v0 = (float)(s.row * kMaskSize) / AH;
    const float u1 = (float)((s.col + 1) * kMaskSize) / AW;
    const float v1 = (float)((s.row + 1) * kMaskSize) / AH;
    // Whole-pixel placement: the mask has exactly 16 source texels, so any sub-pixel offset
    // would turn the 1px detail into grey mush.
    const float x0 = std::floor(center.x - size * 0.5f + 0.5f);
    const float y0 = std::floor(center.y - size * 0.5f + 0.5f);
    dl->AddImage((ImTextureID)(intptr_t)g_mask_tex, ImVec2(x0, y0), ImVec2(x0 + size, y0 + size),
                 ImVec2(u0, v0), ImVec2(u1, v1), col);
}
