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

import java.util.Set;

public class GLTextureTarget extends Filter {

    @FilterParameter(name = "texId", isOptional = false, isUpdatable = true)
    private int mTexId;

    @FilterParameter(name = "createTex", isOptional = true)
    private boolean mCreateTex = false;

    private FrameFormat mInputFormat;
    private MutableFrameFormat mFrameFormat;
    private Frame mFrame;

    public GLTextureTarget(String name) {
        super(name);
    }

    @Override
    public String[] getInputNames() {
        return new String[] { "frame" };
    }

    @Override
    public String[] getOutputNames() {
        return null;
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        if (format.getDimensionCount() == 2 && format.getBytesPerSample() == 4) {
            mInputFormat = format;
            return true;
        }
        return false;
    }

    @Override
    public FrameFormat getOutputFormat(int index) {
        return null;
    }

    @Override
    public void parametersUpdated(Set<String> updated) {
        if (mFrame != null) {
            mFrame.release();
            mFrame = null;
        }
    }

    public void prepare(FilterContext context) {
        mFrameFormat = new MutableFrameFormat(FrameFormat.TYPE_BYTE, FrameFormat.TARGET_GPU);
        mFrameFormat.setBytesPerSample(4);
        mFrameFormat.setDimensions(mInputFormat.getDimensions());
    }

    @Override
    public int process(FilterContext context) {
        // Get input frame
        Frame input = pullInput(0);

        // Generate frame if not generated already
        if (mFrame == null) {
            int binding = mCreateTex
                ? GLFrame.NEW_TEXTURE_BINDING
                : GLFrame.EXISTING_TEXTURE_BINDING;
            mFrame = context.getFrameManager().newBoundFrame(mFrameFormat, binding, mTexId);
        }

        // Copy to our texture frame
        mFrame.setDataFromFrame(input);

        // Wait for free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS;
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mFrame != null) {
            mFrame.release();
        }
    }
}
