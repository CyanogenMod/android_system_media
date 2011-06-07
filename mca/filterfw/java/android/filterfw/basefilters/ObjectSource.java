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

import java.util.Set;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.MutableFrameFormat;

public class ObjectSource extends Filter {

    @FilterParameter(name = "object", isOptional = true, isUpdatable = true)
    private Object mObject;

    @FilterParameter(name = "format", isOptional = false, isUpdatable = true)
    private FrameFormat mFormat;

    @FilterParameter(name = "repeatFrame", isOptional = true)
    boolean repeatFrame = false;

    private MutableFrameFormat mOutputFormat;
    private Frame mFrame;

    public ObjectSource(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return null;
    }

    public String[] getOutputNames() {
        return new String[] { "frame" };
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return false;
    }

    public FrameFormat getOutputFormat(int index) {
        updateOutputFormat();
        return mOutputFormat;
    }

    private void updateOutputFormat() {
        mOutputFormat = mFormat.mutableCopy();
        if (mObject != null) {
            mOutputFormat.setObjectClass(mObject.getClass());
        }
    }

    public int process(FilterContext context) {
        // If no frame has been created, create one now.
        if (mFrame == null) {
            if (mObject == null) {
                throw new NullPointerException("ObjectSource producing frame with no object set!");
            }
            updateOutputFormat();
            mFrame = context.getFrameManager().newEmptyFrame(mOutputFormat);
            mFrame.setObjectValue(mObject);
        }

        // Push output
        putOutput(0, mFrame);

        // Wait for free output
        return repeatFrame ? Filter.STATUS_WAIT_FOR_FREE_OUTPUTS : Filter.STATUS_FINISHED;
    }

    public void tearDown(FilterContext context) {
        mFrame.release();
    }

    @Override
    public void parametersUpdated(Set<String> updated) {
        // Release our internal frame, so that it is regenerated on the next call to process().
        if (mFrame != null) {
            mFrame.release();
            mFrame = null;
        }
    }
}
