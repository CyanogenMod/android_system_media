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
import android.filterfw.core.GLFrame;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;

import android.opengl.GLES20;

public class ResizeFilter extends Filter {

    @FilterParameter(name = "owidth", isOptional = false)
    private int mOWidth;
    @FilterParameter(name = "oheight", isOptional = false)
    private int mOHeight;
    @FilterParameter(name = "generateMipMap", isOptional = true)
    private boolean mGenerateMipMap = false;

    private Program mProgram;
    private MutableFrameFormat mOutputFormat;
    private int mInputChannels;

    public ResizeFilter(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return new String[] { "image" };
    }

    public String[] getOutputNames() {
        return new String[] { "image" };
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        if (format.isBinaryDataType()) {
            mOutputFormat = format.mutableCopy();
            mOutputFormat.setDimensions(mOWidth, mOHeight);
            return true;
        }
        return false;
    }

    public FrameFormat getOutputFormat(int index) {
        return mOutputFormat;
    }

    protected void prepare(FilterContext environment) {
        switch (mOutputFormat.getTarget()) {
            case FrameFormat.TARGET_NATIVE:
                throw new RuntimeException("Native ResizeFilter not implemented yet!");


            case FrameFormat.TARGET_GPU:
                ShaderProgram prog = ShaderProgram.createIdentity();
                mProgram = prog;
                break;

            default:
                throw new RuntimeException("ResizeFilter could not create suitable program!");
        }
    }

    public int process(FilterContext env) {
        // Get input frame
        Frame input = pullInput(0);

        // Create output frame
        Frame output = env.getFrameManager().newFrame(mOutputFormat);

        // Process
        if (mGenerateMipMap) {
            GLFrame mipmapped = (GLFrame)env.getFrameManager().newFrame(input.getFormat());
            mipmapped.setTextureParameter(GLES20.GL_TEXTURE_MIN_FILTER,
                                          GLES20.GL_LINEAR_MIPMAP_NEAREST);
            mipmapped.setDataFromFrame(input);
            mipmapped.generateMipMap();
            mProgram.process(mipmapped, output);
            mipmapped.release();
        } else {
            mProgram.process(input, output);
        }

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }


}
