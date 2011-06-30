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


package android.filterpacks.videosrc;

import android.content.Context;
import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.FrameManager;
import android.filterfw.core.GenerateFieldPort;
import android.filterfw.core.GenerateFinalPort;
import android.filterfw.core.GLFrame;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.filterfw.format.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.ConditionVariable;

import java.io.IOException;
import java.util.List;
import java.util.Set;

import android.util.Log;

/**
 * @hide
 */
public class CameraSource extends Filter {

    /** User-visible parameters */

    /** Camera ID to use for input. Defaults to 0. */
    @GenerateFieldPort(name = "id", hasDefault = true)
    private int mCameraId = 0;

    /** Frame width to request from camera. Actual size may not match requested. */
    @GenerateFieldPort(name = "width", hasDefault = true)
    private int mWidth = 320;

    /** Frame height to request from camera. Actual size may not match requested. */
    @GenerateFieldPort(name = "height", hasDefault = true)
    private int mHeight = 240;

    /** Stream framerate to request from camera. Actual frame rate may not match requested. */
    @GenerateFieldPort(name = "framerate", hasDefault = true)
    private int mFps = 30;

    /** Whether the filter should always wait for a new frame from the camera
     * before providing output.  If set to false, the filter will keep
     * outputting the last frame it received from the camera if multiple process
     * calls are received before the next update from the Camera. Defaults to true.
     */
    @GenerateFinalPort(name = "waitForNewFrame", hasDefault = true)
    private boolean mWaitForNewFrame = true;

    private Camera mCamera;
    private GLFrame mCameraFrame;
    private SurfaceTexture mSurfaceTexture;
    private ShaderProgram mFrameExtractor;
    private MutableFrameFormat mOutputFormat;
    private ConditionVariable mNewFrameAvailable;
    private float[] mCameraTransform;

    private Camera.Parameters mCameraParameters;

    private static final String mFrameShader =
            "#extension GL_OES_EGL_image_external : require\n" +
            "precision mediump float;\n" +
            "uniform mat4 camera_transform;\n" +
            "uniform samplerExternalOES tex_sampler_0;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec2 transformed_texcoord = (camera_transform * vec4(v_texcoord, 0., 1.) ).xy;" +
            "  gl_FragColor = texture2D(tex_sampler_0, transformed_texcoord);\n" +
            "}\n";

    private boolean mLogVerbose;
    private static final String TAG = "CameraSource";

    public CameraSource(String name) {
        super(name);
        mNewFrameAvailable = new ConditionVariable();
        mCameraTransform = new float[16];

        mLogVerbose = Log.isLoggable(TAG, Log.VERBOSE);
    }

    @Override
    public void setupPorts() {
        // Add input port
        addOutputPort("video", ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                  FrameFormat.TARGET_GPU));
    }

    private void createFormats() {
        mOutputFormat = ImageFormat.create(mWidth, mHeight,
                                           ImageFormat.COLORSPACE_RGBA,
                                           FrameFormat.TARGET_GPU);
    }

    @Override
    public void prepare(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Preparing");
        // Compile shader TODO: Move to onGLEnvSomething?
        mFrameExtractor = new ShaderProgram(mFrameShader);
        // SurfaceTexture defines (0,0) to be bottom-left. The filter framework defines
        // (0,0) as top-left, so do the flip here.
        mFrameExtractor.setSourceRect(0, 1, 1, -1);
    }

    @Override
    public void open(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Opening");
        // Open camera
        mCamera = Camera.open(mCameraId);

        // Set parameters
        getCameraParameters();
        mCamera.setParameters(mCameraParameters);

        // Create frame formats
        createFormats();

        // Bind it to our camera frame
        mCameraFrame = (GLFrame)context.getFrameManager().newBoundFrame(mOutputFormat,
                                                                        GLFrame.EXTERNAL_TEXTURE,
                                                                        0);
        mSurfaceTexture = new SurfaceTexture(mCameraFrame.getTextureId());
        try {
            mCamera.setPreviewTexture(mSurfaceTexture);
        } catch (IOException e) {
            throw new RuntimeException("Could not bind camera surface texture: " +
                                       e.getMessage() + "!");
        }

        // Connect SurfaceTexture to callback
        mSurfaceTexture.setOnFrameAvailableListener(onCameraFrameAvailableListener);
        // Start the preview
        mCamera.startPreview();
    }

    @Override
    public void process(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Processing new frame");

        if (mWaitForNewFrame) {
            boolean gotNewFrame;
            gotNewFrame = mNewFrameAvailable.block(1000);
            if (!gotNewFrame) {
                throw new RuntimeException("Timeout waiting for new frame");
            }
            mNewFrameAvailable.close();
        }

        mSurfaceTexture.updateTexImage();

        if (mLogVerbose) Log.v(TAG, "Using frame extractor in thread: " + Thread.currentThread());
        mSurfaceTexture.getTransformMatrix(mCameraTransform);
        mFrameExtractor.setHostValue("camera_transform", mCameraTransform);

        Frame output = context.getFrameManager().newFrame(mOutputFormat);
        mFrameExtractor.process(mCameraFrame, output);

        pushOutput("video", output);

        // Release pushed frame
        output.release();

        if (mLogVerbose) Log.v(TAG, "Done processing new frame");
    }

    @Override
    public void close(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Closing");

        mCamera.release();
        mCamera = null;
        mSurfaceTexture = null;
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mCameraFrame != null) {
            mCameraFrame.release();
        }
    }

    @Override
    public void fieldPortValueUpdated(String name, FilterContext context) {
        if (name.equals("framerate")) {
            getCameraParameters();
            int closestRange[] = findClosestFpsRange(mFps, mCameraParameters);
            mCameraParameters.setPreviewFpsRange(closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
                                                 closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
            mCamera.setParameters(mCameraParameters);
        }
    }

    synchronized public Camera.Parameters getCameraParameters() {
        if (mCameraParameters == null) {
            boolean closeCamera = false;
            if (mCamera == null) {
                mCamera = Camera.open(mCameraId);
                closeCamera = true;
            }
            mCameraParameters = mCamera.getParameters();

            mCameraParameters.setPreviewSize(mWidth, mHeight);
            int closestRange[] = findClosestFpsRange(mFps, mCameraParameters);
            if (mLogVerbose) Log.v(TAG, "Closest frame rate range: ["
                            + closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX] / 1000.
                            + ","
                            + closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX] / 1000.
                            + "]");

            mCameraParameters.setPreviewFpsRange(closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
                                                 closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);

            if (closeCamera) {
                mCamera.release();
                mCamera = null;
            }
        }
        return mCameraParameters;
    }

    /** Update camera parameters. Image resolution cannot be changed. */
    synchronized public void setCameraParameters(Camera.Parameters params) {
        params.setPreviewSize(mWidth, mHeight);
        mCameraParameters = params;
        if (isOpen()) {
            mCamera.setParameters(mCameraParameters);
        }
    }

    private int[] findClosestFpsRange(int fps, Camera.Parameters params) {
        List<int[]> supportedFpsRanges = params.getSupportedPreviewFpsRange();
        int[] closestRange = supportedFpsRanges.get(0);
        for (int[] range : supportedFpsRanges) {
            if (range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX] < fps*1000 &&
                range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX] > fps*1000 &&
                range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX] >
                closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX] &&
                range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX] <
                closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]) {
                closestRange = range;
            }
        }
        return closestRange;
    }

    private SurfaceTexture.OnFrameAvailableListener onCameraFrameAvailableListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            if (mLogVerbose) Log.v(TAG, "New frame from camera");
            mNewFrameAvailable.open();
        }
    };

}
