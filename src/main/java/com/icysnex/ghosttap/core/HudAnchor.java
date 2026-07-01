package com.icysnex.ghosttap.core;

// Where the HUD sits. The corner anchors follow the screen edges (so they stay
// put when the window resizes); MANUAL uses a free X/Y position.
public enum HudAnchor {

    TOP_LEFT("TL"),
    TOP_RIGHT("TR"),
    BOTTOM_LEFT("BL"),
    BOTTOM_RIGHT("BR"),
    MANUAL("Manual");

    public final String label;

    HudAnchor(String label) {
        this.label = label;
    }
}
