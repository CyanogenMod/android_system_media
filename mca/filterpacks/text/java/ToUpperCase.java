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


package android.filterpacks.text;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.JavaFrame;

public class ToUpperCase extends Filter {

    private FrameFormat mOutputFormat;

    public ToUpperCase(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return new String[] { "mixedcase" };
    }

    public String[] getOutputNames() {
        return new String[] { "uppercase" };
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        // TODO: Check meta-property ObjectClass
        if (format.getBaseType() == FrameFormat.TYPE_OBJECT) {
            mOutputFormat = format;
            return true;
        }
        return false;
    }

    public FrameFormat getOutputFormat(int index) {
        return mOutputFormat;
    }

    public int process(FilterContext env) {
        Frame input = pullInput(0);
        String inputString = (String)input.getObjectValue();

        Frame output = env.getFrameManager().newEmptyFrame(mOutputFormat);
        output.setObjectValue(inputString.toUpperCase());

        putOutput(0, output);

        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

}
