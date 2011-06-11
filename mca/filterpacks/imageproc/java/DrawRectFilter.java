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

public class DrawRectFilter extends Filter {

    @FilterParameter(name = "colorRed", isOptional = true, isUpdatable = true)
    private float mColorRed = 0.8f;

    @FilterParameter(name = "colorGreen", isOptional = true, isUpdatable = true)
    private float mColorGreen = 0.8f;

    @FilterParameter(name = "colorBlue", isOptional = true, isUpdatable = true)
    private float mColorBlue = 0.0f;

    private ShaderProgram mProgram;

    private final String mVertexShader =
        "attribute vec4 aPosition;\n" +
        "void main() {\n" +
        "  gl_Position = aPosition;\n" +
        "}\n";

    private final String mFixedColorFragmentShader =
        "precision mediump float;\n" +
        "uniform vec4 color;\n" +
        "void main() {\n" +
        "  gl_FragColor = color;\n" +
        "}\n";

    public DrawRectFilter(String name) {
        super(name);
    }

    @Override
    public String[] getInputNames() {
        return new String[] { "image", "box" };
    }

    @Override
    public String[] getOutputNames() {
        return new String[] { "image" };
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        switch(index) {
            case 0: { // image
                FrameFormat requiredFormat = ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                                FrameFormat.TARGET_GPU);
                return format.isCompatibleWith(requiredFormat);
            }

            case 1: // box
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
        mProgram = new ShaderProgram(mVertexShader, mFixedColorFragmentShader);
    }

    @Override
    public int process(FilterContext env) {
        // Get input frame
        Frame imageFrame = pullInput(0);
        Frame boxFrame = pullInput(1);

        // Get the box
        Quad box = (Quad)boxFrame.getObjectValue();
        box = box.scaled(2.0f).translated(-1.0f, -1.0f);

        // Create output frame with copy of input
        GLFrame output = (GLFrame)env.getFrameManager().newFrame(imageFrame.getFormat());
        output.setDataFromFrame(imageFrame);

        // Draw onto output
        output.focus();
        renderBox(box);

        // Push output
        putOutput(0, output);

        // Release pushed frame
        output.release();

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
               Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    private void renderBox(Quad box) {
        final int FLOAT_SIZE = 4;

        // Get current values
        float[] color = {mColorRed, mColorGreen, mColorBlue, 1f};
        float[] vertexValues = { box.p0.x, box.p0.y,
                                 box.p1.x, box.p1.y,
                                 box.p3.x, box.p3.y,
                                 box.p2.x, box.p2.y };

        // Set the program variables
        mProgram.setHostValue("color", color);
        mProgram.setAttributeValues("aPosition", vertexValues, 2);
        mProgram.setVertexCount(4);

        // Draw
        mProgram.beginDrawing();
        GLES20.glLineWidth(1.0f);
        GLES20.glDrawArrays(GLES20.GL_LINE_LOOP, 0, 4);
    }
}
