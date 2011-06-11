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

import android.content.Context;
import android.filterfw.core.FilterSurfaceRenderer;
import android.filterfw.core.GLEnvironment;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class FilterSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

    private SurfaceHolder mHolder;
    private FilterSurfaceRenderer mRenderer;
    private GLEnvironment mGLEnv;
    private int mFormat = -1;
    private int mWidth  = -1;
    private int mHeight = -1;
    private int mSurfaceId = -1;

    public FilterSurfaceView(Context context) {
        super(context);
        getHolder().addCallback(this);
    }

    public FilterSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        getHolder().addCallback(this);
    }

    public synchronized void bindToRenderer(FilterSurfaceRenderer renderer, GLEnvironment glEnv) {
        // Make sure we are not bound already
        if (mRenderer != null && mRenderer != renderer) {
            throw new RuntimeException(
                "Attempting to bind filter " + renderer + " to SurfaceView with another filter " +
                mRenderer + " attached already!");
        }
        mRenderer = renderer;

        // Set GLEnv
        if (mGLEnv != null && mGLEnv != glEnv) {
            throw new RuntimeException("FilterSurfaceView is already registered with a GLEnvironment!");
        }
        mGLEnv = glEnv;

        // Check if surface has been created already
        if (mHolder != null) {
            // Register with env (double registration will be ignored by GLEnv, so we can simply
            // try to do it here).
            mSurfaceId = mGLEnv.registerSurface(mHolder.getSurface());

            if (mSurfaceId < 0) {
                throw new RuntimeException("Could not register Surface in FilterSurfaceView!");
            }

            // Forward surface changed to listener
            if (mFormat != -1) {
                mRenderer.surfaceChanged(mFormat, mWidth, mHeight);
            }
        }


    }

    public synchronized void unbind() {
        mRenderer = null;
    }

    public synchronized int getSurfaceId() {
        return mSurfaceId;
    }

    public synchronized GLEnvironment getGLEnv() {
        return mGLEnv;
    }

    @Override
    public synchronized void surfaceCreated(SurfaceHolder holder) {
        mHolder = holder;
        mFormat = -1;

        // Register with GLEnvironment if we have it already
        if (mGLEnv != null) {
            mSurfaceId = mGLEnv.registerSurface(mHolder.getSurface());

            if (mSurfaceId < 0) {
                throw new RuntimeException("Could not register Surface: " + mHolder.getSurface() +
                                           " in FilterSurfaceView!");
            }
        }
    }

    @Override
    public synchronized void surfaceChanged(SurfaceHolder holder,
                                            int format,
                                            int width,
                                            int height) {
        // Remember these values
        mFormat = format;
        mWidth = width;
        mHeight = height;

        // Forward to renderer
        if (mRenderer != null) {
            mRenderer.surfaceChanged(mFormat, mWidth, mHeight);
        }
    }

    @Override
    public synchronized void surfaceDestroyed(SurfaceHolder holder) {
        // Forward to renderer
        if (mRenderer != null) {
            mRenderer.surfaceDestroyed();
        }

        // Get rid of internal objects associated with this surface
        unregisterSurface();
        mHolder = null;
        mFormat = -1;
    }

    private void unregisterSurface() {
        if (mGLEnv != null && mSurfaceId > 0) {
            mGLEnv.unregisterSurfaceId(mSurfaceId);
        }
    }

}
