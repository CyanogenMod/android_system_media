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

import android.util.Log;

public class FrameFetch extends Filter {

    @FilterParameter(name = "key", isOptional = false)
    private String mKey;

    @FilterParameter(name = "format", isOptional = true)
    private FrameFormat mFormat;

    @FilterParameter(name = "repeatFrame", isOptional = true)
    private boolean mRepeatFrame = false;

    public FrameFetch(String name) {
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
        if (mFormat != null) {
            return mFormat;
        } else {
            return new FrameFormat(FrameFormat.TYPE_UNSPECIFIED, FrameFormat.TARGET_UNSPECIFIED);
        }
    }

    public int process(FilterContext context) {
        Frame output = context.fetchFrame(mKey);
        if (output != null) {
            putOutput(0, output);
            return mRepeatFrame ? Filter.STATUS_WAIT_FOR_FREE_OUTPUTS : Filter.STATUS_FINISHED;
        } else {
            return Filter.STATUS_SLEEP | Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
        }
    }

}
