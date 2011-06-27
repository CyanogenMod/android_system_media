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
import android.view.Surface;

public class GLEnvironment {

    private int glEnvId;

    public GLEnvironment() {
        allocate();
    }

    private GLEnvironment(NativeAllocatorTag tag) {
    }

    public static native GLEnvironment activeEnvironment();

    @Override
    protected void finalize() throws Throwable {
        deallocate();
    }

    public native boolean initWithNewContext();

    public native boolean initWithCurrentContext();

    public native boolean activate();

    public native boolean deactivate();

    public native boolean swapBuffers();

    public int registerSurface(Surface surface) {
        return nativeAddSurface(surface);
    }

    public void activateSurfaceWithId(int surfaceId) {
        nativeActivateSurfaceId(surfaceId);
    }

    public void unregisterSurfaceId(int surfaceId) {
        nativeRemoveSurfaceId(surfaceId);
    }

    static {
        System.loadLibrary("filterfw");
    }

    private native boolean allocate();

    private native boolean deallocate();

    private native int nativeAddSurface(Surface surface);

    private native boolean nativeActivateSurfaceId(int surfaceId);

    private native boolean nativeRemoveSurfaceId(int surfaceId);
}
