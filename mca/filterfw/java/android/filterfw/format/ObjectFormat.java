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


package android.filterfw.format;

import android.filterfw.core.FrameFormat;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeBuffer;
import android.util.Log;

/**
 * @hide
 */
public class ObjectFormat {

    public static MutableFrameFormat fromClass(Class clazz, int count, int target) {
        // Create frame format
        MutableFrameFormat result = new MutableFrameFormat(FrameFormat.TYPE_OBJECT, target);
        result.setObjectClass(clazz);
        if (count != FrameFormat.SIZE_UNSPECIFIED) {
            result.setDimensions(count);
        }
        result.setBytesPerSample(bytesPerSampleForClass(clazz, target));
        Log.i("ObjectFormat", "Created new format: " + result + "!");
        return result;
    }

    public static MutableFrameFormat fromClass(Class clazz, int target) {
        return fromClass(clazz, FrameFormat.SIZE_UNSPECIFIED, target);
    }

    public static MutableFrameFormat fromObject(Object object, int target) {
        return fromClass(object.getClass(), FrameFormat.SIZE_UNSPECIFIED, target);
    }

    public static MutableFrameFormat fromObject(Object object, int count, int target) {
        return fromClass(object.getClass(), count, target);
    }

    private static int bytesPerSampleForClass(Class clazz, int target) {
        // Native targets have objects manifested in a byte buffer. Thus it is important to
        // correctly determine the size of single element here.
        if (target == FrameFormat.TARGET_NATIVE) {
            if (!NativeBuffer.class.isAssignableFrom(clazz)) {
                throw new IllegalArgumentException("Native object-based formats must be of a " +
                    "NativeBuffer subclass! (Received class: " + clazz + ").");
            }
            try {
                return ((NativeBuffer)clazz.newInstance()).getElementSize();
            } catch (Exception e) {
                throw new RuntimeException("Could not determine the size of an element in a "
                    + "native object-based frame of type " + clazz + "! Perhaps it is missing a "
                    + "default constructor?");
            }
        } else {
            return FrameFormat.BYTES_PER_SAMPLE_UNSPECIFIED;
        }
    }
}
