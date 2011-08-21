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
import android.filterpacks.imageproc.ImageCombineFilter;
import android.graphics.Bitmap;

import android.util.Log;

/**
 * @hide
 */
public class DoodleFilter extends Filter {

    @GenerateFieldPort(name = "doodle")
    private Bitmap mDoodleBitmap;

    @GenerateFieldPort(name = "tile_size", hasDefault = true)
    private int mTileSize = 640;

    private Program mProgram;
    private Frame mDoodleFrame;

    private int mWidth = 0;
    private int mHeight = 0;
    private int mTarget = FrameFormat.TARGET_UNSPECIFIED;

    private final String mDoodleShader =
            "precision mediump float;\n" +
            "uniform sampler2D tex_sampler_0;\n" +
            "uniform sampler2D tex_sampler_1;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec4 original = texture2D(tex_sampler_0, v_texcoord);\n" +
            "  vec4 mask = texture2D(tex_sampler_1, v_texcoord);\n" +
            "  gl_FragColor = vec4(original.rgb * (1.0 - mask.a) + mask.rgb, 1.0);\n" +
            "}\n";

    public DoodleFilter(String name) {
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
                ShaderProgram shaderProgram = new ShaderProgram(context, mDoodleShader);
                shaderProgram.setMaximumTileSize(mTileSize);
                mProgram = shaderProgram;
                break;

            default:
                throw new RuntimeException("Filter FisheyeFilter does not support frames of " +
                    "target " + target + "!");
        }
        mTarget = target;
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mDoodleFrame != null) {
            mDoodleFrame.release();
            mDoodleFrame = null;
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

        // Check if the frame size has changed
        if (inputFormat.getWidth() != mWidth || inputFormat.getHeight() != mHeight) {
            mWidth = inputFormat.getWidth();
            mHeight = inputFormat.getHeight();

            createDoodleFrame(context);
        }

        // Process
        Frame[] inputs = {input, mDoodleFrame};
        mProgram.process(inputs, output);

        // Push output
        pushOutput("image", output);

        // Release pushed frame
        output.release();
    }

    private void createDoodleFrame(FilterContext context) {
        if (mDoodleBitmap != null) {
            Log.e("DoodleFilter", "create doodle frame " +
                  mDoodleBitmap.getWidth() + " " + mDoodleBitmap.getHeight());

            FrameFormat format = ImageFormat.create(mDoodleBitmap.getWidth(),
                                                    mDoodleBitmap.getHeight(),
                                                    ImageFormat.COLORSPACE_RGBA,
                                                    FrameFormat.TARGET_GPU);

            if (mDoodleFrame != null) {
                mDoodleFrame.release();
            }

            mDoodleFrame = context.getFrameManager().newFrame(format);
            mDoodleFrame.setBitmap(mDoodleBitmap);

            mDoodleBitmap.recycle();
            mDoodleBitmap = null;
        }
    }
}
