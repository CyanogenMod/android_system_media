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

/** <p>A filter that converts textures from a SurfaceTexture object into frames for
 * processing in the filter framework.</p>
 *
 * <p>To use, connect up the sourceListener callback, and then when executing
 * the graph, use the SurfaceTexture object passed to the callback to feed
 * frames into the filter graph. For example, pass the SurfaceTexture into
 * {#link
 * android.hardware.Camera.setPreviewTexture(android.graphics.SurfaceTexture)}.
 * This filter is intended for applications that need for flexibility than the
 * CameraSource and MediaSource provide. Note that the application needs to
 * provide width and height information for the SurfaceTextureSource, which it
 * should obtain from wherever the SurfaceTexture data is coming from to avoid
 * unnecessary resampling.</p>
 */
public class SurfaceTextureSource extends Filter {

    /** User-visible parameters */

    /** The callback interface for the sourceListener parameter */
    public interface SurfaceTextureSourceListener {
        public void onSurfaceTextureSourceReady(SurfaceTexture source);
    }
    /** A callback to send the internal SurfaceTexture object to, once it is
     * created. This callback will be called when the the filter graph is
     * preparing to execute, but before any processing has actually taken
     * place. The SurfaceTexture object passed to this callback is the only way
     * to feed this filter. When the filter graph is shutting down, this
     * callback will be called again with null as the source.
     *
     * This callback may be called from an arbitrary thread, so it should not
     * assume it is running in the UI thread in particular.
     */
    @FilterParameter(name = "sourceListener", isOptional = false, isUpdatable = false)
    private SurfaceTextureSourceListener mSourceListener;

    /** The width of the output image frame. If the texture width for the
     * SurfaceTexture source is known, use it here to minimize resampling. */
    @FilterParameter(name = "width", isOptional = false, isUpdatable = true)
    private int mWidth;

    /** The height of the output image frame. If the texture height for the
     * SurfaceTexture source is known, use it here to minimize resampling. */
    @FilterParameter(name = "height", isOptional = false, isUpdatable = true)
    private int mHeight;

    /** Whether the filter will always wait for a new frame from its
     * SurfaceTexture, or whether it will output an old frame again if a new
     * frame isn't available. The filter will always wait for the first frame,
     * to avoid outputting a blank frame. Defaults to true.
     */
    @FilterParameter(name = "waitForNewFrame", isOptional = true, isUpdatable = true)
    private boolean mWaitForNewFrame = true;

    /** Maximum timeout before signaling error when waiting for a new frame. Set
     * this to zero to disable the timeout and wait indefinitely. In milliseconds.
     */
    @FilterParameter(name = "waitTimeout", isOptional = true, isUpdatable = true)
    private int mWaitTimeout = 1000;

    private GLFrame mMediaFrame;
    private SurfaceTexture mSurfaceTexture;
    private ShaderProgram mFrameExtractor;
    private MutableFrameFormat mOutputFormat;
    private MutableFrameFormat mMediaFormat;
    private ConditionVariable mNewFrameAvailable;
    private float[] mFrameTransform;
    private boolean mFirstFrame;

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

    private static final boolean LOGV = true;
    private static final boolean LOGVV = false;
    private static final String TAG = "SurfaceTextureSource";

    public SurfaceTextureSource(String name) {
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
        // Use user-specified dimensions, as SurfaceTexture does not provide a way to get
        // its underlying size.
        mMediaFormat = mOutputFormat.mutableCopy();
        mMediaFormat.setDimensionCount(2);
        mMediaFormat.setMetaValue(GLFrame.USE_EXTERNAL_TEXTURE, true);

        mOutputFormat.setDimensions(mWidth, mHeight);
        return mOutputFormat;
    }

    @Override
    protected void prepare(FilterContext context) {
        if (LOGV) Log.v(TAG, "Preparing SurfaceTextureSource");

        mMediaFrame = (GLFrame)context.getFrameManager().newFrame(mMediaFormat);

        mFrameExtractor = new ShaderProgram(mFrameShader);
        // SurfaceTexture defines (0,0) to be bottom-left. The filter framework
        // defines (0,0) as top-left, so do the flip here.
        mFrameExtractor.setSourceRect(0, 1, 1, -1);

        mSurfaceTexture = new SurfaceTexture(mMediaFrame.getTextureId());
        mSourceListener.onSurfaceTextureSourceReady(mSurfaceTexture);
    }

    @Override
    public int open(FilterContext context) {
        if (LOGV) Log.v(TAG, "Opening SurfaceTextureSource");
        // Connect SurfaceTexture to callback
        mSurfaceTexture.setOnFrameAvailableListener(onFrameAvailableListener);
        mFirstFrame = true;

        return Filter.STATUS_WAIT_FOR_FREE_OUTPUTS;
    }

    @Override
    public int process(FilterContext context) {
        if (LOGVV) Log.v(TAG, "Processing new frame");

        if (mWaitForNewFrame || mFirstFrame) {
            boolean gotNewFrame;
            if (mWaitTimeout != 0) {
                gotNewFrame = mNewFrameAvailable.block(mWaitTimeout);
                if (!gotNewFrame) {
                    Log.e(TAG, "Timeout waiting for new frame");
                    return Filter.STATUS_ERROR;
                }
            } else {
                mNewFrameAvailable.block();
            }
            mNewFrameAvailable.close();
            mFirstFrame = false;
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
        if (LOGV) Log.v(TAG, "MediaSource closed");
        mSurfaceTexture.setOnFrameAvailableListener(null);
    }

    @Override
    public void tearDown(FilterContext context) {
        if (mMediaFrame != null) {
            mMediaFrame.release();
        }
        mSurfaceTexture = null;
        mSourceListener.onSurfaceTextureSourceReady(mSurfaceTexture);
    }

    @Override
    public void parametersUpdated(Set<String> updated) {
        if (updated.contains("width") || updated.contains("height") ) {
            mOutputFormat.setDimensions(mWidth, mHeight);
        }
    }

    private SurfaceTexture.OnFrameAvailableListener onFrameAvailableListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            if (LOGVV) Log.v(TAG, "New frame from SurfaceTextureSource");
            mNewFrameAvailable.open();
        }
    };

}
