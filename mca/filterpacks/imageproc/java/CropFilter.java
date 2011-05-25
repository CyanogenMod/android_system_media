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


package android.filterpacks.imageproc;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterEnvironment;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.geometry.Point;
import android.filterfw.geometry.Quad;

import android.util.Log;

public class CropFilter extends Filter {

    private ShaderProgram mProgram;
    private FrameFormat mOutputFormat;

    public CropFilter(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return new String[] { "image", "box" };
    }

    public String[] getOutputNames() {
        return new String[] { "image" };
    }

    public boolean setInputFormat(int index, FrameFormat format) {
        switch(index) {
            case 0: // image
                if (format.isBinaryDataType() &&
                    format.getTarget() == FrameFormat.TARGET_GPU) {
                    mOutputFormat = format;
                    return true;
                }
                return false;

            case 1: // box
                return (format.getTarget() == FrameFormat.TARGET_JAVA &&
                        format.getBaseType() == FrameFormat.TYPE_OBJECT &&
                        Quad.class.isAssignableFrom(format.getObjectClass()));
        }
        return false;
    }

    public FrameFormat getFormatForOutput(int index) {
        return mOutputFormat;
    }

    public void prepare(FilterEnvironment env) {
        mProgram = ShaderProgram.createIdentity();
    }

    public int process(FilterEnvironment env) {
        // Get input frame
        Frame imageFrame = pullInput(0);
        Frame boxFrame = pullInput(1);

        // Get the box
        Quad box = (Quad)boxFrame.getObjectValue();

        // Create output frame
        Frame output = env.getFrameManager().newFrame(mOutputFormat);

        mProgram.setSourceRegion(box);
        mProgram.process(imageFrame, output);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
               Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }


}
