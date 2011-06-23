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
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.format.ImageFormat;

import android.util.Log;

public class ToRGBAFilter extends Filter {

    private int mInputBPP;
    private Program mProgram;

    public ToRGBAFilter(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        MutableFrameFormat mask = new MutableFrameFormat(FrameFormat.TYPE_BYTE,
                                                         FrameFormat.TARGET_NATIVE);
        mask.setDimensionCount(2);
        addMaskedInputPort("image", mask);
        addOutputBasedOnInput("image", "image");
    }

    @Override
    public FrameFormat getOutputFormat(String portName, FrameFormat inputFormat) {
        return getConvertedFormat(inputFormat);
    }

    public FrameFormat getConvertedFormat(FrameFormat format) {
        MutableFrameFormat result = format.mutableCopy();
        result.setMetaValue(ImageFormat.COLORSPACE_KEY, ImageFormat.COLORSPACE_RGBA);
        result.setBytesPerSample(4);
        return result;
    }

    @Override
    public void prepare(FilterContext context) {
        mInputBPP = getInputFormat("image").getBytesPerSample();
        switch (mInputBPP) {
            case 1:
                mProgram = new NativeProgram("filterpack_imageproc", "gray_to_rgba");
                break;
            case 3:
                mProgram = new NativeProgram("filterpack_imageproc", "rgb_to_rgba");
                break;
            default:
                throw new RuntimeException("Unsupported BytesPerPixel: " + mInputBPP + "!");
        }
    }

    @Override
    public void process(FilterContext context) {
        // Get input frame
        Frame input = pullInput("image");

        // Create output frame
        Frame output = context.getFrameManager().newFrame(getConvertedFormat(input.getFormat()));

        // Process
        mProgram.process(input, output);

        // Push output
        pushOutput("image", output);

        // Release pushed frame
        output.release();
    }

}
