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
import android.filterfw.core.FilterParameter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.JavaFrame;

public class StringSource extends Filter {

    @FilterParameter(name = "stringValue", isOptional = false)
    private String mString;

    private FrameFormat mOutputFormat;

    public StringSource(String name) {
        super(name);
    }

    @Override
    public String[] getInputNames() {
        return null;
    }

    @Override
    public String[] getOutputNames() {
        return new String[] { "string" };
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return false;
    }

    @Override
    public FrameFormat getOutputFormat(int index) {
        mOutputFormat = new FrameFormat(FrameFormat.TYPE_OBJECT, FrameFormat.TARGET_JAVA);
        return mOutputFormat;
    }

    @Override
    public int process(FilterContext env) {
        Frame output = env.getFrameManager().newEmptyFrame(mOutputFormat);
        output.setObjectValue(mString);
        putOutput(0, output);
        return Filter.STATUS_FINISHED;
    }


}
