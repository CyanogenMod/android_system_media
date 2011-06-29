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
import android.filterfw.core.GenerateFieldPort;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.format.ImageFormat;

import android.util.Log;

/**
 * @hide
 */
public class ToPackedGrayFilter extends Filter {

    @GenerateFieldPort(name = "owidth", hasDefault = true)
    private int mOWidth = FrameFormat.SIZE_UNSPECIFIED;
    @GenerateFieldPort(name = "oheight", hasDefault = true)
    private int mOHeight = FrameFormat.SIZE_UNSPECIFIED;

    private Program mProgram;

    private final String mColorToPackedGrayShader =
        "precision mediump float;\n" +
        "const vec4 coeff_y = vec4(0.299, 0.587, 0.114, 0);\n" +
        "uniform sampler2D tex_sampler_0;\n" +
        "uniform float pix_stride;\n" +
        "varying vec2 v_texcoord;\n" +
        "void main() {\n" +
        "  for (int i = 0; i < 4; ++i) {\n" +
        "    vec4 p = texture2D(tex_sampler_0,\n" +
        "                       v_texcoord + vec2(pix_stride * float(i), 0.0));\n" +
        "    gl_FragColor[i] = dot(p, coeff_y);\n" +
        "  }\n" +
        "}\n";

    public ToPackedGrayFilter(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        addMaskedInputPort("image", ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                       FrameFormat.TARGET_GPU));
        addOutputBasedOnInput("image", "image");
    }

    @Override
    public FrameFormat getOutputFormat(String portName, FrameFormat inputFormat) {
        return convertInputFormat(inputFormat);
    }

    private void checkOutputDimensions() {
        if (mOWidth % 4 != 0) {
            throw new RuntimeException("Output width not divisible by four: " + mOWidth);
        }
        if (mOWidth <= 0 || mOHeight <= 0) {
            throw new RuntimeException("Invalid output dimensions: " + mOWidth + " " + mOHeight);
        }
    }

    private FrameFormat convertInputFormat(FrameFormat inputFormat) {
        if (mOWidth == FrameFormat.SIZE_UNSPECIFIED) {
            mOWidth = inputFormat.getWidth();
        }
        if (mOHeight == FrameFormat.SIZE_UNSPECIFIED) {
            mOHeight = inputFormat.getHeight();
        }
        return ImageFormat.create(mOWidth, mOHeight,
                                  ImageFormat.COLORSPACE_GRAY,
                                  FrameFormat.TARGET_NATIVE);
    }

    @Override
    public void prepare(FilterContext context) {
        mProgram = new ShaderProgram(mColorToPackedGrayShader);
    }

    @Override
    public void process(FilterContext context) {
        Frame input = pullInput("image");
        FrameFormat inputFormat = input.getFormat();
        FrameFormat outputFormat = convertInputFormat(inputFormat);
        checkOutputDimensions();
        mProgram.setHostValue("pix_stride", 1.0f / mOWidth);

        // Do the RGBA to luminance conversion.
        MutableFrameFormat tempFrameFormat = inputFormat.mutableCopy();
        tempFrameFormat.setDimensions(mOWidth / 4, mOHeight);
        Frame temp = context.getFrameManager().newFrame(tempFrameFormat);
        mProgram.process(input, temp);

        // Read frame from GPU to CPU.
        Frame output = context.getFrameManager().newFrame(outputFormat);
        output.setDataFromFrame(temp);
        temp.release();

        // Push output and yield ownership.
        pushOutput("image", output);
        output.release();
    }

}
