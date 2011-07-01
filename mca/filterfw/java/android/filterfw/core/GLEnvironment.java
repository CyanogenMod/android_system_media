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

import android.filterfw.core.NativeAllocatorTag;
import android.graphics.SurfaceTexture;
import android.view.Surface;

/**
 * @hide
 */
public class GLEnvironment {

    private int glEnvId;

    public GLEnvironment() {
        nativeAllocate();
    }

    private GLEnvironment(NativeAllocatorTag tag) {
    }

    @Override
    protected void finalize() throws Throwable {
        nativeDeallocate();
    }

    public static GLEnvironment activeEnvironment() {
        return nativeActiveEnvironment();
    }

    public void initWithNewContext() {
        if (!nativeInitWithNewContext()) {
            throw new RuntimeException("Could not initialize GLEnvironment with new context!");
        }
    }

    public void initWithCurrentContext() {
        if (!nativeInitWithCurrentContext()) {
            throw new RuntimeException("Could not initialize GLEnvironment with current context!");
        }
    }

    public boolean isActive() {
        return nativeIsActive();
    }

    public void activate() {
        if (!nativeActivate()) {
            throw new RuntimeException("Could not activate GLEnvironment!");
        }
    }

    public void deactivate() {
        if (!nativeDeactivate()) {
            throw new RuntimeException("Could not deactivate GLEnvironment!");
        }
    }

    public void swapBuffers() {
        if (!nativeSwapBuffers()) {
            throw new RuntimeException("Error swapping EGL buffers!");
        }
    }

    public int registerSurface(Surface surface) {
        int result = nativeAddSurface(surface);
        if (result < 0) {
            throw new RuntimeException("Error registering surface " + surface + "!");
        }
        return result;
    }

    public int registerSurfaceTexture(SurfaceTexture surfaceTexture, int width, int height) {
        int result = nativeAddSurfaceTexture(surfaceTexture, width, height);
        if (result < 0) {
            throw new RuntimeException("Error registering surfaceTexture " + surfaceTexture + "!");
        }
        return result;
    }

    public void activateSurfaceWithId(int surfaceId) {
        if (!nativeActivateSurfaceId(surfaceId)) {
            throw new RuntimeException("Could not activate surface " + surfaceId + "!");
        }
    }

    public void unregisterSurfaceId(int surfaceId) {
        if (!nativeRemoveSurfaceId(surfaceId)) {
            throw new RuntimeException("Could not unregister surface " + surfaceId + "!");
        }
    }

    static {
        System.loadLibrary("filterfw");
    }

    private native static GLEnvironment nativeActiveEnvironment();

    private native boolean nativeInitWithNewContext();

    private native boolean nativeInitWithCurrentContext();

    private native boolean nativeIsActive();

    private native boolean nativeActivate();

    private native boolean nativeDeactivate();

    private native boolean nativeSwapBuffers();

    private native boolean nativeAllocate();

    private native boolean nativeDeallocate();

    private native int nativeAddSurface(Surface surface);

    private native int nativeAddSurfaceTexture(SurfaceTexture surface, int width, int height);

    private native boolean nativeActivateSurfaceId(int surfaceId);

    private native boolean nativeRemoveSurfaceId(int surfaceId);
}
