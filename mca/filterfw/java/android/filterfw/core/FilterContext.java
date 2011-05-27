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
import android.filterfw.core.GraphRunner;

import java.util.HashMap;
import java.util.HashSet;

public class FilterContext {

    private FrameManager mFrameManager;
    private GLEnvironment mGLEnvironment;
    private HashSet<GraphRunner> mRunners;
    private HashMap<String, Frame> mStoredFrames;

    public FilterContext() {
        mRunners = new HashSet<GraphRunner>();
        mStoredFrames = new HashMap<String, Frame>();
    }

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
        mFrameManager = manager;
    }

    public GLEnvironment getGLEnvironment() {
        return mGLEnvironment;
    }

    public void setGLEnvironment(GLEnvironment environment) {
        mGLEnvironment = environment;
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

    // Core internal methods /////////////////////////////////////////////////////////////////////////
    void addRunner(GraphRunner runner) {
        mRunners.add(runner);
    }

    void removeRunner(GraphRunner runner) {
        mRunners.remove(runner);
    }
}
