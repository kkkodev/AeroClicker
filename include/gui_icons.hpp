#pragma once
#include "imgui.h"

// Symbolic icons from the Oxygen and Breeze 16px sets, baked into alpha masks in
// gui_icons_symbolic.inc and tinted with the theme ink when drawn, so there is no SVG
// rasteriser or icon theme dependency at runtime.
enum class OxIcon {
    None = 0,
    // navigation
    Home,
    ArrowLeft,
    ArrowRight,
    List,
    Sliders,
    Chart,
    // panels
    MouseLeft,
    MouseRight,
    Gauge,
    Person,
    Clock,
    Click,
    Pause,
    Globe,
    Checklist,
    Play,
    Keyboard,
    Grid,
    Speaker,
    Folder,
    Sun,
    Terminal,
    Pulse,
    Record,
    Export,
    // actions
    Save,
    Trash,
    Refresh,
    Stop,
    Plus,
    Close,
    Count
};

// Draw `icon` centred on `center`, inside a box of `size` pixels, tinted with `col`.
void ox_draw_icon(ImDrawList* dl, OxIcon icon, ImVec2 center, float size, ImU32 col);
