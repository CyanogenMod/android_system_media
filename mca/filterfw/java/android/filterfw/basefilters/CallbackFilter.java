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
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;

public class CallbackFilter extends Filter {

    @FilterParameter(name = "listener", isOptional = false, isUpdatable = true)
    private FilterContext.OnFrameReceivedListener mListener;

    @FilterParameter(name = "userData", isOptional = true, isUpdatable = true)
    private Object mUserData;

    public CallbackFilter(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return new String[] { "frame" };
    }

    public String[] getOutputNames() {
        return null;
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return true;
    }

    public FrameFormat getOutputFormat(int index) {
        return null;
    }

    public int process(FilterContext context) {
        // Get frame and forward to listener
        Frame input = pullInput(0);
        if (mListener != null) {
            mListener.onFrameReceived(this, input, mUserData);
        } else {
            throw new RuntimeException("CallbackFilter received frame, but no listener set!");
        }

        return Filter.STATUS_WAIT_FOR_ALL_INPUTS;
    }

}
