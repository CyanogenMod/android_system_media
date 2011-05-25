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


package android.filterpacks.ui;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterEnvironment;
import android.filterfw.core.FilterParameter;
import android.filterfw.core.FilterSurfaceView;
import android.filterfw.core.FilterSurfaceRenderer;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.GLEnvironment;
import android.filterfw.core.GLFrame;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeProgram;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import android.graphics.Rect;

import android.util.Log;

public class SurfaceRenderFilter extends Filter implements FilterSurfaceRenderer {

    private final int RENDERMODE_STRETCH   = 0;
    private final int RENDERMODE_FIT       = 1;
    private final int RENDERMODE_FILL_CROP = 2;

    /** Required. Sets the destination filter surface view for this
     * node.
     */
    @FilterParameter(name = "surfaceView", isOptional = false)
    private FilterSurfaceView mSurfaceView;

    /** Optional. Control how the incoming frames are rendered onto the
     * output. Default is FIT.
     * RENDERMODE_STRETCH: Just fill the output surfaceView.
     * RENDERMODE_FIT: Keep aspect ratio and fit without cropping. May
     * have black bars.
     * RENDERMODE_FILL_CROP: Keep aspect ratio and fit without black
     * bars. May crop.
     */
    @FilterParameter(name = "renderMode", isOptional = true)
    private String mRenderModeString;

    private ShaderProgram mProgram;
    private GLFrame mScreen;
    private int mRenderMode = RENDERMODE_FIT;
    private float mAspectRatio;

    private int mScreenWidth;
    private int mScreenHeight;

    private static final boolean LOGV = false;
    private static final String TAG = "SurfaceRenderFilter";

    public SurfaceRenderFilter(String name) {
        super(name);
    }

    protected void finalize() throws Throwable {
        if (mSurfaceView != null) {
            mSurfaceView.unbind();
        }
    }

    @Override
    public void initFilter() {
        // SurfaceView
        if (mSurfaceView == null) {
            throw new RuntimeException("NULL SurfaceView passed to SurfaceRenderFilter");
        }

        // RenderMode
        if (mRenderModeString != null) {
            if (mRenderModeString == "stretch") {
                mRenderMode = RENDERMODE_STRETCH;
            } else if (mRenderModeString == "fit") {
                mRenderMode = RENDERMODE_FIT;
            } else if (mRenderModeString == "fill_crop") {
                mRenderMode = RENDERMODE_FILL_CROP;
            } else {
                throw new RuntimeException("Unknown render mode '" + mRenderModeString + "'!");
            }
        }

        mAspectRatio = 1.f;
    }

    @Override
    public String[] getInputNames() {
        return new String[] { "frame" };
    }

    @Override
    public String[] getOutputNames() {
        return null;
    }

    @Override
    public boolean setInputFormat(int index, FrameFormat format) {
        if (format.isBinaryDataType() &&
            format.getBytesPerSample() == 4 &&
            format.getNumberOfDimensions() == 2 &&
            (format.getTarget() == FrameFormat.TARGET_NATIVE ||
             format.getTarget() == FrameFormat.TARGET_GPU)) {

            return true;
        }
        return false;
    }

    @Override
    public FrameFormat getFormatForOutput(int index) {
        return null;
    }

    @Override
    public void prepare(FilterEnvironment environment) {
        // Create identity shader to render, and make sure to render upside-down, as textures
        // are stored internally bottom-to-top.
        mProgram = ShaderProgram.createIdentity();
        mProgram.setSourceRect(0, 1, 1, -1);
        mProgram.setClearsOutput(true);
        mProgram.setClearColor(0.0f, 0.0f, 0.0f);

        updateTargetRect();

        // Create a frame representing the screen
        MutableFrameFormat screenFormat = new MutableFrameFormat(FrameFormat.TYPE_BYTE,
                                                                 FrameFormat.TARGET_GPU);
        screenFormat.setBytesPerSample(4);
        screenFormat.setDimensions(mSurfaceView.getWidth(), mSurfaceView.getHeight());
        mScreen = (GLFrame)environment.getFrameManager().newBoundFrame(screenFormat,
                                                                       GLFrame.EXISTING_FBO_BINDING,
                                                                       0);

        // Bind surface view to us. This will emit a surfaceChanged call that will update our
        // screen width and height.
        if (mSurfaceView != null) {
            mSurfaceView.unbind();
            mSurfaceView.bindToRenderer(this, environment.getGLEnvironment());
        }
    }

