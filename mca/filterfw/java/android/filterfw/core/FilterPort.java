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
import android.filterfw.core.PortToPortConnection;

public class FilterPort {

    public static final int UNATTACHED_PORT = 0;
    public static final int INPUT_PORT      = 1;
    public static final int OUTPUT_PORT     = 2;

    private int mType;

    private Filter mFilter;

    private int mIndex;

    private FilterConnection mConnection;

    public FilterPort(Filter filter, int type, int index) {
        mType = type;
        mIndex = index;
        mFilter = filter;
    }

    public boolean isAttached() {
        return mFilter != null;
    }

    public int getType() {
        return mType;
    }

    public FrameFormat getFormat() {
        return mConnection != null ? mConnection.getFormat() : null;
    }

    public void setFormat(FrameFormat format) {
        if (mConnection == null) {
            throw new RuntimeException("Attempting to set format on unconnected port!");
        }
        mConnection.setFormat(format);
    }

    public Filter getFilter() {
        return mFilter;
    }

    public Filter getSourceFilter() {
        if (getType() != INPUT_PORT) {
            throw new RuntimeException("Cannot get source port of non-input port!");
        } else if (mConnection == null || mConnection.getSourcePort() == null) {
            throw new RuntimeException("Attempting to get source of disconnected port!");
        }
        return getConnection().getSourcePort().getFilter();
    }

    public Filter getTargetFilter() {
        if (getType() != OUTPUT_PORT) {
            throw new RuntimeException("Cannot get target port of non-output port!");
        } else if (mConnection == null || mConnection.getTargetPort() == null) {
            throw new RuntimeException("Attempting to get target of disconnected port!");
        }
        return getConnection().getTargetPort().getFilter();
    }

    public int getIndex() {
        return mIndex;
    }

    public FilterConnection getConnection() {
        return mConnection;
    }

    public void connectTo(FilterPort port) {
        if (getConnection() != null) {
            throw new RuntimeException("Attempting to connect already connected port!");
        } else if (getType() != OUTPUT_PORT) {
            throw new RuntimeException("Attempting to connect from non-output port!");
        } else if (port == null) {
            throw new RuntimeException("Attempting to connect to null port!");
        } else if (port.getType() != INPUT_PORT) {
            throw new RuntimeException("Attempting to connect to non-input port!");
        }
        PortToPortConnection connection = new PortToPortConnection();
        connection.initWithConnection(this, port);
    }

    public void setConnection(FilterConnection connection) {
        if (connection == null) {
            throw new IllegalArgumentException("Attempting to set null connection!");
        } else if (mConnection != null) {
            throw new RuntimeException("Attempting to set connection on already connected port!");
        }
        mConnection = connection;
    }

    public boolean isConnected() {
        return getConnection() != null;
    }

    public String toString() {
        switch(mType) {
            case INPUT_PORT:
                return "input port " + mIndex + " of Filter " + mFilter;
            case OUTPUT_PORT:
                return "output port " + mIndex + " of Filter " + mFilter;
            default:
                return "unattached port";
        }
    }
}
