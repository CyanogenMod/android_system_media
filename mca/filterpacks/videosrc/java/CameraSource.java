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
import android.filterfw.core.FilterParameter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.Frame;
import android.filterfw.core.GLFrame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.FrameManager;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.Program;
import android.filterfw.core.ShaderProgram;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.ConditionVariable;

import java.io.IOException;
import java.util.List;
import java.util.Set;

import android.util.Log;

public class CameraSource extends Filter {

    /** User-visible parameters */

    /** Camera ID to use for input. Defaults to 0. */
    @FilterParameter(name = "id", isOptional = true, isUpdatable = false)
    private int mCameraId = 0;

    /** Frame width to request from camera. Actual size may not match requested. */
    @FilterParameter(name = "width", isOptional = true, isUpdatable = false)
    private int mWidth = 320;

    /** Frame height to request from camera. Actual size may not match requested. */
    @FilterParameter(name = "height", isOptional = true, isUpdatable = false)
    private int mHeight = 240;

    /** Stream framerate to request from camera. Actual frame rate may not match requested. */
    @FilterParameter(name = "framerate", isOptional = true, isUpdatable = true)
    private int mFps = 30;

    /** Whether the filter should always wait for a new frame from the camera
     * before providing output.  If set to false, the filter will keep
     * outputting the last frame it received from the camera if multiple process
     * calls are received before the next update from the Camera. Defaults to true.
     */
    @FilterParameter(name = "waitForNewFrame", isOptional = true, isUpdatable = true)
    private boolean mWaitForNewFrame = true;

    private Camera mCamera;
    private GLFrame mCameraFrame;
    private SurfaceTexture mSurfaceTexture;
    private ShaderProgram mFrameExtractor;
    private MutableFrameFormat mOutputFormat;
    private MutableFrameFormat mCameraFormat;
    private ConditionVariable mNewFrameAvailable;
    private float[] mCameraTransform;

    private final String mFrameShader =
            "#extension GL_OES_EGL_image_external : require\n" +
            "precision mediump float;\n" +
            "uniform mat4 camera_transform;\n" +
            "uniform samplerExternalOES tex_sampler_0;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec2 transformed_texcoord = (camera_transform * vec4(v_texcoord, 0., 1.) ).xy;" +
            "  gl_FragColor = texture2D(tex_sampler_0, transformed_texcoord);\n" +
            "}\n";

    private static final boolean LOGV = false;
    private static final boolean LOGVV = false;

    private static final String TAG = "CameraSource";

    public CameraSource(String name) {
        super(name);
        mNewFrameAvailable = new ConditionVariable();
        mCameraTransform = new float[16];
    }

    @Override
    public String[] getInputNames() {
        return null;
    }

    @Override
    public String[] getOutputNames() {
        return new String[] { "video" };
    }

    @Override
    public boolean acceptsInputFormat(int index, FrameFormat format) {
        return false;
    }

    @Override
    public FrameFormat getOutputFormat(int index) {
        mOutputFormat = new MutableFrameFormat(FrameFormat.TYPE_BYTE, FrameFormat.TARGET_GPU);
        mOutputFormat.setDimensions(mWidth, mHeight);
        mOutputFormat.setBytesPerSample(4);

        mCameraFormat = mOutputFormat.mutableCopy();
        mCameraFormat.setMetaValue(GLFrame.USE_EXTERNAL_TEXTURE, true);

        return mOutputFormat;
    }

    @Override
    public void prepare(FilterContext context) {
        if (LOGV) Log.v(TAG, "Preparing");

        // Create external camera frame
        mCameraFrame = (GLFrame)context.getFrameManager().newFrame(mCameraFormat);

        // Compile shader TODO: Move to onGLEnvSomething?
        mFrameExtractor = new ShaderProgram(mFrameShader);
        // SurfaceTexture defines (0,0) to be bottom-left. The filter framework defines
        // (0,0) as top-left, so do the flip here.
        mFrameExtractor.setSourceRect(0, 1, 1, -1);
    }

    @Override
    public int open(FilterContext context) {
        if (LOGV) Log.v(TAG, "Opening");
        // Open camera
        mCamera = Camera.open(mCameraId);

        // Set parameters
        Camera.Parameters params = mCamera.getParameters();
        params.setPreviewSize(mWidth, mHeight);

        int closestRange[] = findClosestFpsRange(mFps, params);
        if (LOGV) Log.v(TAG, "Closest frame rate range: ["
                        + closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX] / 1000.
                        + ","
                        + closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX] / 1000.
                        + "]");

        params.setPreviewFpsRange(closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
                                  closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
        mCamera.setParameters(params);

        // Bind it to our camera frame
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

        return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    @Override
    public int process(FilterContext context) {
        if (LOGVV) Log.v(TAG, "Processing new frame");

        if (mWaitForNewFrame) {
            boolean gotNewFrame;
            gotNewFrame = mNewFrameAvailable.block(1000);
            if (!gotNewFrame) {
                throw new RuntimeException("Timeout waiting for new frame");
            }
            mNewFrameAvailable.close();
        }

        mSurfaceTexture.updateTexImage();

        mSurfaceTexture.getTransformMatrix(mCameraTransform);
        mFrameExtractor.setHostValue("camera_transform", mCameraTransform);

        Frame output = context.getFrameManager().newFrame(mOutputFormat);
        mFrameExtractor.process(mCameraFrame, output);

        putOutput(0, output);

        // Release pushed frame
        output.release();

        if (LOGVV) Log.v(TAG, "Done processing new frame");
        return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    @Override
    public void close(FilterContext context) {
        if (LOGV) Log.v(TAG, "Closing");

        mCamera.stopPreview();
        mCamera.release();
        mSurfaceTexture = null;
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mCameraFrame != null) {
            mCameraFrame.release();
        }
    }

    @Override
    public void parametersUpdated(Set<String> updated) {
        if (updated.contains("framerate")) {
            Camera.Parameters params = mCamera.getParameters();
            int closestRange[] = findClosestFpsRange(mFps, params);
            params.setPreviewFpsRange(closestRange[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
                                      closestRange[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
            mCamera.setParameters(params);
        }
    }

    public Camera.Parameters getCameraParameters() {
        return mCamera.getParameters();
    }

    /** Update camera parameters. Image resolution cannot be changed. */
    public void setCameraParameters(Camera.Parameters params) {
        params.setPreviewSize(mWidth, mHeight);
        mCamera.setParameters(params);
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
            if (LOGV) Log.v(TAG, "New frame from camera");
            mNewFrameAvailable.open();
        }
    };

}
