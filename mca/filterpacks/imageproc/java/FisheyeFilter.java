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

import android.util.Log;

import java.util.Set;

public class FisheyeFilter extends SimpleImageFilter {
    private static final String TAG = "FisheyeFilter";

    // This parameter has range between 0 and 1. It controls the effect of radial distortion.
    // The larger the value, the more prominent the distortion effect becomes (a straight line
    // becomes a curve).
    @GenerateFieldPort(name = "scale")
    private float mScale;

    // focus_x and focus_y are set to be the center of image for now.
    // @FilterParameter(name = "focus_x", isOptional = true, isUpdatable = false)
    private float mFocusX;
    // @FilterParameter(name = "focus_y", isOptional = true, isUpdatable = false)
    private float mFocusY;

    private static final String mFisheyeShader =
            "precision mediump float;\n" +
            "uniform sampler2D tex_sampler_0;\n" +
            "uniform vec2 center;\n" +
            "uniform float height;\n" +
            "uniform float width;\n" +
            "uniform float alpha;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  const float m_pi_2 = 1.570963;\n" +
            "  float bound = length(center);\n" +
            "  float bound2 = bound * bound;\n" +
            "  float radius = 1.15 * bound;\n" +
            "  float radius2 =  radius * radius;\n" +
            "  float max_radian = m_pi_2 - atan(alpha * sqrt(radius2 - bound2), bound);\n" +
            "  float dist = distance(gl_FragCoord.xy, center);\n" +
            "  float dist2 = dist * dist;\n" +
            "  float radian = m_pi_2 - atan(alpha * sqrt(radius2 - dist2), dist);\n" +
            "  float scale = radian / max_radian * bound / dist;\n" +
            "  vec2 new_coord = gl_FragCoord.xy * scale + (1.0 - scale) * center;\n" +
            "  new_coord.x /= width;\n" +
            "  new_coord.y /= height;\n" +
            "  vec4 color = texture2D(tex_sampler_0, new_coord);\n" +
            "  gl_FragColor = color;\n" +
            "}\n";

    public FisheyeFilter(String name) {
        super(name, null);
    }

    @Override
    protected Program getNativeProgram() {
        Program result = new NativeProgram("filterpack_imageproc", "fisheye");
        initProgram(result);
        return result;
    }

    @Override
    protected Program getShaderProgram() {
        Program result = new ShaderProgram(mFisheyeShader);
        initProgram(result);
        return result;
    }

    @Override
    public void fieldPortValueUpdated(String name, FilterContext context) {
        if (mProgram != null) {
          Log.v(TAG, "alpha=" + (mScale * 2.0 + 0.75));
          mProgram.setHostValue("alpha", (float) (mScale * 2.0 + 0.75));
        }
    }

    @Override
    public void process(FilterContext env) {
        // Get input frame
        Frame input = pullInput("image");

        // Create output frame
        // TODO: Use Frame Provider
        Frame output = env.getFrameManager().newFrame(input.getFormat());

        Log.e("Fisheye", "width: " + input.getFormat().getWidth() +
              ", height: " + input.getFormat().getHeight());

        mFocusX = (float) (0.5 * input.getFormat().getWidth());
        mFocusY = (float) (0.5 * input.getFormat().getHeight());

        float center[] = {mFocusX, mFocusY};

        Log.e("Fisheye", "( " + center[0] + ", " + center[1] + " )");

        // Make sure we have a program
        updateProgramWithTarget(input.getFormat().getTarget());

        // set uniforms in shader
        mProgram.setHostValue("center", center);
        mProgram.setHostValue("width", (float) input.getFormat().getWidth());
        mProgram.setHostValue("height", (float) input.getFormat().getHeight());

        // Process
        mProgram.process(input, output);

        // Push output
        pushOutput("image", output);

        // Release output
        output.release();
    }

    private void initProgram(Program program) {
        FrameFormat outFormat = getInputFormat("image");
        mFocusX = (float) (0.5 * outFormat.getWidth());
        mFocusY = (float) (0.5 * outFormat.getHeight());

        float center[] = {mFocusX, mFocusY};

        Log.e("Fisheye", "( " + center[0] + ", " + center[1] + " )");

        // set uniforms in shader
        program.setHostValue("center", center);
        program.setHostValue("width", (float) outFormat.getWidth());
        program.setHostValue("height", (float) outFormat.getHeight());
        program.setHostValue("alpha", (float) (mScale * 2.0 + 0.75));
    }

}
