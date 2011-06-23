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
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.GenerateFieldPort;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;

public class CallbackFilter extends Filter {

    @GenerateFieldPort(name = "listener", hasDefault = true)
    private FilterContext.OnFrameReceivedListener mListener;

    @GenerateFieldPort(name = "userData", hasDefault = true)
    private Object mUserData;

    public CallbackFilter(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        addInputPort("frame");
    }

    public void process(FilterContext context) {
        // Get frame and forward to listener
        Frame input = pullInput("frame");
        if (mListener != null) {
            mListener.onFrameReceived(this, input, mUserData);
        } else {
            throw new RuntimeException("CallbackFilter received frame, but no listener set!");
        }
    }

}
