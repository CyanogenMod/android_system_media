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

import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;

import java.util.Arrays;

public class FrameFormat {

    public static final int TYPE_UNSPECIFIED = 0;
    public static final int TYPE_BIT         = 1;
    public static final int TYPE_BYTE        = 2;
    public static final int TYPE_INT16       = 3;
    public static final int TYPE_INT32       = 4;
    public static final int TYPE_FLOAT       = 5;
    public static final int TYPE_DOUBLE      = 6;
    public static final int TYPE_POINTER     = 7;
    public static final int TYPE_OBJECT      = 8;
    public static final int TYPE_STRUCT      = 9;

    public static final int TARGET_UNSPECIFIED  = 0;
    public static final int TARGET_JAVA         = 1;
    public static final int TARGET_NATIVE       = 2;
    public static final int TARGET_GPU          = 3;
    public static final int TARGET_VERTEXBUFFER = 4;
    public static final int TARGET_RS           = 5;

    public static final int SIZE_UNSPECIFIED = 0;

    // TODO: When convenience formats are used, consider changing this to 0 and have the convenience
    // intializers use a proper BPS.
    public static final int BYTES_PER_SAMPLE_UNSPECIFIED = 1;

    protected static final int SIZE_UNKNOWN = -1;

    protected int mBaseType = TYPE_UNSPECIFIED;
    protected int mBytesPerSample = 1;
    protected int mSize = SIZE_UNKNOWN;
    protected int mTarget = TARGET_UNSPECIFIED;
    protected int[] mDimensions;
    protected KeyValueMap mMetaData;
    protected Class mObjectClass;

    protected FrameFormat() {
        mMetaData = new KeyValueMap();
    }

    public FrameFormat(int baseType, int target) {
        mBaseType = baseType;
        mTarget = target;
        initDefaults();
    }

    public int getBaseType() {
        return mBaseType;
    }

    public boolean isBinaryDataType() {
        return mBaseType >= TYPE_BIT && mBaseType <= TYPE_DOUBLE;
    }

    public int getBytesPerSample() {
        return mBytesPerSample;
    }

    public int getTarget() {
        return mTarget;
    }

    public int[] getDimensions() {
        return mDimensions;
    }

    public int getDimension(int i) {
        return mDimensions[i];
    }

    public int getDimensionCount() {
        return mDimensions == null ? 0 : mDimensions.length;
    }

    public boolean hasMetaKey(String key) {
        return mMetaData != null ? mMetaData.hasKey(key) : false;
    }

    public boolean hasMetaKey(String key, Class expectedClass) {
        if (mMetaData != null && mMetaData.hasKey(key)) {
            if (!expectedClass.isAssignableFrom(mMetaData.getValue(key).getClass())) {
                throw new RuntimeException(
                    "FrameFormat meta-key '" + key + "' is of type " +
                    mMetaData.getValue(key).getClass() + " but expected to be of type " +
                    expectedClass + "!");
            }
            return true;
        }
        return false;
    }

    public Object getMetaValue(String key) {
        return mMetaData != null ? mMetaData.getValue(key) : null;
    }

    public int getNumberOfDimensions() {
        return mDimensions != null ? mDimensions.length : 0;
    }

    public int getLength() {
        return (mDimensions != null && mDimensions.length >= 1) ? mDimensions[0] : -1;
    }

    public int getWidth() {
        return getLength();
    }

    public int getHeight() {
        return (mDimensions != null && mDimensions.length >= 2) ? mDimensions[1] : -1;
    }

    public int getDepth() {
        return (mDimensions != null && mDimensions.length >= 3) ? mDimensions[2] : -1;
    }

    public int getSize() {
        if (mSize == SIZE_UNKNOWN) mSize = calcSize(mDimensions);
        return mSize;
    }

    public Class getObjectClass() {
        return mObjectClass;
    }

    public MutableFrameFormat mutableCopy() {
        MutableFrameFormat result = new MutableFrameFormat();
        result.setBaseType(getBaseType());
        result.setTarget(getTarget());
        result.setBytesPerSample(getBytesPerSample());
        result.setDimensions(getDimensions());
        result.setObjectClass(getObjectClass());
        result.mMetaData = mMetaData;
        return result;
    }

