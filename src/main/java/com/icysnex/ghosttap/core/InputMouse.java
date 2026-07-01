package com.icysnex.ghosttap.core;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicBoolean;

public abstract class InputMouse {

    public static class Event {
        public Event(byte button, byte state) {
            this.button = button;
            this.state = state;
        }
        public byte button;
        public byte state;
    }

    public static final byte BUTTON_LEFT = 0;
    public static final byte BUTTON_RIGHT = 1;

    public static final byte STATE_DOWN = 1;
    public static final byte STATE_UP = 0;


    public static final ConcurrentLinkedQueue<Event> pendingEvents = new ConcurrentLinkedQueue<>();

    public static byte spoofedLeft = STATE_UP;
    public static byte spoofedRight = STATE_UP;

    public static final AtomicBoolean pollLeftLatch = new AtomicBoolean(false);
    public static final AtomicBoolean pollRightLatch = new AtomicBoolean(false);


    public static void downLeft() {
        pendingEvents.add(new Event(BUTTON_LEFT, STATE_DOWN));
        spoofedLeft = STATE_DOWN;
        pollLeftLatch.set(true);
    }

    public static void upLeft() {
        pendingEvents.add(new Event(BUTTON_LEFT, STATE_UP));
        spoofedLeft = STATE_UP;
    }

    public static void downRight() {
        pendingEvents.add(new Event(BUTTON_RIGHT, STATE_DOWN));
        spoofedRight = STATE_DOWN;
        pollRightLatch.set(true);
    }

    public static void upRight() {
        pendingEvents.add(new Event(BUTTON_RIGHT, STATE_UP));
        spoofedRight = STATE_UP;
    }


    // Generic dispatch by button, used by the Clicker so one code path drives both.
    public static void down(byte button) {
        if (button == BUTTON_LEFT) downLeft();
        else downRight();
    }

    public static void up(byte button) {
        if (button == BUTTON_LEFT) upLeft();
        else upRight();
    }

    public static byte spoofed(byte button) {
        return button == BUTTON_LEFT ? spoofedLeft : spoofedRight;
    }
}