    @Override
    public int process(FilterEnvironment env) {
        if (mSurfaceView != null) {
            if (LOGV) Log.v(TAG, "Starting frame processing");

            // Make sure we are processing in the correct GL environment
            GLEnvironment glEnv = mSurfaceView.getGLEnv();
            if (glEnv != env.getGLEnvironment()) {
                throw new RuntimeException("Surface created under different GLEnvironment!");
            }

            // Get input frame
            Frame input = pullInput(0);
            boolean createdFrame = false;

            float currentAspectRatio = (float)input.getFormat().getWidth() / input.getFormat().getHeight();
            if (currentAspectRatio != mAspectRatio) {
                if (LOGV) Log.v(TAG, "New aspect ratio: " + currentAspectRatio +", previously: " + mAspectRatio);
                mAspectRatio = currentAspectRatio;
                updateTargetRect();
            }

            // See if we need to copy to GPU
            Frame gpuFrame = null;
            if (input.getFormat().getTarget() == FrameFormat.TARGET_NATIVE) {
                MutableFrameFormat gpuFormat = input.getFormat().mutableCopy();
                gpuFormat.setTarget(FrameFormat.TARGET_GPU);
                gpuFrame = env.getFrameManager().newFrame(gpuFormat);
                gpuFrame.setData(input.getData());
                createdFrame = true;
            } else {
                gpuFrame = input;
            }

            // Activate our surface
            glEnv.activateSurfaceWithId(mSurfaceView.getSurfaceId());

            // Process
            mProgram.process(gpuFrame, mScreen);

            // And swap buffers
            glEnv.swapBuffers();

            if (createdFrame) {
                gpuFrame.release();
            }
        }

        // Wait for next input and free output
        return Filter.STATUS_WAIT_FOR_ALL_INPUTS |
                Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    @Override
    public void tearDown(FilterEnvironment env) {
        if (mScreen != null) {
            mScreen.release();
        }
    }

    @Override
    public synchronized void surfaceChanged(int format, int width, int height) {
        // If the screen is null, we do not care about surface changes (yet). Once we have a
        // screen object, we need to keep track of these changes.
        if (mScreen != null) {
            mScreenWidth = width;
            mScreenHeight = height;
            mScreen.setViewport(0, 0, mScreenWidth, mScreenHeight);
            updateTargetRect();
        }
    }

    public synchronized void surfaceDestroyed() {
        // We do nothing here but declare this synchronized so that the surface is not destroyed
        // behind our backs.
    }

    private void updateTargetRect() {
        if (mScreenWidth > 0 && mScreenHeight > 0 && mProgram != null) {
            float screenAspectRatio = (float)mScreenWidth / mScreenHeight;
            float relativeAspectRatio = screenAspectRatio / mAspectRatio;

            switch (mRenderMode) {
                case RENDERMODE_STRETCH:
                    mProgram.setTargetRect(0, 0, 1, 1);
                    break;
                case RENDERMODE_FIT:
                    if (relativeAspectRatio > 1.0f) {
                        // Screen is wider than the camera, scale down X
                        mProgram.setTargetRect(0.5f - 0.5f / relativeAspectRatio, 0.0f,
                                               1.0f / relativeAspectRatio, 1.0f);
                    } else {
                        // Screen is taller than the camera, scale down Y
                        mProgram.setTargetRect(0.0f, 0.5f - 0.5f * relativeAspectRatio,
                                               1.0f, relativeAspectRatio);
                    }
                    break;
                case RENDERMODE_FILL_CROP:
                    if (relativeAspectRatio > 1) {
                        // Screen is wider than the camera, crop in Y
                        mProgram.setTargetRect(0.0f, 0.5f - 0.5f * relativeAspectRatio,
                                               1.0f, relativeAspectRatio);
                    } else {
                        // Screen is taller than the camera, crop in X
                        mProgram.setTargetRect(0.5f - 0.5f / relativeAspectRatio, 0.0f,
                                               1.0f / relativeAspectRatio, 1.0f);
                    }
                    break;
            };
        }
    }
}
