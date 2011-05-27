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


package android.filterpacks.imageproc;

import android.content.Context;
import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.FrameManager;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeFrame;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

public class ImageDecoder extends Filter {

    @FilterParameter(name = "resourceName", isOptional = false)
    String mResourceName;

    @FilterParameter(name = "context", isOptional = false)
    Context mContext;

    @FilterParameter(name = "target", isOptional = true)
    String mTargetString;

    @FilterParameter(name = "repeatFrame", isOptional = true)
    boolean repeatFrame = false;

    private String mFilePath;
    private Bitmap mBitmap;
    private MutableFrameFormat mOutputFormat;
    private Frame mImageFrame;

    public ImageDecoder(String name) {
        super(name);
    }

    public void initFilter() {
        int target = FrameFormat.TARGET_NATIVE;
        if (mTargetString != null) {
            if (mTargetString.equals("CPU")) {
                target = FrameFormat.TARGET_NATIVE;
            } else if (mTargetString.equals("GPU")) {
                target = FrameFormat.TARGET_GPU;
            } else {
                throw new RuntimeException("Unknown frame target: '" + mTargetString + "'!");
            }
        }

        // Load image
        int resourceId = mContext.getResources().getIdentifier(mResourceName,
                                                               null,
                                                               mContext.getPackageName());
        mBitmap = BitmapFactory.decodeResource(mContext.getResources(), resourceId);

        // Setup output format
        mOutputFormat = new MutableFrameFormat(FrameFormat.TYPE_BYTE, target);
        mOutputFormat.setBytesPerSample(4);
        mOutputFormat.setDimensions(mBitmap.getWidth(), mBitmap.getHeight());
    }

    public String[] getInputNames() {
        return null;
    }

    public String[] getOutputNames() {
        return new String[] { "image" };
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return false;
    }

    public FrameFormat getOutputFormat(int index) {
        return mOutputFormat;
    }

    public int open(FilterContext env) {
        mImageFrame = env.getFrameManager().newFrame(mOutputFormat);
        mImageFrame.setBitmap(mBitmap);

        return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    public int process(FilterContext env) {
        putOutput(0, mImageFrame);

        if (repeatFrame) {
            return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
        } else {
            return Filter.STATUS_FINISHED;
        }
    }

    public void close(FilterContext env) {
        mImageFrame.release();
        mImageFrame = null;
    }
}
