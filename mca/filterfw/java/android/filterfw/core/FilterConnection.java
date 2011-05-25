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

import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;

public class FilterConnection {

    protected Frame mFrameOnWire;

    public void setFormat(FrameFormat format) {
    }

    public FrameFormat getFormat() {
        return null;
    }

    public Frame getFrame() {
        return mFrameOnWire;
    }

    public boolean hasFrame() {
        return mFrameOnWire != null;
    }

    public boolean putFrame(Frame frame) {
        if (mFrameOnWire == null) {
            mFrameOnWire = frame.retain();
            return true;
        }
        return false;
    }

    public boolean clearFrame() {
        if (mFrameOnWire != null) {
            mFrameOnWire.release();
            mFrameOnWire = null;
            return true;
        }
        return false;
    }

    public FilterPort getSourcePort() {
        return null;
    }

    public FilterPort getTargetPort() {
        return null;
    }

}
