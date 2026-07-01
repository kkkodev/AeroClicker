package com.icysnex.ghosttap.core;

// How a clicker is activated by its key.
//   TOGGLE - press the key to switch clicking on/off.
//   HOLD   - clicks only while the key is held.
//   MOUSE  - press the key to arm; while armed, holding the real mouse button
//            autoclicks (the real hold is masked so it becomes clicks).
public enum ActivationMode {

    TOGGLE("Toggle"),
    HOLD("Hold"),
    MOUSE("Mouse");

    public final String label;

    ActivationMode(String label) {
        this.label = label;
    }
}
