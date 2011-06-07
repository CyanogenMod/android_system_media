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
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;

public class RetargetFilter extends Filter {

    @FilterParameter(name = "target", isOptional = false)
    private String mTargetString;

    private MutableFrameFormat mOutputFormat;
    private int mTarget = -1;

    public RetargetFilter(String name) {
        super(name);
    }

    public void initFilter() {
        if (mTargetString.equals("CPU") || mTargetString.equals("NATIVE")) {
            mTarget = FrameFormat.TARGET_NATIVE;
        } else if (mTargetString.equals("GPU")) {
            mTarget = FrameFormat.TARGET_GPU;
        } else {
            throw new RuntimeException("Unknown or unsupported target type '" +
                                       mTargetString + "'!");
        }
    }

    public String[] getInputNames() {
        return new String[] { "frame" };
    }

    public String[] getOutputNames() {
        return new String[] { "frame" };
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return true;
    }

    public FrameFormat getOutputFormat(int index) {
        return getRetargetedFormat(getInputFormat(0));
    }

    public FrameFormat getRetargetedFormat(FrameFormat format) {
        MutableFrameFormat retargeted = format.mutableCopy();
        retargeted.setTarget(mTarget);
        return retargeted;
    }

    public int process(FilterContext context) {
        // Get input frame
        Frame input = pullInput(0);

        // Create output frame
        FrameFormat outFormat = getRetargetedFormat(input.getFormat());
        Frame output = context.getFrameManager().newFrame(outFormat);

        // Process
        output.setDataFromFrame(input);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

}
