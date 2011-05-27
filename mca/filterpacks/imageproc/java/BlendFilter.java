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

import java.util.Set;

public class BlendFilter extends Filter {

    @FilterParameter(name = "blend", isOptional = false, isUpdatable = true)
    private float mBlend;

    private Program mProgram;
    private FrameFormat mOutputFormat;

    private final String mBlendShader =
            "precision mediump float;\n" +
            "uniform sampler2D tex_sampler_0;\n" +
            "uniform sampler2D tex_sampler_1;\n" +
            "uniform float blend;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec4 colorL = texture2D(tex_sampler_0, v_texcoord);\n" +
            "  vec4 colorR = texture2D(tex_sampler_1, v_texcoord);\n" +
            "  gl_FragColor = colorL * (1.0 - blend) + colorR * blend;\n" +
            "}\n";

    public BlendFilter(String name) {
        super(name);
    }

    public String[] getInputNames() {
        return new String[] { "left", "right" };
    }

    public String[] getOutputNames() {
        return new String[] { "blended" };
    }

    public boolean acceptsInputFormat(int index, FrameFormat format) {
        if (format.isBinaryDataType() && format.getBytesPerSample() == 4) {
            mOutputFormat = format;
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
                throw new RuntimeException("TODO: Write native implementation for Blend!");

            case FrameFormat.TARGET_GPU:
                mProgram = new ShaderProgram(mBlendShader);
                break;

            default:
                throw new RuntimeException("BlendFilter could not create suitable program!");
        }
        mProgram.setHostValue("blend", mBlend);
    }

    public void parametersUpdated(Set<String> updated) {
        if (mProgram != null) {
            mProgram.setHostValue("blend", mBlend);
        }
    }

    public int process(FilterContext env) {
        // Get input frame
        Frame[] inputs = { pullInput(0), pullInput(1) };

        // Create output frame
        Frame output = env.getFrameManager().newFrame(inputs[0].getFormat());

        // Process
        mProgram.process(inputs, output);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

}
