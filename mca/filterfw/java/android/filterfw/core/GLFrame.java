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
import android.filterfw.core.StopWatchMap;
import android.graphics.Bitmap;
import android.opengl.GLES20;

import android.graphics.Rect;

import java.nio.ByteBuffer;

class GLFrameTimer {

    private static StopWatchMap mTimer = null;

    public static StopWatchMap get() {
        if (mTimer == null) {
            mTimer = new StopWatchMap();
        }
        return mTimer;
    }

}

public class GLFrame extends Frame {

    // GL-related binding types
    public final static int EXISTING_TEXTURE_BINDING = 100;
    public final static int EXISTING_FBO_BINDING     = 101;
    public final static int NEW_TEXTURE_BINDING      = 102;
    public final static int NEW_FBO_BINDING          = 103;

    // GL-related meta-values
    public final static String USE_EXTERNAL_TEXTURE = "UseExternalTexture";

    private int glFrameId;

    GLFrame(FrameFormat format, FrameManager frameManager) {
        super(format, frameManager);
    }

    GLFrame(FrameFormat format, FrameManager frameManager, int bindingType, long bindingId) {
        super(format, frameManager, bindingType, bindingId);
    }

    void init(boolean isEmpty) {
        FrameFormat format = getFormat();

        // Check that we have a valid format
        if (isEmpty) {
            throw new RuntimeException("Instantiating empty GL frames is not supported!");
        } else if (format.getBytesPerSample() != 4) {
            throw new IllegalArgumentException("GL frames must have 4 bytes per sample!");
        } else if (format.getDimensionCount() != 2) {
            throw new IllegalArgumentException("GL frames must be 2-dimensional!");
        } else if (getFormat().getSize() < 0) {
            throw new IllegalArgumentException("Initializing GL frame with zero size!");
        }

        // Create correct frame
        int bindingType = getBindingType();
        if (bindingType == Frame.NO_BINDING) {
            boolean isExternal = false;
            if (format.hasMetaKey(USE_EXTERNAL_TEXTURE, Boolean.class)) {
                isExternal = (Boolean)format.getMetaValue(USE_EXTERNAL_TEXTURE);
            }
            initNew(isExternal);
            setReusable(!isEmpty && !isExternal);
        } else if (bindingType == EXISTING_TEXTURE_BINDING) {
            initWithTexture((int)getBindingId(), false);
        } else if (bindingType == EXISTING_FBO_BINDING) {
            initWithFbo((int)getBindingId(), false);
        } else if (bindingType == NEW_TEXTURE_BINDING) {
            initWithTexture((int)getBindingId(), true);
        } else if (bindingType == NEW_FBO_BINDING) {
            initWithFbo((int)getBindingId(), true);
        } else {
            throw new RuntimeException("Attempting to create GL frame with unknown binding type "
                + bindingType + "!");
        }
    }

    private void initNew(boolean isExternal) {
        if (isExternal) {
            if (!allocateExternal()) {
                throw new RuntimeException("Could not allocate external GL frame!");
            }
        } else {
            if (!allocate(getFormat().getWidth(), getFormat().getHeight())) {
                throw new RuntimeException("Could not allocate GL frame!");
            }
        }
    }

    private void initWithTexture(int texId, boolean isNew) {
        int width = getFormat().getWidth();
        int height = getFormat().getHeight();
        if (!allocateWithTexture(texId, width, height, isNew, isNew)) {
            throw new RuntimeException("Could not allocate texture backed GL frame!");
        }
        setReusable(isNew);
    }

    private void initWithFbo(int fboId, boolean isNew) {
        int width = getFormat().getWidth();
        int height = getFormat().getHeight();
        if (!allocateWithFbo(fboId, width, height, isNew, isNew)) {
            throw new RuntimeException("Could not allocate FBO backed GL frame!");
        }
        setReusable(isNew);
    }

    void flushGPU(String message) {
        StopWatchMap timer = GLFrameTimer.get();
        if (timer.LOG_MFF_RUNNING_TIMES) {
          timer.start("glFinish " + message);
          GLES20.glFinish();
          timer.stop("glFinish " + message);
        }
    }

    void dealloc() {
        deallocate();
    }

    public Object getObjectValue() {
        return ByteBuffer.wrap(getNativeData());
    }

    public void setInts(int[] ints) {
        assertFrameMutable();
        if (!setNativeInts(ints)) {
            throw new RuntimeException("Could not set int values for GL frame!");
        }
    }

