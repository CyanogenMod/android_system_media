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
import android.media.CamcorderProfile;
import android.filterfw.core.GLEnvironment;

import java.io.IOException;
import java.util.List;
import java.util.Set;

import android.util.Log;

/** @hide */
public class MediaEncoderFilter extends Filter {

    /** User-visible parameters */

    /** Recording state. When set to false, recording will stop, or will not
     * start if not yet running the graph. Instead, frames are simply ignored.
     * When switched back to true, recording will restart. This allows a single
     * graph to both provide preview and to record video. If this is false,
     * recording settings can be updated while the graph is running.
     */
    @GenerateFieldPort(name = "recording", hasDefault = true)
    private boolean mRecording = true;

    /** Filename to save the output. */
    @GenerateFieldPort(name = "outputFile", hasDefault = true)
    private String mOutputFile = new String("/sdcard/MediaEncoderOut.mp4");

    /** Input audio source. If not set, no audio will be recorded.
     * Select from the values in MediaRecorder.AudioSource
     */
    @GenerateFieldPort(name = "audioSource", hasDefault = true)
    private int mAudioSource = NO_AUDIO_SOURCE;

    /** Media recorder info listener, which needs to implement
     * MediaRecorder.onInfoListener. Set this to receive notifications about
     * recording events.
     */
    @GenerateFieldPort(name = "infoListener", hasDefault = true)
    private MediaRecorder.OnInfoListener mInfoListener = null;

    /** Media recorder error listener, which needs to implement
     * MediaRecorder.onErrorListener. Set this to receive notifications about
     * recording errors.
     */
    @GenerateFieldPort(name = "errorListener", hasDefault = true)
    private MediaRecorder.OnErrorListener mErrorListener = null;

    /** Orientation hint. Used for indicating proper video playback orientation.
     * Units are in degrees of clockwise rotation, valid values are (0, 90, 180,
     * 270).
     */
    @GenerateFieldPort(name = "orientationHint", hasDefault = true)
    private int mOrientationHint = 0;

    /** Camcorder profile to use. Select from the profiles available in
     * android.media.CamcorderProfile. If this field is set, it overrides
     * settings to width, height, framerate, outputFormat, and videoEncoder.
     */
    @GenerateFieldPort(name = "recordingProfile", hasDefault = true)
    private CamcorderProfile mProfile = null;

    /** Frame width to be encoded, defaults to 320.
     * Actual received frame size has to match this */
    @GenerateFieldPort(name = "width", hasDefault = true)
    private int mWidth = 320;

    /** Frame height to to be encoded, defaults to 240.
     * Actual received frame size has to match */
    @GenerateFieldPort(name = "height", hasDefault = true)
    private int mHeight = 240;

    /** Stream framerate to encode the frames at.
     * By default, frames are encoded at 30 FPS*/
    @GenerateFieldPort(name = "framerate", hasDefault = true)
    private int mFps = 30;

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

    private static final int NO_AUDIO_SOURCE = -1;

    private int mSurfaceId;
    private ShaderProgram mProgram;
    private GLFrame mScreen;

    private boolean mRecordingActive = false;

    private boolean mLogVerbose;
    private static final String TAG = "MediaEncoderFilter";

    // Our hook to the encoder
    private MediaRecorder mMediaRecorder;

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
        if (mLogVerbose) Log.v(TAG, "Port " + name + " has been updated");
        if (name.equals("recording")) return;

        if (isOpen() && mRecordingActive) {
            throw new RuntimeException("Cannot change recording parameters"
                                       + " when the filter is recording!");
        }
    }

    // update the MediaRecorderParams based on the variables.
    // These have to be in certain order as per the MediaRecorder
    // documentation
    private void updateMediaRecorderParams() {
        final int GRALLOC_BUFFER = 2;
        mMediaRecorder.setVideoSource(GRALLOC_BUFFER);
        if (mAudioSource != NO_AUDIO_SOURCE) {
            mMediaRecorder.setAudioSource(mAudioSource);
        }
        if (mProfile != null) {
            mMediaRecorder.setProfile(mProfile);
        } else {
            mMediaRecorder.setOutputFormat(mOutputFormat);
            mMediaRecorder.setVideoEncoder(mVideoEncoder);
            mMediaRecorder.setVideoSize(mWidth, mHeight);
            mMediaRecorder.setVideoFrameRate(mFps);
        }
        mMediaRecorder.setOrientationHint(mOrientationHint);
        mMediaRecorder.setOnInfoListener(mInfoListener);
        mMediaRecorder.setOnErrorListener(mErrorListener);
        mMediaRecorder.setOutputFile(mOutputFile);
    }

    @Override
    public void prepare(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Preparing");

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

        mRecordingActive = false;
    }

    @Override
    public void open(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Opening");
        if (mRecording) startRecording(context);
    }

    private void startRecording(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Starting recording");

        mMediaRecorder = new MediaRecorder();
        updateMediaRecorderParams();

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

        mRecordingActive = true;
    }

    @Override
    public void process(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Starting frame processing");

        GLEnvironment glEnv = context.getGLEnvironment();
        // Get input frame
        Frame input = pullInput("videoframe");

        // Check if recording needs to start
        if (!mRecordingActive && mRecording) {
            startRecording(context);
        }
        // Check if recording needs to stop
        if (mRecordingActive && !mRecording) {
            stopRecording(context);
        }

        if (!mRecordingActive) return;

        // Activate our surface
        glEnv.activateSurfaceWithId(mSurfaceId);

        // Process
        mProgram.process(input, mScreen);

        // Set timestamp from input
        glEnv.setSurfaceTimestamp(input.getTimestamp());
        // And swap buffers
        glEnv.swapBuffers();
    }

    private void stopRecording(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Stopping recording");

        GLEnvironment glEnv = context.getGLEnvironment();
        glEnv.disconnectSurfaceMediaSource(mMediaRecorder);
        mMediaRecorder.stop();
        glEnv.unregisterSurfaceId(mSurfaceId);
        mMediaRecorder.release();
        mMediaRecorder = null;

        mRecordingActive = false;
    }

    @Override
    public void close(FilterContext context) {
        if (mLogVerbose) Log.v(TAG, "Closing");
        if (mRecordingActive) stopRecording(context);
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
