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
import android.filterfw.core.SimpleFrameManager;

import java.util.LinkedList;
import java.util.ListIterator;
import java.util.HashMap;

public class CachedFrameManager extends SimpleFrameManager {

    private LinkedList<Frame> mAvailableFrames;
    private int mStorageCapacity = 32 * 1024 * 1024; // Cap default storage to 32MB
    private int mStorageSize = 0;

    public CachedFrameManager() {
        super();
        mAvailableFrames = new LinkedList<Frame>();
    }

    public Frame newFrame(FrameFormat format) {
        Frame result = findAvailableFrame(format, Frame.NO_BINDING, 0);
        if (result == null) {
            result = super.newFrame(format);
        }
        return result;
    }

    public Frame newEmptyFrame(FrameFormat format) {
        return super.newEmptyFrame(format);
    }

    public Frame newBoundFrame(FrameFormat format, int bindingType, long bindingId) {
        Frame result = findAvailableFrame(format, bindingType, bindingId);
        if (result == null) {
            result = super.newBoundFrame(format, bindingType, bindingId);
        }
        return result;
    }

    public Frame duplicateFrame(Frame frame) {
        // TODO
        return null;
    }

    public Frame retainFrame(Frame frame) {
        return super.retainFrame(frame);
    }

    public Frame releaseFrame(Frame frame) {
        if (frame.isReusable()) {
            int refCount = frame.decRefCount();
            if (refCount == 0) {
                if (!storeFrame(frame)) {
                    frame.dealloc();
                }
                return null;
            } else if (refCount < 0) {
                throw new RuntimeException("Frame reference count dropped below 0!");
            }
        } else {
            super.releaseFrame(frame);
        }
        return frame;
    }

    private boolean storeFrame(Frame frame) {
        synchronized(mAvailableFrames) {
            int newStorageSize = mStorageSize + frame.getFormat().getSize();
            if (newStorageSize <= mStorageCapacity) {
                mStorageSize = newStorageSize;
                mAvailableFrames.add(frame);
                return true;
            }
            return false;
        }
    }

    private Frame findAvailableFrame(FrameFormat format, int bindingType, long bindingId) {
        // Look for a frame that is compatible with the requested format
        synchronized(mAvailableFrames) {
            ListIterator<Frame> iter = mAvailableFrames.listIterator();
            while (iter.hasNext()) {
                Frame frame = iter.next();
                // Check that format is compatible
                if (frame.getFormat().isReplaceableBy(format)) {
                    // Check that binding is compatible (if frame is bound)
                    if (bindingType == Frame.NO_BINDING
                        || (bindingId == frame.getBindingId()
                            && bindingType == frame.getBindingType())) {
                        // We found one! Take it out of the set of available frames and attach the
                        // requested format to it.
                        super.retainFrame(frame);
                        iter.remove();
                        frame.reset(format);
                        mStorageSize -= format.getSize();
                        return frame;
                    }
                }
            }
        }
        return null;
    }
}
