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
import android.content.res.AssetFileDescriptor;
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
import android.media.MediaPlayer;
import android.os.ConditionVariable;

import java.io.IOException;
import java.io.FileDescriptor;
import java.lang.IllegalArgumentException;
import java.util.List;
import java.util.Set;


import android.util.Log;

public class MediaSource extends Filter {

    /** User-visible parameters */

    /** The source URL for the media source. Can be an http: link to a remote
     * resource, or a file: link to a local media file */
    @FilterParameter(name = "sourceUrl", isOptional = true, isUpdatable = true)
    private String mSourceUrl = "";

    /** An open asset file descriptor to a local media source. If set,
     * overrides the sourceUrl field. Set to null to use the sourceUrl field
     * instead. */
    @FilterParameter(name = "sourceAsset", isOptional = true, isUpdatable = true)
    private AssetFileDescriptor mSourceAsset;

    /** Whether the filter will always wait for a new video frame, or whether it
     * will output an old frame again if a new frame isn't available. Defaults to true.
     */
    @FilterParameter(name = "waitForNewFrame", isOptional = true, isUpdatable = true)
    private boolean mWaitForNewFrame = true;

    /** Whether the media source should loop automatically or not. Defaults to true. */
    @FilterParameter(name = "loop", isOptional = true, isUpdatable = true)
    private boolean mLooping = true;

    private MediaPlayer mMediaPlayer;
    private GLFrame mMediaFrame;
    private SurfaceTexture mSurfaceTexture;
    private ShaderProgram mFrameExtractor;
    private MutableFrameFormat mOutputFormat;
    private MutableFrameFormat mMediaFormat;
    private ConditionVariable mNewFrameAvailable;
    private float[] mFrameTransform;

    private final String mFrameShader =
            "#extension GL_OES_EGL_image_external : require\n" +
            "precision mediump float;\n" +
            "uniform mat4 frame_transform;\n" +
            "uniform samplerExternalOES tex_sampler_0;\n" +
            "varying vec2 v_texcoord;\n" +
            "void main() {\n" +
            "  vec2 transformed_texcoord = (frame_transform * vec4(v_texcoord, 0., 1.) ).xy;" +
            "  gl_FragColor = texture2D(tex_sampler_0, transformed_texcoord);\n" +
            "}\n";

    private boolean mGotSize;
    private boolean mPrepared;
    private boolean mPlaying;

    private static final boolean LOGV = true;
    private static final boolean LOGVV = false;
    private static final String TAG = "MediaSource";

    public MediaSource(String name) {
        super(name);
        mNewFrameAvailable = new ConditionVariable();
        mFrameTransform = new float[16];
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
        // Format matches that of a GLFrame, but don't want to access an OpenGL
        // context yet
        mOutputFormat = new MutableFrameFormat(FrameFormat.TYPE_BYTE,
                                               FrameFormat.TARGET_GPU);
        mOutputFormat.setBytesPerSample(4);

        mMediaFormat = mOutputFormat.mutableCopy();
        mMediaFormat.setMetaValue(GLFrame.USE_EXTERNAL_TEXTURE, true);

        // Don't know dimensions until we open the media file
        mOutputFormat.setDimensions(0, 0);
        return mOutputFormat;
    }

    @Override
    protected void prepare(FilterContext context) {
        if (LOGV) Log.v(TAG, "Preparing MediaSource");

        mMediaFrame = (GLFrame)context.getFrameManager().newFrame(mMediaFormat);

        mFrameExtractor = new ShaderProgram(mFrameShader);
        // SurfaceTexture defines (0,0) to be bottom-left. The filter framework
        // defines (0,0) as top-left, so do the flip here.
        mFrameExtractor.setSourceRect(0, 1, 1, -1);

        mSurfaceTexture = new SurfaceTexture(mMediaFrame.getTextureId());
    }

    @Override
    public int open(FilterContext context) {
        if (LOGV) Log.v(TAG, "Opening MediaSource");
        return setupMediaPlayer();
    }

    @Override
    public int process(FilterContext context) {
        if (LOGVV) Log.v(TAG, "Processing new frame");

        if (mMediaPlayer == null) {
            // Something went wrong in initialization or parameter updates
            return Filter.STATUS_ERROR;
        }

        if (!mPlaying) {
            synchronized(this) {
                int waitCount = 0;
                while (!mGotSize || !mPrepared) {
                    try {
                        this.wait(100);
                    } catch (InterruptedException e) {
                        // ignoring
                    }
                    waitCount++;
                    if (waitCount == 50) {
                        Log.e(TAG, "MediaPlayer timed out while preparing");
                        mMediaPlayer.release();
                        return Filter.STATUS_ERROR;
                    }
                }
            }
            mMediaPlayer.start();
            mPlaying = true;
        }

        if (mWaitForNewFrame) {
            boolean gotNewFrame;
            gotNewFrame = mNewFrameAvailable.block(1000);
            if (!gotNewFrame) {
                Log.e(TAG, "Timeout waiting for new frame");
                return Filter.STATUS_ERROR;
            }
            mNewFrameAvailable.close();
        }

        mSurfaceTexture.updateTexImage();

        mSurfaceTexture.getTransformMatrix(mFrameTransform);
        mFrameExtractor.setHostValue("frame_transform", mFrameTransform);

        Frame output = context.getFrameManager().newFrame(mOutputFormat);
        mFrameExtractor.process(mMediaFrame, output);

        putOutput(0, output);
        output.release();

        return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    @Override
    public void close(FilterContext context) {
        if (mMediaPlayer.isPlaying()) {
            mMediaPlayer.stop();
        }
        mMediaPlayer.release();
        mMediaPlayer = null;
        if (LOGV) Log.v(TAG, "MediaSource closed");
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mMediaFrame != null) {
            mMediaFrame.release();
        }
    }

