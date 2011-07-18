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

import android.filterfw.core.Filter;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameManager;
import android.filterfw.core.GLEnvironment;

import java.util.HashMap;

/**
 * @hide
 */
public class FilterContext {

    private FrameManager mFrameManager;
    private GLEnvironment mGLEnvironment;
    private HashMap<String, Frame> mStoredFrames;

    public FilterContext() {
        mStoredFrames = new HashMap<String, Frame>();
    }

    @Override
    protected void finalize() {
        for (Frame frame : mStoredFrames.values()) {
            frame.release();
        }
        synchronized(mStoredFrames) {
            mStoredFrames.clear();
        }
    }

    public FrameManager getFrameManager() {
        return mFrameManager;
    }

    public void setFrameManager(FrameManager manager) {
        if (manager == null) {
            throw new NullPointerException("Attempting to set null FrameManager!");
        } else if (manager.getContext() != null) {
            throw new IllegalArgumentException("Attempting to set FrameManager which is already "
                + "bound to another FilterContext!");
        } else {
            mFrameManager = manager;
            mFrameManager.setContext(this);
        }
    }

    public GLEnvironment getGLEnvironment() {
        return mGLEnvironment;
    }

    public void initGLEnvironment(GLEnvironment environment) {
        if (mGLEnvironment == null) {
            mGLEnvironment = environment;
        } else {
            throw new RuntimeException("Attempting to re-initialize GL Environment for " +
                "FilterContext!");
        }
    }

    public interface OnFrameReceivedListener {
        public void onFrameReceived(Filter filter, Frame frame, Object userData);
    }

    public synchronized void storeFrame(String key, Frame frame) {
        Frame storedFrame = fetchFrame(key);
        if (storedFrame != null) {
            storedFrame.release();
        }
        mStoredFrames.put(key, frame.retain());
    }

    public synchronized Frame fetchFrame(String key) {
        return mStoredFrames.get(key);
    }

    public synchronized void removeFrame(String key) {
        Frame frame = mStoredFrames.get(key);
        if (frame != null) {
            mStoredFrames.remove(key);
            frame.release();
        }
    }
}
