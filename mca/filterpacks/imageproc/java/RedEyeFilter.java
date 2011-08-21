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
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.format.ImageFormat;
import android.graphics.Bitmap;

import java.util.Set;

import android.util.Log;

/**
 * @hide
 */
public class RedEyeFilter extends Filter {

    @GenerateFieldPort(name = "intensity")
    private float mIntensity;

    @GenerateFieldPort(name = "redeye")
    private Bitmap mRedEyeBitmap = null;

    @GenerateFieldPort(name = "update")
    private boolean mUpdate;

    @GenerateFieldPort(name = "tile_size", hasDefault = true)
    private int mTileSize = 640;

    private Frame mRedEyeFrame;

    private int mWidth = 0;
    private int mHeight = 0;

    private Program mProgram;
    private int mTarget = FrameFormat.TARGET_UNSPECIFIED;

    private final String mRedEyeShader =
            "precision mediump float;\n" +
            "uniform sampler2D tex_sampler_0;\n" +
            "uniform sampler2D tex_sampler_1;\n" +
            "uniform float intensity;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec4 color = texture2D(tex_sampler_0, v_texcoord);\n" +
            "  vec4 mask = texture2D(tex_sampler_1, v_texcoord);\n" +
            "  gl_FragColor = vec4(mask.a, mask.a, mask.a, 1.0) * intensity + color * (1.0 - intensity);\n" +
            "  if (mask.a > 0.0) {\n" +
            "    gl_FragColor.r = 0.0;\n" +
            "    float green_blue = color.g + color.b;\n" +
            "    float red_intensity = color.r / green_blue;\n" +
            "    if (red_intensity > intensity) {\n" +
            "      color.r = 0.5 * green_blue;\n" +
            "    }\n" +
            "  }\n" +
            "  gl_FragColor = color;\n" +
            "}\n";

    public RedEyeFilter(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        addMaskedInputPort("image", ImageFormat.create(ImageFormat.COLORSPACE_RGBA));
        addOutputBasedOnInput("image", "image");
    }

    @Override
    public FrameFormat getOutputFormat(String portName, FrameFormat inputFormat) {
        return inputFormat;
    }

    public void initProgram(FilterContext context, int target) {
        switch (target) {
            case FrameFormat.TARGET_GPU:
                ShaderProgram shaderProgram = new ShaderProgram(context, mRedEyeShader);
                shaderProgram.setMaximumTileSize(mTileSize);
                mProgram = shaderProgram;
                break;

            default:
                throw new RuntimeException("Filter RedEye does not support frames of " +
                    "target " + target + "!");
        }
        mTarget = target;
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mRedEyeFrame != null) {
            mRedEyeFrame.release();
            mRedEyeFrame = null;
        }
    }

    @Override
    public void process(FilterContext context) {
        // Get input frame
        Frame input = pullInput("image");
        FrameFormat inputFormat = input.getFormat();

        // Create output frame
        Frame output = context.getFrameManager().newFrame(inputFormat);

        // Create program if not created already
        if (mProgram == null || inputFormat.getTarget() != mTarget) {
            initProgram(context, inputFormat.getTarget());
        }

        if (mUpdate)
            updateProgramParams(context);

        // Process
        Frame[] inputs = {input, mRedEyeFrame};
        mProgram.process(inputs, output);

        // Push output
        pushOutput("image", output);

        // Release pushed frame
        output.release();
    }

    private void updateProgramParams(FilterContext context) {
        mProgram.setHostValue("intensity", mIntensity);

        if (mRedEyeBitmap != null) {

            // TODO(rslin): Can I reuse the existing frame instead of creating
            // a new frame each time?
            FrameFormat format = ImageFormat.create(mRedEyeBitmap.getWidth(),
                                                    mRedEyeBitmap.getHeight(),
                                                    ImageFormat.COLORSPACE_RGBA,
                                                    FrameFormat.TARGET_GPU);
            if (mRedEyeFrame != null) {
                mRedEyeFrame.release();
            }
            mRedEyeFrame = context.getFrameManager().newFrame(format);
            mRedEyeFrame.setBitmap(mRedEyeBitmap);
        }
    }
}
