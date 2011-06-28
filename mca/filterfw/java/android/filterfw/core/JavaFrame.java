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
import android.filterfw.core.NativeBuffer;
import android.filterfw.format.ObjectFormat;
import android.graphics.Bitmap;

import java.lang.reflect.Constructor;
import java.nio.ByteBuffer;

/**
 * @hide
 */
public class JavaFrame extends Frame {

    private Object mObject;

    JavaFrame(FrameFormat format, FrameManager frameManager, boolean isEmpty) {
        super(format, frameManager);
        if (!isEmpty) {
            initWithFormat(format);
        }
        setReusable(false);
    }

    static JavaFrame wrapObject(Object object, FrameManager frameManager) {
        FrameFormat format = ObjectFormat.fromObject(object, FrameFormat.TARGET_JAVA);
        JavaFrame result = new JavaFrame(format, frameManager, true);
        result.setObjectValue(object);
        return result;
    }

    private void initWithFormat(FrameFormat format) {
        final int count = format.getLength();
        final int baseType = format.getBaseType();
        if (baseType == FrameFormat.TYPE_OBJECT) {
            initWithObject(format.getObjectClass(), count);
//        } else if (baseType == FrameFormat.TYPE_STRUCT) {
//            initWithStruct(format.getObjectClass(), count);
        } else {
            if (count < 1) {
                throw new IllegalArgumentException(
                    "Initializing non-empty object frame with no size specified! Did you forget " +
                    "to set the dimensions?");
            }
            switch (baseType) {
                case FrameFormat.TYPE_BYTE:
                    mObject = new byte[count];
                    break;
                case FrameFormat.TYPE_INT16:
                    mObject = new short[count];
                    break;
                case FrameFormat.TYPE_INT32:
                    mObject = new int[count];
                    break;
                case FrameFormat.TYPE_FLOAT:
                    mObject = new float[count];
                    break;
                case FrameFormat.TYPE_DOUBLE:
                    mObject = new double[count];
                    break;
                default:
                    throw new IllegalArgumentException(
                        "Unsupported object frame type: " + baseType + "!");
            }
        }
    }

    private void initWithObject(Class objectClass, int count) {
        // Make sure we have an object class set
        if (objectClass == null) {
            throw new RuntimeException(
                "JavaFrame based on TYPE_OBJECT requires an object class!");
        }

        // Size must be 1 for custom objects
        if (count != 1) {
            throw new RuntimeException(
                "JavaFrame does not support instantiating a frame based on " + objectClass + " " +
                "with a size not equal to 1!");
        }

        // Instantiate a new empty instance of this class
        try {
          mObject = objectClass.newInstance();
        } catch (Exception e) {
          throw new RuntimeException("Could not instantiate new structure instance of type '" +
                                     objectClass.getName() + "'. Make sure this class type has " +
                                     "a public default constructor!");
        }
    }

/*  TODO: Remove?
    private void initWithStruct(Class structClass, int count) {
        // Make sure we have an object class set
        if (structClass == null) {
            throw new RuntimeException(
                "JavaFrame based on TYPE_STRUCT requires an object class!");
        }

        // Make sure it is a NativeBuffer subclass
        if (!NativeBuffer.class.isAssignableFrom(structClass)) {
            throw new RuntimeException("JavaFrame's class must be a subclass of " +
                                       "NativeBuffer when using TYPE_STRUCT!");
        }

        // Instantiate a new empty instance of this class
        try {
            Constructor structConstructor = structClass.getConstructor(int.class);
            mObject = structConstructor.newInstance(count);
        } catch (Exception e) {
            throw new RuntimeException("Could not instantiate new structure instance of type '" +
                                       structClass + "'!");
        }
    }
*/

    @Override
    void dealloc() {
        mObject = null;
    }

    @Override
    public Object getObjectValue() {
        return mObject;
    }

    @Override
    public void setInts(int[] ints) {
        assertFrameMutable();
        setGenericObjectValue(ints);
    }

    @Override
    public int[] getInts() {
        return (mObject instanceof int[]) ? (int[])mObject : null;
    }

    @Override
    public void setFloats(float[] floats) {
        assertFrameMutable();
        setGenericObjectValue(floats);
    }

    @Override
    public float[] getFloats() {
        return (mObject instanceof float[]) ? (float[])mObject : null;
    }

    @Override
    public void setData(ByteBuffer buffer, int offset, int length) {
        assertFrameMutable();
        setGenericObjectValue(ByteBuffer.wrap(buffer.array(), offset, length));
    }

    @Override
    public ByteBuffer getData() {
        return (mObject instanceof ByteBuffer) ? (ByteBuffer)mObject : null;
    }

    @Override
    public void setBitmap(Bitmap bitmap) {
        assertFrameMutable();
        setGenericObjectValue(bitmap);
    }

    @Override
    public Bitmap getBitmap() {
        return (mObject instanceof Bitmap) ? (Bitmap)mObject : null;
    }

    private void setFormatObjectClass(Class objectClass) {
        MutableFrameFormat format = getFormat().mutableCopy();
        format.setObjectClass(objectClass);
        setFormat(format);
    }

    @Override
    protected void setGenericObjectValue(Object object) {
        // Update the FrameFormat class
        // TODO: Take this out! FrameFormats should not be modified and convenience formats used
        // instead!
        FrameFormat format = getFormat();
        if (format.getObjectClass() == null) {
            setFormatObjectClass(object.getClass());
        } else if (!format.getObjectClass().isAssignableFrom(object.getClass())) {
            throw new RuntimeException(
                "Attempting to set object value of type '" + object.getClass() + "' on " +
                "JavaFrame of type '" + format.getObjectClass() + "'!");
        }

        // Set the object value
        mObject = object;
    }
}