    public boolean equals(Object object) {
        if (this == object) {
            return true;
        }

        if (!(object instanceof FrameFormat)) {
            return false;
        }

        FrameFormat format = (FrameFormat)object;
        return format.mBaseType == mBaseType &&
                format.mTarget == mTarget &&
                format.mBytesPerSample == mBytesPerSample &&
                Arrays.equals(format.mDimensions, mDimensions) &&
                format.mMetaData.equals(mMetaData);
    }

    public int hashCode() {
        return 4211 ^ mBaseType ^ mBytesPerSample ^ getSize();
    }

    public boolean isCompatibleWith(FrameFormat specification) {
        // Check base type
        if (specification.getBaseType() != TYPE_UNSPECIFIED
            && getBaseType() != specification.getBaseType()) {
            return false;
        }

        // Check target
        if (specification.getTarget() != TARGET_UNSPECIFIED
            && getTarget() != specification.getTarget()) {
            return false;
        }

        // Check bytes per sample
        if (specification.getBytesPerSample() != BYTES_PER_SAMPLE_UNSPECIFIED
            && getBytesPerSample() != specification.getBytesPerSample()) {
            return false;
        }

        // Check number of dimensions
        if (specification.getDimensionCount() > 0
            && getDimensionCount() != specification.getDimensionCount()) {
            return false;
        }

        // Check dimensions
        for (int i = 0; i < getDimensionCount(); ++i) {
            int specDim = specification.getDimension(i);
            if (specDim != SIZE_UNSPECIFIED && getDimension(i) != specDim) {
                return false;
            }
        }

        // Check meta-data
        for (String specKey : specification.mMetaData.keySet()) {
            if (!mMetaData.hasKey(specKey)
            || !mMetaData.getValue(specKey).equals(specification.mMetaData.getValue(specKey))) {
                return false;
            }
        }

        // Passed all the tests
        return true;
    }

    static String dimensionsToString(int[] dimensions) {
        StringBuffer buffer = new StringBuffer("(");
        if (dimensions != null) {
            int n = dimensions.length;
            for (int i = 0; i < n - 1; ++i) {
                buffer.append(String.valueOf(dimensions[i])).append(" x ");
            }
            buffer.append(dimensions[n - 1]);
        }
        buffer.append(")");
        return buffer.toString();
    }

    static String baseTypeToString(int baseType) {
        switch (baseType) {
            case TYPE_UNSPECIFIED: return "unspecified";
            case TYPE_BIT:         return "bit";
            case TYPE_BYTE:        return "byte";
            case TYPE_INT16:       return "int16";
            case TYPE_INT32:       return "int32";
            case TYPE_FLOAT:       return "float";
            case TYPE_DOUBLE:      return "double";
            case TYPE_POINTER:     return "pointer";
            case TYPE_OBJECT:      return "object";
            case TYPE_STRUCT:      return "struct";
            default:               return "unknown";
        }
    }

    static String targetToString(int target) {
        switch (target) {
            case TARGET_UNSPECIFIED:  return "unspecified";
            case TARGET_JAVA:         return "Java";
            case TARGET_NATIVE:       return "Native";
            case TARGET_GPU:          return "GPU";
            case TARGET_VERTEXBUFFER: return "VBO";
            case TARGET_RS:           return "RenderScript";
            default:                  return "unknown";
        }
    }

    public String toString() {
        return "Format {  BaseType: " + baseTypeToString(mBaseType)
            + ", Target: " + targetToString(mTarget)
            + ", BytesPerSample: " + mBytesPerSample
            + ", Dimensions: " + dimensionsToString(mDimensions)
            + " }";
    }

    private void initDefaults() {
        // Defaults based on base-type
        switch (mBaseType) {
            case TYPE_BIT:
            case TYPE_BYTE:
                mBytesPerSample = 1;
                break;
            case TYPE_INT16:
                mBytesPerSample = 2;
                break;
            case TYPE_INT32:
            case TYPE_FLOAT:
                mBytesPerSample = 4;
                break;
            case TYPE_DOUBLE:
                mBytesPerSample = 8;
                break;
            default:
                mBytesPerSample = BYTES_PER_SAMPLE_UNSPECIFIED;
                break;
        }
    }
    // Core internal methods /////////////////////////////////////////////////////////////////////////
    int calcSize(int[] dimensions) {
        if (dimensions != null && dimensions.length > 0) {
            int size = getBytesPerSample();
            for (int dim : dimensions) {
                size *= dim;
            }
            return size;
        }
        return 0;
    }

    boolean isReplaceableBy(FrameFormat format) {
        return mTarget == format.mTarget && getSize() == format.getSize();
    }
}
