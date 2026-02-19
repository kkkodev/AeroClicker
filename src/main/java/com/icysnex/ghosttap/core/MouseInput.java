package com.icysnex.ghosttap.core;

public class MouseInput {

    private static int simulatedButton = -1;
    private static boolean simulatedState = false;

    private static boolean hasQueuedEvent = false;
    private static boolean isSimulated = false;


    private MouseInput() { }


    public static void keyDown(int button) {
        if (simulatedButton != button || simulatedState) {
            simulatedButton = button;
            simulatedState = true;
            hasQueuedEvent = true;
            isSimulated = true;
        }
    }

    public static void keyUp(int button) {
        simulatedButton = button;
        simulatedState = false;
        hasQueuedEvent = true;
        isSimulated = true;
    }

    public static void consumeEvent() {
        if (!hasQueuedEvent)
            return;

        hasQueuedEvent = false;

        if (!simulatedState) {
            simulatedButton = -1;
            isSimulated = false;
        }
    }


    public static int getButton() {
        return simulatedButton;
    }

    public static boolean getState() {
        return simulatedState;
    }

    public static boolean injectNextEvent() {
        return hasQueuedEvent && isSimulated;
    }
}
