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
import android.filterfw.core.GLFrame;
import android.filterfw.core.NativeBuffer;
import android.graphics.Bitmap;

import java.nio.ByteBuffer;

public class NativeFrame extends Frame {

    private int nativeFrameId;

    NativeFrame(FrameFormat format, FrameManager frameManager, boolean isEmpty) {
        super(format, frameManager);
        int capacity = format.getSize();
        if (isEmpty && capacity != 0) {
            throw new IllegalArgumentException(
                "Initializing empty native frame with non-zero size! You must not specify any " +
                "dimensions when allocating an empty frame!");
        } else if (!isEmpty && capacity <= 0) {
            throw new IllegalArgumentException(
                "Initializing non-empty native frame with no size specified! Did you forget " +
                "to set the dimensions?");
        } else {
            allocate(capacity);
        }
        setReusable(!isEmpty);
    }

    void dealloc() {
        deallocate();
    }

    public int getCapacity() {
        return getNativeCapacity();
    }

    public Object getObjectValue() {
        // If this is not a structured frame, return our data
        if (getFormat().getBaseType() != FrameFormat.TYPE_STRUCT) {
            return getData();
        }

        // Get the structure class
        Class structClass = getFormat().getObjectClass();
        if (structClass == null) {
            throw new RuntimeException("Attempting to get object data from frame that does " +
                                       "not specify a structure object class!");
        }

        // Make sure it is a NativeBuffer subclass
        if (!NativeBuffer.class.isAssignableFrom(structClass)) {
            throw new RuntimeException("NativeFrame object class must be a subclass of " +
                                       "NativeBuffer!");
        }

        // Instantiate a new empty instance of this class
        NativeBuffer structData = null;
        try {
          structData = (NativeBuffer)structClass.newInstance();
        } catch (Exception e) {
          throw new RuntimeException("Could not instantiate new structure instance of type '" +
                                     structClass + "'!");
        }

        // Wrap it around our data
        if (!getNativeBuffer(structData)) {
            throw new RuntimeException("Could not get the native structured data for frame!");
        }

        // Attach this frame to it, so that we do not get deallocated while the NativeBuffer is
        // accessing our data.
        structData.attachToFrame(this);

        return structData;
    }

    public void setInts(int[] ints) {
        assertFrameMutable();
        if (ints.length * nativeIntSize() > getFormat().getSize()) {
            throw new RuntimeException(
                "NativeFrame cannot hold " + ints.length + " integers. (Can only hold " +
                (getFormat().getSize() / nativeIntSize()) + " integers).");
        } else if (!setNativeInts(ints)) {
            throw new RuntimeException("Could not set int values for native frame!");
        }
    }

    public int[] getInts() {
        return getNativeInts(getFormat().getSize());
    }

    public void setFloats(float[] floats) {
        assertFrameMutable();
        if (floats.length * nativeFloatSize() > getFormat().getSize()) {
            throw new RuntimeException(
                "NativeFrame cannot hold " + floats.length + " floats. (Can only hold " +
                (getFormat().getSize() / nativeFloatSize()) + " floats).");
        } else if (!setNativeFloats(floats)) {
            throw new RuntimeException("Could not set int values for native frame!");
        }
    }

    public float[] getFloats() {
        return getNativeFloats(getFormat().getSize());
    }

    // TODO: This function may be a bit confusing: Is the offset the target or source offset? Maybe
    // we should allow specifying both? (May be difficult for other frame types).
    public void setData(ByteBuffer buffer, int offset, int length) {
        assertFrameMutable();
        byte[] bytes = buffer.array();
        if ((length + offset) > buffer.limit()) {
            throw new RuntimeException("Offset and length exceed buffer size in native setData: " +
                                       (length + offset) + " bytes given, but only " + buffer.limit() +
                                       " bytes available!");
        } else if (getFormat().getSize() != length) {
            throw new RuntimeException("Data size in setData does not match native frame size: " +
                                       "Frame size is " + getFormat().getSize() + " bytes, but " +
                                       length + " bytes given!");
        } else if (!setNativeData(bytes, offset, length)) {
            throw new RuntimeException("Could not set native frame data!");
        }
    }

    public ByteBuffer getData() {
        return ByteBuffer.wrap(getNativeData(getFormat().getSize()));
    }

    public void setBitmap(Bitmap bitmap) {
        assertFrameMutable();
        if (getFormat().getNumberOfDimensions() != 2) {
            throw new RuntimeException("Attempting to set Bitmap for non 2-dimensional native frame!");
        } else if (getFormat().getWidth()  != bitmap.getWidth() ||
                   getFormat().getHeight() != bitmap.getHeight()) {
            throw new RuntimeException("Bitmap dimensions do not match native frame dimensions!");
        } else {
            Bitmap rgbaBitmap = convertBitmapToRGBA(bitmap);
            if (!setNativeBitmap(rgbaBitmap, rgbaBitmap.getByteCount())) {
                throw new RuntimeException("Could not set native frame bitmap data!");
            }
        }
    }

    public Bitmap getBitmap() {
        if (getFormat().getNumberOfDimensions() != 2) {
            throw new RuntimeException("Attempting to get Bitmap for non 2-dimensional native frame!");
        }
        Bitmap result = Bitmap.createBitmap(getFormat().getWidth(),
                                            getFormat().getHeight(),
                                            Bitmap.Config.ARGB_8888);
        if (!getNativeBitmap(result)) {
            throw new RuntimeException("Could not get bitmap data from native frame!");
        }
        return result;
    }

    public void setDataFromFrame(Frame frame) {
        // Make sure frame fits
        if (getFormat().getSize() < frame.getFormat().getSize()) {
            throw new RuntimeException(
                "Attempting to assign frame of size " + frame.getFormat().getSize() + " to " +
                "smaller native frame of size " + getFormat().getSize() + "!");
        }

        // Invoke optimized implementations if possible
        if (frame instanceof NativeFrame) {
            nativeCopyFromNative((NativeFrame)frame);
        } else if (frame instanceof GLFrame) {
            nativeCopyFromGL((GLFrame)frame);
        } else if (frame instanceof ObjectFrame) {
            setObjectValue(frame.getObjectValue());
        } else {
            super.setDataFromFrame(frame);
        }
    }

    static {
        System.loadLibrary("filterfw2-jni");
    }

    private native boolean allocate(int capacity);

    private native boolean deallocate();

    private native int getNativeCapacity();

    private static native int nativeIntSize();

    private static native int nativeFloatSize();

    private native boolean setNativeData(byte[] data, int offset, int length);

    private native byte[] getNativeData(int byteCount);

    private native boolean getNativeBuffer(NativeBuffer buffer);

    private native boolean setNativeInts(int[] ints);

    private native boolean setNativeFloats(float[] floats);

    private native int[] getNativeInts(int byteCount);

    private native float[] getNativeFloats(int byteCount);

    private native boolean setNativeBitmap(Bitmap bitmap, int size);

    private native boolean getNativeBitmap(Bitmap bitmap);

    private native boolean nativeCopyFromNative(NativeFrame frame);

    private native boolean nativeCopyFromGL(GLFrame frame);
}
