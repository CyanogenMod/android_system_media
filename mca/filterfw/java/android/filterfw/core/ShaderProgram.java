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


package android.filterfw.core;

import android.filterfw.core.Frame;
import android.filterfw.core.NativeAllocatorTag;
import android.filterfw.core.Program;
import android.filterfw.core.StopWatchMap;
import android.filterfw.core.VertexFrame;
import android.filterfw.geometry.Quad;
import android.opengl.GLES20;

/**
 * @hide
 */
public class ShaderProgram extends Program {

    private int shaderProgramId;
    private StopWatchMap mTimer = null;

    private void setTimer() {
        mTimer = new StopWatchMap();
    }

    // Used from native layer for creating empty wrapper only!
    private ShaderProgram() {
    }

    private ShaderProgram(NativeAllocatorTag tag) {
    }

    public ShaderProgram(String fragmentShader) {
        allocate(null, fragmentShader);
        if (!compileAndLink()) {
            throw new RuntimeException("Could not compile and link shader!");
        }
        this.setTimer();
    }

    public ShaderProgram(String vertexShader, String fragmentShader) {
        allocate(vertexShader, fragmentShader);
        if (!compileAndLink()) {
            throw new RuntimeException("Could not compile and link shader!");
        }
        this.setTimer();
    }

    public static ShaderProgram createIdentity() {
        ShaderProgram program = nativeCreateIdentity();
        program.setTimer();
        return program;
    }

    @Override
    protected void finalize() throws Throwable {
        deallocate();
    }

    @Override
    public void process(Frame[] inputs, Frame output) {
        if (mTimer.LOG_MFF_RUNNING_TIMES) {
          mTimer.start("glFinish");
          GLES20.glFinish();
          mTimer.stop("glFinish");
        }

        // Get the GL input frames
        // TODO: We do the same in the NativeProgram... can we find a better way?!
        GLFrame[] glInputs = new GLFrame[inputs.length];
        for (int i = 0; i < inputs.length; ++i) {
            if (inputs[i] instanceof GLFrame) {
                glInputs[i] = (GLFrame)inputs[i];
            } else {
                throw new RuntimeException("ShaderProgram got non-GL frame as input " + i + "!");
            }
        }

        // Get the GL output frame
        GLFrame glOutput = null;
        if (output instanceof GLFrame) {
            glOutput = (GLFrame)output;
        } else {
            throw new RuntimeException("ShaderProgram got non-GL output frame!");
        }

        // Process!
        if (!shaderProcess(glInputs, glOutput)) {
            throw new RuntimeException("Error executing ShaderProgram!");
        }

        if (mTimer.LOG_MFF_RUNNING_TIMES) {
          GLES20.glFinish();
        }
    }

    @Override
    public void setHostValue(String variableName, Object value) {
        if (!setUniformValue(variableName, value)) {
            throw new RuntimeException("Error setting uniform value for variable '" +
                                       variableName + "'!");
        }
    }

    @Override
    public Object getHostValue(String variableName) {
        return getUniformValue(variableName);
    }

    public void setAttributeValues(String attributeName, float[] data, int componentCount) {
        if (!setShaderAttributeValues(attributeName, data, componentCount)) {
            throw new RuntimeException("Error setting attribute value for attribute '" +
                                       attributeName + "'!");
        }
    }

    public void setAttributeValues(String attributeName,
                                   VertexFrame vertexData,
                                   int type,
                                   int componentCount,
                                   int strideInBytes,
                                   int offsetInBytes,
                                   boolean normalize) {
        if (!setShaderAttributeVertexFrame(attributeName,
                                           vertexData,
                                           type,
                                           componentCount,
                                           strideInBytes,
                                           offsetInBytes,
                                           normalize)) {
            throw new RuntimeException("Error setting attribute value for attribute '" +
                                       attributeName + "'!");
        }
    }

    public void setSourceRegion(Quad region) {
        setShaderSourceRegion(region.p0.x, region.p0.y,
                              region.p1.x, region.p1.y,
                              region.p2.x, region.p2.y,
                              region.p3.x, region.p3.y);
    }

    public void setTargetRegion(Quad region) {
        setShaderTargetRegion(region.p0.x, region.p0.y,
                              region.p1.x, region.p1.y,
                              region.p2.x, region.p2.y,
                              region.p3.x, region.p3.y);
    }

    public void setSourceRect(float x, float y, float width, float height) {
        setShaderSourceRegion(x, y, x + width, y, x, y + height, x + width, y + height);
    }

    public void setTargetRect(float x, float y, float width, float height) {
        setShaderTargetRegion(x, y, x + width, y, x, y + height, x + width, y + height);
    }

    public void setClearsOutput(boolean clears) {
        if (!setShaderClearsOutput(clears)) {
            throw new RuntimeException("Could not set clears-output flag to " + clears + "!");
        }
    }

    public void setClearColor(float r, float g, float b) {
        if (!setShaderClearColor(r, g, b)) {
            throw new RuntimeException("Could not set clear color to " + r + "," + g + "," + b + "!");
        }
    }

    public void setBlendEnabled(boolean enable) {
        if (!setShaderBlendEnabled(enable)) {
            throw new RuntimeException("Could not set Blending " + enable + "!");
        }
    }

    public void setBlendFunc(int sfactor, int dfactor) {
        if (!setShaderBlendFunc(sfactor, dfactor)) {
            throw new RuntimeException("Could not set BlendFunc " + sfactor +","+ dfactor + "!");
        }
    }

    public void setDrawMode(int drawMode) {
        if (!setShaderDrawMode(drawMode)) {
            throw new RuntimeException("Could not set GL draw-mode to " + drawMode + "!");
        }
    }

    public void setVertexCount(int count) {
        if (!setShaderVertexCount(count)) {
            throw new RuntimeException("Could not set GL vertex count to " + count + "!");
        }
    }

    public void beginDrawing() {
        if (!beginShaderDrawing()) {
            throw new RuntimeException("Could not prepare shader-program for drawing!");
        }
    }

    static {
        System.loadLibrary("filterfw");
    }

    private native boolean allocate(String vertexShader, String fragmentShader);

    private native boolean deallocate();

    private native boolean compileAndLink();

    private native boolean shaderProcess(GLFrame[] inputs, GLFrame output);

    private native boolean setUniformValue(String name, Object value);

    private native Object getUniformValue(String name);

    private native boolean setShaderSourceRegion(float x0, float y0, float x1, float y1,
                                                 float x2, float y2, float x3, float y3);

    private native boolean setShaderTargetRegion(float x0, float y0, float x1, float y1,
                                                 float x2, float y2, float x3, float y3);

    private static native ShaderProgram nativeCreateIdentity();

    private native boolean setShaderClearsOutput(boolean clears);

    private native boolean setShaderBlendEnabled(boolean enable);

    private native boolean setShaderBlendFunc(int sfactor, int dfactor);

    private native boolean setShaderClearColor(float r, float g, float b);

    private native boolean setShaderDrawMode(int drawMode);

    private native boolean setShaderVertexCount(int vertexCount);

    private native boolean beginShaderDrawing();

    private native boolean setShaderAttributeValues(String attributeName,
                                                    float[] data,
                                                    int componentCount);

    private native boolean setShaderAttributeVertexFrame(String attributeName,
                                                         VertexFrame vertexData,
                                                         int type,
                                                         int componentCount,
                                                         int strideInBytes,
                                                         int offsetInBytes,
                                                         boolean normalize);

}
