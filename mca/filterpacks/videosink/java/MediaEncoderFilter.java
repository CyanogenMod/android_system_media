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


package android.filterpacks.videosink;

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
import android.os.ConditionVariable;
import android.media.MediaRecorder;
import android.filterfw.core.GLEnvironment;

import java.io.IOException;
import java.util.List;
import java.util.Set;

import android.util.Log;

/** @hide */
public class MediaEncoderFilter extends Filter {

    /** User-visible parameters */

    /** Frame width to be encoded.
     * Actual received frame size has to match this */
    @GenerateFieldPort(name = "width", hasDefault = false)
    private int mWidth;

    /** Frame height to to be encoded.
     * Actual received frame size has to match */
    @GenerateFieldPort(name = "height", hasDefault = false)
    private int mHeight;

    /** Stream framerate to encode the frames at.
     * By default, frames are encoded at 30 FPS*/
    @GenerateFieldPort(name = "framerate", hasDefault = true)
    private int mFps = 30;


    /** Filename to save the output. */
    @GenerateFieldPort(name = "outputFile", hasDefault = true)
    private String mOutputFile = new String("/sdcard/MediaEncoderOut.mp4");

    /** The output format to encode the frames in.
     * Choose an output format from the options in
     * android.media.MediaRecorder.OutputFormat */
    @GenerateFieldPort(name = "outputFormat", hasDefault = true)
    private int mOutputFormat = MediaRecorder.OutputFormat.MPEG_4;

    /** The videoencoder to encode the frames with.
     * Choose a videoencoder from the options in
     * android.media.MediaRecorder.VideoEncoder */
    @GenerateFieldPort(name = "videoEncoder", hasDefault = true)
    private int mVideoEncoder = MediaRecorder.VideoEncoder.H264;

    // End of user visible parameters

    private int mSurfaceId;
    private ShaderProgram mProgram;
    private GLFrame mScreen;

    private boolean mLogVerbose;
    private static final String TAG = "MediaEncoderFilter";

    // Our hook to the encoder
    private MediaRecorder mMediaRecorder = new MediaRecorder();

    public MediaEncoderFilter(String name) {
        super(name);
        mLogVerbose = Log.isLoggable(TAG, Log.VERBOSE);
    }

    @Override
    public void setupPorts() {
        // Add input port- will accept RGBA GLFrames
        addMaskedInputPort("videoframe", ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                      FrameFormat.TARGET_GPU));
    }

    @Override
    public void fieldPortValueUpdated(String name, FilterContext context) {
        if (isOpen()) {
            throw new RuntimeException("Cannot change filter parameter"
                                    + "values when the filter is open!");
        }
        updateMediaRecorderParams();
    }

    // update the MediaRecorderParams based on the variables.
    // These have to be in certain order as per the MediaRecorder
    // documentation
    private void updateMediaRecorderParams() {
        final int GRALLOC_BUFFER = 2;
        mMediaRecorder.setVideoSource(GRALLOC_BUFFER);
        mMediaRecorder.setOutputFormat(mOutputFormat);
        mMediaRecorder.setVideoEncoder(mVideoEncoder);
        mMediaRecorder.setOutputFile(mOutputFile);
        mMediaRecorder.setVideoSize(mWidth, mHeight);
        mMediaRecorder.setVideoFrameRate(mFps);
    }

    @Override
    public void prepare(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Preparing");
        // Setup the MediaRecorder correctly
        updateMediaRecorderParams();

        // Create identity shader to render, and make sure to render upside-down, as textures
        // are stored internally bottom-to-top.
        mProgram = ShaderProgram.createIdentity(context);
        mProgram.setSourceRect(0, 1, 1, -1);

        // Create a frame representing the screen
        MutableFrameFormat screenFormat = new MutableFrameFormat(
                              FrameFormat.TYPE_BYTE, FrameFormat.TARGET_GPU);
        screenFormat.setBytesPerSample(4);
        screenFormat.setDimensions(mWidth, mHeight);
        mScreen = (GLFrame)context.getFrameManager().newBoundFrame(
                           screenFormat, GLFrame.EXISTING_FBO_BINDING, 0);
    }

    @Override
    public void open(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Opening");
        try {
            mMediaRecorder.prepare();
        } catch (IllegalStateException e) {
            throw e;
        } catch (IOException e) {
            throw new RuntimeException("IOException in"
                    + "MediaRecorder.prepare()!", e);
        } catch (Exception e) {
            throw new RuntimeException("Unknown Exception in"
                    + "MediaRecorder.prepare()!", e);
        }
        // Make sure start() is called before trying to
        // register the surface. The native window handle needed to create
        // the surface is initiated in start()
        mMediaRecorder.start();
        Log.v(TAG, "ME Filter: Open: registering surface from Mediarecorder");
        mSurfaceId = context.getGLEnvironment().
                 registerSurfaceFromMediaRecorder(mMediaRecorder);
    }

    @Override
    public void process(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Starting frame processing");

        GLEnvironment glEnv = context.getGLEnvironment();
        // Get input frame
        Frame input = pullInput("videoframe");

        // Activate our surface
        glEnv.activateSurfaceWithId(mSurfaceId);

        // Process
        mProgram.process(input, mScreen);

        // And swap buffers
        glEnv.swapBuffers();
    }

    @Override
    public void close(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Closing");
        GLEnvironment glEnv = context.getGLEnvironment();
        glEnv.disconnectSurfaceMediaSource(mMediaRecorder);
        mMediaRecorder.stop();
        context.getGLEnvironment().unregisterSurfaceId(mSurfaceId);
    }

    @Override
    public void tearDown(FilterContext context) {
        // Release all the resources associated with the MediaRecorder
        // and GLFrame members
        if (mMediaRecorder != null) {
            mMediaRecorder.release();
        }
        if (mScreen != null) {
            mScreen.release();
        }
    }

}