    @Override
    public void parametersUpdated(Set<String> updated) {
        if (updated.contains("sourceUrl") && mSourceAsset == null) {
            if (isOpen()) {
                if (LOGV) Log.v(TAG, "Opening new source URL");
                setupMediaPlayer();
            }
        } else if (updated.contains("sourceAsset") ) {
            if (isOpen()) {
                if (LOGV) {
                    if (mSourceAsset == null) {
                        if (LOGV) Log.v(TAG, "Opening new source URL");
                    } else {
                        Log.v(TAG, "Opening new source FD");
                    }
                }
                setupMediaPlayer();
            }
        } else if (updated.contains("loop")) {
            if (isOpen()) {
                mMediaPlayer.setLooping(mLooping);
            }
        }
    }

    /** Creates a media player, sets it up, and calls prepare */
    synchronized private int setupMediaPlayer() {
        mPrepared = false;
        mGotSize = false;
        mPlaying = false;

        if (mMediaPlayer != null) {
            // Clean up existing media player
            mMediaPlayer.reset();
        } else {
            // Create new media player
            mMediaPlayer = new MediaPlayer();
        }

        if (mMediaPlayer == null) {
            Log.e(TAG, "Unable to create a media player!");
            return Filter.STATUS_ERROR;
        }

        // Set up data sources, etc
        try {
            if (mSourceAsset == null) {
                mMediaPlayer.setDataSource(mSourceUrl);
            } else {
                mMediaPlayer.setDataSource(mSourceAsset.getFileDescriptor(), mSourceAsset.getStartOffset(), mSourceAsset.getLength());
            }
        } catch(IOException e) {
            if (mSourceAsset == null) {
                Log.e(TAG, "Unable to set media player source to " + mSourceUrl + ". Exception: " + e);
            } else {
                Log.e(TAG, "Unable to set media player source to " + mSourceAsset + ". Exception: " + e);
            }
            mMediaPlayer.release();
            mMediaPlayer = null;
            return Filter.STATUS_ERROR;
        } catch(IllegalArgumentException e) {
            if (mSourceAsset == null) {
                Log.e(TAG, "Unable to set media player source to " + mSourceUrl + ". Exception: " + e);
            } else {
                Log.e(TAG, "Unable to set media player source to " + mSourceAsset + ". Exception: " + e);
            }
            mMediaPlayer.release();
            mMediaPlayer = null;
            return Filter.STATUS_ERROR;
        }

        mMediaPlayer.setLooping(mLooping);

        // Bind it to our media frame
        mMediaPlayer.setTexture(mSurfaceTexture);

        // Connect Media Player to callbacks

        mMediaPlayer.setOnVideoSizeChangedListener(onVideoSizeChangedListener);
        mMediaPlayer.setOnPreparedListener(onPreparedListener);
        mMediaPlayer.setOnCompletionListener(onCompletionListener);

        // Connect SurfaceTexture to callback
        mSurfaceTexture.setOnFrameAvailableListener(onMediaFrameAvailableListener);

        mMediaPlayer.prepareAsync();

        if (LOGV) Log.v(TAG, "MediaPlayer now preparing.");
        return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    private MediaPlayer.OnVideoSizeChangedListener onVideoSizeChangedListener =
            new MediaPlayer.OnVideoSizeChangedListener() {
        public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
            if (LOGV) Log.v(TAG, "MediaPlayer sent dimensions: " + width + " x " + height);
            if (!mGotSize) {
                mOutputFormat.setDimensions(width, height);
            } else {
                if (mOutputFormat.getWidth() != width ||
                    mOutputFormat.getHeight() != height) {
                    Log.e(TAG, "Multiple video size change events received!");
                }
            }
            synchronized(this) {
                mGotSize = true;
                this.notify();
            }
        }
    };

    private MediaPlayer.OnPreparedListener onPreparedListener =
            new MediaPlayer.OnPreparedListener() {
        public void onPrepared(MediaPlayer mp) {
            if (LOGV) Log.v(TAG, "MediaPlayer is prepared");
            synchronized(this) {
                mPrepared = true;
                this.notify();
            }
        }
    };

    private MediaPlayer.OnCompletionListener onCompletionListener =
            new MediaPlayer.OnCompletionListener() {
        public void onCompletion(MediaPlayer mp) {
            if (LOGV) Log.v(TAG, "MediaPlayer has completed playback");
        }
    };

    private SurfaceTexture.OnFrameAvailableListener onMediaFrameAvailableListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            if (LOGVV) Log.v(TAG, "New frame from media player");
            mNewFrameAvailable.open();
        }
    };

}
