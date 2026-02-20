package com.icysnex.ghosttap.core;

import com.icysnex.ghosttap.mixin.MixinLwjglInputMouseAccessor;
import org.lwjgl.Sys;
import org.lwjgl.input.Mouse;

import java.nio.ByteBuffer;

public abstract class InputMouse {
    static final byte BUTTON_LEFT = 0;
    static final byte BUTTON_RIGHT = 1;

    public static final byte BUTTON_DOWN = 1;
    public static final byte BUTTON_UP = 0;


    public static byte spoofedLeft = InputMouse.BUTTON_UP;
    public static byte spoofedRight = InputMouse.BUTTON_UP;


    public static void downLeft() {
        setReadBuffer(BUTTON_LEFT, BUTTON_DOWN);
        spoofedLeft = BUTTON_DOWN;
    }
    public static void upLeft() {
        setReadBuffer(BUTTON_LEFT, BUTTON_UP);
        spoofedLeft = BUTTON_UP;
    }

    public static void downRight() {
        setReadBuffer(BUTTON_RIGHT, BUTTON_DOWN);
        spoofedRight = BUTTON_DOWN;
    }
    public static void upRight() {
        setReadBuffer(BUTTON_RIGHT, BUTTON_UP);
        spoofedRight = BUTTON_UP;
    }


    static void setReadBuffer(byte button, byte down) {
        ByteBuffer buffer = MixinLwjglInputMouseAccessor.getReadBuffer();

        // Save old position
        int oldPos = buffer.position();

        buffer.position(buffer.limit()); // write at end
        buffer.limit(buffer.capacity());

        buffer.put(button);
        buffer.put(down);

        buffer.putInt(Mouse.getX());
        buffer.putInt(Mouse.getY());

        buffer.putInt(0);
        buffer.putLong(System.nanoTime());

        // restore for reading
        buffer.limit(buffer.position());
        buffer.position(oldPos);
    }
}
