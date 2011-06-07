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

import java.util.HashSet;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterConnection;
import android.filterfw.core.FrameFormat;

public class PortToPortConnection extends FilterConnection {

    private FilterPort mSourcePort;
    private FilterPort mTargetPort;

    // TODO: Move most of these to core internal methods?

    final boolean initWithConnection(FilterPort source, FilterPort target) {
        if (source == null || target == null) {
            throw new IllegalArgumentException(
                "Null port specified in connection setup!");
        } else if (source.getType() != FilterPort.OUTPUT_PORT) {
            throw new IllegalArgumentException(
                "Source port specfied in connection setup is not an output port!");
        } else if (target.getType() != FilterPort.INPUT_PORT) {
            throw new IllegalArgumentException(
                "Source port specfied in connection setup is not an input port!");
        }

        source.setConnection(this);
        target.setConnection(this);

        mSourcePort = source;
        mTargetPort = target;

        return true;
    }

    public final FilterPort getSourcePort() {
        return mSourcePort;
    }

    public final FilterPort getTargetPort() {
        return mTargetPort;
    }
}
