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
import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.format.ImageFormat;

import java.util.HashSet;
import java.util.Set;

public abstract class ImageFilter extends Filter {

    public ImageFilter(String name) {
        super(name);
    }

    @Override
    public String[] getInputNames() {
        return new String[] { "image" };
    }

    @Override
    public String[] getOutputNames() {
        return new String[] { "image" };
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        FrameFormat requiredFormat = ImageFormat.create(ImageFormat.COLORSPACE_RGBA);
        return format.isCompatibleWith(requiredFormat);
    }

    @Override
    public FrameFormat getOutputFormat(int index) {
        return getInputFormat(0);
    }

    @Override
    public void prepare(FilterContext environment) {
        // Make sure this is a valid ImageFilter
        if (getNumberOfInputs() == 0) {
            throw new RuntimeException("Invalid ImageFilter! Must have at least 1 input port!");
        } else if (getNumberOfOutputs() != 1) {
            throw new RuntimeException("Invalid ImageFilter! Must have exactly 1 output port!");
        }
        // Create proper program
        createProgram(getInputFormat(0).getTarget());
    }

    @Override
    public int process(FilterContext env) {
        // Get input frame
        int inputCount = getNumberOfInputs();
        Frame[] inputs = new Frame[inputCount];
        for (int i = 0; i < inputCount; ++i) {
            inputs[i] = pullInput(i);
        }

        // Create output frame
        Frame output = env.getFrameManager().newFrame(inputs[0].getFormat());

        // Process
        getProgram().process(inputs, output);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    protected abstract void createProgram(int target);

    protected abstract Program getProgram();
}
