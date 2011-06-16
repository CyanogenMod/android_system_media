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
import android.filterfw.core.FrameManager;
import android.filterfw.core.GLFrame;
import android.filterfw.core.NativeFrame;
import android.filterfw.core.JavaFrame;
import android.filterfw.core.VertexFrame;
import android.util.Log;

public class SimpleFrameManager extends FrameManager {

    public SimpleFrameManager() {
    }

    @Override
    public Frame newFrame(FrameFormat format) {
        //Log.v("FrameManager", "Creating new frame of format: " + format + ".");
        return createNewFrame(format, false);
    }

    @Override
    public Frame newEmptyFrame(FrameFormat format) {
        return createNewFrame(format, true);
    }

    @Override
    public Frame newBoundFrame(FrameFormat format, int bindingType, long bindingId) {
        Frame result = null;
        switch(format.getTarget()) {
            case FrameFormat.TARGET_GPU: {
                GLFrame glFrame = new GLFrame(format, this, bindingType, bindingId);
                glFrame.init(false);
                result = glFrame;
                break;
            }

            default:
                throw new RuntimeException("Attached frames are not supported for target type: "
                    + FrameFormat.targetToString(format.getTarget()) + "!");
        }
        return result;
    }

    private Frame createNewFrame(FrameFormat format, boolean isEmpty) {
        Frame result = null;
        switch(format.getTarget()) {
            case FrameFormat.TARGET_JAVA:
                result = new JavaFrame(format, this, isEmpty);
                break;

            case FrameFormat.TARGET_NATIVE:
                result = new NativeFrame(format, this, isEmpty);
                break;

            case FrameFormat.TARGET_GPU: {
                GLFrame glFrame = new GLFrame(format, this);
                glFrame.init(isEmpty);
                result = glFrame;
                break;
            }

            case FrameFormat.TARGET_VERTEXBUFFER: {
                result = new VertexFrame(format, this, isEmpty);
                break;
            }

            default:
                throw new RuntimeException("Unsupported frame target type: " +
                                           FrameFormat.targetToString(format.getTarget()) + "!");
        }
        return result;
    }

    @Override
    public Frame duplicateFrame(Frame frame) {
        // TODO
        return null;
    }

    @Override
    public Frame retainFrame(Frame frame) {
        frame.incRefCount();
        return frame;
    }

    @Override
    public Frame releaseFrame(Frame frame) {
        int refCount = frame.decRefCount();
        if (refCount == 0) {
            frame.dealloc();
            return null;
        } else if (refCount < 0) {
            throw new RuntimeException("Frame reference count dropped below 0!");
        }
        return frame;
    }

}
