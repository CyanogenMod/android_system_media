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
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.geometry.Quad;
import android.filterfw.format.ImageFormat;
import android.opengl.GLES20;

public class DrawOverlayFilter extends Filter {

    private ShaderProgram mProgram;

    public DrawOverlayFilter(String name) {
        super(name);
    }

    @Override
    public String[] getInputNames() {
        return new String[] { "source", "overlay", "box" };
    }

    @Override
    public String[] getOutputNames() {
        return new String[] { "image" };
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        switch(index) {
            case 0: { // source
                FrameFormat requiredFormat = ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                                FrameFormat.TARGET_GPU);
                return format.isCompatibleWith(requiredFormat);
            }

            case 1: { // overlay
                FrameFormat requiredFormat = ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                                FrameFormat.TARGET_GPU);
                return format.isCompatibleWith(requiredFormat);
            }

            case 2: // box
                return (format.getTarget() == FrameFormat.TARGET_JAVA &&
                        format.getBaseType() == FrameFormat.TYPE_OBJECT &&
                        Quad.class.isAssignableFrom(format.getObjectClass()));
        }
        return false;
    }

    @Override
    public FrameFormat getOutputFormat(int index) {
        return getInputFormat(0);
    }

    @Override
    public void prepare(FilterContext env) {
        mProgram = ShaderProgram.createIdentity();
    }

    @Override
    public int process(FilterContext env) {
        // Get input frame
        Frame sourceFrame = pullInput(0);
        Frame overlayFrame = pullInput(1);
        Frame boxFrame = pullInput(2);

        // Get the box
        Quad box = (Quad)boxFrame.getObjectValue();
        box = box.translated(1.0f, 1.0f).scaled(2.0f);

        mProgram.setTargetRegion(box);

        // Create output frame with copy of input
        Frame output = env.getFrameManager().newFrame(sourceFrame.getFormat());
        output.setDataFromFrame(sourceFrame);

        // Draw onto output
        mProgram.process(overlayFrame, output);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
               Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }
}
