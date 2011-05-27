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

public class FrameBranch extends Filter {

    @FilterParameter(name = "outputs", isOptional = true)
    private int mNumberOfOutputs = 2;

    private FrameFormat mOutputFormat;

    public FrameBranch(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return new String[] { "in" };
    }

    public String[] getOutputNames() {
        String[] outNames = new String[mNumberOfOutputs];
        for (int i = 0; i < mNumberOfOutputs; ++i) {
            outNames[i] = "out" + i;
        }
        return outNames;
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        mOutputFormat = format;
        return true;
    }

    public FrameFormat getOutputFormat(int index) {
        return mOutputFormat;
    }

    public int process(FilterContext context) {
        // Get input frame
        Frame input = pullInput(0);

        // Push output
        for (int i = 0; i < mNumberOfOutputs; ++i) {
            putOutput(i, input);
        }

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

}