    public int[] getInts() {
        flushGPU("getInts");
        return getNativeInts();
    }

    public void setFloats(float[] floats) {
        assertFrameMutable();
        if (!setNativeFloats(floats)) {
            throw new RuntimeException("Could not set int values for GL frame!");
        }
    }

    public float[] getFloats() {
        flushGPU("getFloats");
        return getNativeFloats();
    }

    public void setData(ByteBuffer buffer, int offset, int length) {
        assertFrameMutable();
        byte[] bytes = buffer.array();
        if (getFormat().getSize() != bytes.length) {
            throw new RuntimeException("Data size in setData does not match GL frame size!");
        } else if (!setNativeData(bytes, offset, length)) {
            throw new RuntimeException("Could not set GL frame data!");
        }
    }

    public ByteBuffer getData() {
        flushGPU("getData");
        return ByteBuffer.wrap(getNativeData());
    }

    public void setBitmap(Bitmap bitmap) {
        assertFrameMutable();
        if (getFormat().getWidth()  != bitmap.getWidth() ||
            getFormat().getHeight() != bitmap.getHeight()) {
            throw new RuntimeException("Bitmap dimensions do not match GL frame dimensions!");
        } else {
            Bitmap rgbaBitmap = convertBitmapToRGBA(bitmap);
            if (!setNativeBitmap(rgbaBitmap, rgbaBitmap.getByteCount())) {
                throw new RuntimeException("Could not set GL frame bitmap data!");
            }
        }
    }

    public Bitmap getBitmap() {
        flushGPU("getBitmap");
        Bitmap result = Bitmap.createBitmap(getFormat().getWidth(),
                                            getFormat().getHeight(),
                                            Bitmap.Config.ARGB_8888);
        if (!getNativeBitmap(result)) {
            throw new RuntimeException("Could not get bitmap data from GL frame!");
        }
        return result;
    }

    public void setDataFromFrame(Frame frame) {
        // Make sure frame fits
        if (getFormat().getSize() < frame.getFormat().getSize()) {
            throw new RuntimeException(
                "Attempting to assign frame of size " + frame.getFormat().getSize() + " to " +
                "smaller GL frame of size " + getFormat().getSize() + "!");
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

    public void setViewport(int x, int y, int width, int height) {
        assertFrameMutable();
        setNativeViewport(x, y, width, height);
    }

    public void setViewport(Rect rect) {
        assertFrameMutable();
        setNativeViewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    }

    public void generateMipMap() {
        assertFrameMutable();
        if (!generateNativeMipMap()) {
            throw new RuntimeException("Could not generate mip-map for GL frame!");
        }
    }

    public void setTextureParameter(int param, int value) {
        assertFrameMutable();
        if (!setNativeTextureParam(param, value)) {
            throw new RuntimeException("Could not set texture value " + param + " = " + value + " " +
                                       "for GLFrame!");
        }
    }

    public int getTextureId() {
        return getNativeTextureId();
    }

    public int getFboId() {
        return getNativeFboId();
    }

    public void focus() {
        if (!nativeFocus()) {
            throw new RuntimeException("Could not focus on GLFrame for drawing!");
        }
    }

    static {
        System.loadLibrary("filterfw2-jni");
    }

    private native boolean allocate(int width, int height);

    private native boolean allocateWithTexture(int textureId,
                                               int width,
                                               int height,
                                               boolean owns,
                                               boolean create);

    private native boolean allocateWithFbo(int fboId,
                                           int width,
                                           int height,
                                           boolean owns,
                                           boolean create);

    private native boolean allocateExternal();

    private native boolean deallocate();

    private native boolean setNativeData(byte[] data, int offset, int length);

    private native byte[] getNativeData();

    private native boolean setNativeInts(int[] ints);

    private native boolean setNativeFloats(float[] floats);

    private native int[] getNativeInts();

    private native float[] getNativeFloats();

    private native boolean setNativeBitmap(Bitmap bitmap, int size);

    private native boolean getNativeBitmap(Bitmap bitmap);

    private native boolean setNativeViewport(int x, int y, int width, int height);

    private native int getNativeTextureId();

    private native int getNativeFboId();

    private native boolean generateNativeMipMap();

    private native boolean setNativeTextureParam(int param, int value);

    private native boolean nativeCopyFromNative(NativeFrame frame);

    private native boolean nativeCopyFromGL(GLFrame frame);

    private native boolean nativeFocus();
}
