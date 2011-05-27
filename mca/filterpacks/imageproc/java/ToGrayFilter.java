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
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;

import android.util.Log;

public class ToGrayFilter extends Filter {

    @FilterParameter(name = "outputChannels", isOptional = false)
    private int mOutChannels;
    @FilterParameter(name = "invertSource", isOptional = true)
    private boolean mInvertSource;

    private Program mProgram;
    private MutableFrameFormat mOutputFormat;
    private int mInputChannels;

    private final String mColorToGray4Shader =
            "precision mediump float;\n" +
            "uniform sampler2D tex_sampler_0;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec4 color = texture2D(tex_sampler_0, v_texcoord);\n" +
            "  float y = dot(color, vec4(0.299, 0.587, 0.114, 0));\n" +
            "  gl_FragColor = vec4(y, y, y, 1);\n" +
            "}\n";

    public ToGrayFilter(String name) {
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
            mInputChannels = format.getBytesPerSample();
            mOutputFormat = format.mutableCopy();
            mOutputFormat.setBytesPerSample(mOutChannels);
            return true;
        }
        return false;
    }

    public FrameFormat getOutputFormat(int index) {
        return mOutputFormat;
    }

    public void prepare(FilterContext environment) {
        switch (mOutputFormat.getTarget()) {
            case FrameFormat.TARGET_NATIVE:
                /*switch (mOutChannels) {
                  case 1:
                  mProgram = new NativeProgram("filterpack_imageproc", "color_to_gray1");
                  break;
                  case 3:
                  mProgram = new NativeProgram("filterpack_imageproc", "color_to_gray3");
                  break;
                  case 4:
                  mProgram = new NativeProgram("filterpack_imageproc", "color_to_gray4");
                  break;
                  default:
                  throw new RuntimeException("Unsupported output channels: " + mOutChannels + "!");
                  }
                  mProgram.setHostValue("inputChannels", mInputChannels);
                  break;*/
                throw new RuntimeException("Native toGray not implemented yet!");


            case FrameFormat.TARGET_GPU:
                if (mInputChannels != 4 || mOutChannels != 4) {
                    throw new RuntimeException("Unsupported GL channels: " + mInputChannels + "/" +
                                               mOutChannels +
                                               "(in/out)! Both input and output channels " +
                                               "must be 4!");
                }
                ShaderProgram prog = new ShaderProgram(mColorToGray4Shader);
                if (mInvertSource)
                    prog.setSourceRect(0.0f, 1.0f, 1.0f, -1.0f);
                mProgram = prog;
                break;

            default:
                throw new RuntimeException("ToGrayFilter could not create suitable program!");
        }
    }

    public int process(FilterContext env) {
        // Get input frame
        Frame input = pullInput(0);

        // Create output frame
        MutableFrameFormat outputFormat = input.getFormat().mutableCopy();
        outputFormat.setBytesPerSample(mOutChannels);
        Frame output = env.getFrameManager().newFrame(outputFormat);

        // Process
        mProgram.process(input, output);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }


}
