/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package android.filterfw.core;

import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.FrameManager;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeFrame;
import android.graphics.Bitmap;

import android.graphics.Rect;

import java.nio.ByteBuffer;

public class VertexFrame extends Frame {

    private int vertexFrameId;

    VertexFrame(FrameFormat format, FrameManager frameManager, boolean isEmpty) {
        super(format, frameManager);
        if (isEmpty) {
            throw new RuntimeException("Instantiating empty vertex frames is not supported!");
        } else {
            if (getFormat().getSize() < 0) {
                throw new IllegalArgumentException("Initializing vertex frame with zero size!");
            } else {
                if (!allocate(getFormat().getSize())) {
                    throw new RuntimeException("Could not allocate vertex frame!");
                }
            }
        }
    }

    void dealloc() {
        deallocate();
    }

    public Object getObjectValue() {
        throw new RuntimeException("Vertex frames do not support reading data!");
    }

    public void setInts(int[] ints) {
        assertFrameMutable();
        if (!setNativeInts(ints)) {
            throw new RuntimeException("Could not set int values for vertex frame!");
        }
    }

    public int[] getInts() {
        throw new RuntimeException("Vertex frames do not support reading data!");
    }

    public void setFloats(float[] floats) {
        assertFrameMutable();
        if (!setNativeFloats(floats)) {
            throw new RuntimeException("Could not set int values for vertex frame!");
        }
    }

    public float[] getFloats() {
        throw new RuntimeException("Vertex frames do not support reading data!");
    }

    public void setData(ByteBuffer buffer, int offset, int length) {
        assertFrameMutable();
        byte[] bytes = buffer.array();
        if (getFormat().getSize() != bytes.length) {
            throw new RuntimeException("Data size in setData does not match vertex frame size!");
        } else if (!setNativeData(bytes, offset, length)) {
            throw new RuntimeException("Could not set vertex frame data!");
        }
    }

    public ByteBuffer getData() {
        throw new RuntimeException("Vertex frames do not support reading data!");
    }

    public void setBitmap(Bitmap bitmap) {
        throw new RuntimeException("Unsupported: Cannot set vertex frame bitmap value!");
    }

    public Bitmap getBitmap() {
        throw new RuntimeException("Vertex frames do not support reading data!");
    }

    public void setDataFromFrame(Frame frame) {
        // TODO: Optimize
        super.setDataFromFrame(frame);
    }

    public int getVboId() {
        return getNativeVboId();
    }

    static {
        System.loadLibrary("filterfw-jni");
    }

    private native boolean allocate(int size);

    private native boolean deallocate();

    private native boolean setNativeData(byte[] data, int offset, int length);

    private native boolean setNativeInts(int[] ints);

    private native boolean setNativeFloats(float[] floats);

    private native int getNativeVboId();
}

