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


package android.filterpacks.base;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.GLFrame;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.format.ImageFormat;

import java.util.Set;

public class GLTextureSource extends Filter {

    @FilterParameter(name = "texId", isOptional = false, isUpdatable = false)
    private int mTexId;

    @FilterParameter(name = "width", isOptional = false, isUpdatable = false)
    private int mWidth;

    @FilterParameter(name = "height", isOptional = false, isUpdatable = false)
    private int mHeight;

    private MutableFrameFormat mOutputFormat;
    private Frame mFrame;

    public GLTextureSource(String name) {
        super(name);
    }

    @Override
    public void initFilter() {
        mOutputFormat = ImageFormat.create(mWidth, mHeight,
                                           ImageFormat.COLORSPACE_RGBA,
                                           FrameFormat.TARGET_GPU);
    }

    @Override
    public String[] getInputNames() {
        return null;
    }

    @Override
    public String[] getOutputNames() {
        return new String[] { "frame" };
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return false;
    }

    @Override
    public FrameFormat getOutputFormat(int index) {
        return mOutputFormat;
    }

    @Override
    public int process(FilterContext context) {
        // Generate frame if not generated already
        if (mFrame == null) {
            mFrame = context.getFrameManager().newBoundFrame(mOutputFormat,
                                                             GLFrame.EXISTING_TEXTURE_BINDING,
                                                             mTexId);
        }

        // Push output
        putOutput(0, mFrame);

        // Wait for free output
        return Filter.STATUS_FINISHED;
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mFrame != null) {
            mFrame.release();
        }
    }

}
