/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *            http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package android.filterfw.core;

import android.content.Context;
import android.filterfw.core.FilterEnvironment;
import android.filterfw.core.FilterPort;
import android.filterfw.core.FrameManager;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.Protocol;
import android.filterfw.core.ProtocolException;
import android.util.Log;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Set;

public abstract class Filter {

    public static final int STATUS_WAIT_FOR_ALL_INPUTS   = 0x0001;
    public static final int STATUS_WAIT_FOR_ONE_INPUT    = 0x0002;
    public static final int STATUS_WAIT_FOR_FREE_OUTPUTS = 0x0004;
    public static final int STATUS_WAIT_FOR_FREE_OUTPUT  = 0x0008;
    public static final int STATUS_PAUSE                 = 0x0010;
    public static final int STATUS_SLEEP                 = 0x0020;

    public static final int STATUS_READY                 = 0x0040;

    public static final int STATUS_FINISHED              = 0x0080;
    public static final int STATUS_ERROR                 = 0x0100;

    private String mName;

    private int mInputCount = -1;
    private int mOutputCount = -1;

    private FilterPort[] mInputPorts;
    private FilterPort[] mOutputPorts;

    private HashMap<String, FilterPort> mInputPortNames;
    private HashMap<String, FilterPort> mOutputPortNames;

    private HashSet<Frame> mPulledFrames;

    private KeyValueMap mParameters;
    private Protocol mFilterProtocol;

    private int mStatus = 0;
    private boolean mIsOpen = false;

    public Filter(String name) {
        mName = name;
        mPulledFrames = new HashSet<Frame>();
        mStatus = STATUS_READY;
    }

    public final void initWithParameterMap(KeyValueMap parameters) throws ProtocolException {
        parameters.assertConformsToProtocol(getProtocol());
        mParameters = parameters;
        setParameters(parameters, false);
        initFilter();
        setupIOPorts();
    }

    public final void initWithParameterString(String assignments) throws ProtocolException {
        KeyValueMap parameters = new KeyValueMap();
        parameters.readAssignments(assignments);
        initWithParameterMap(parameters);
    }

    public final void initWithParameterList(Object... keyValues) throws ProtocolException {
        KeyValueMap parameters = new KeyValueMap();
        parameters.setKeyValues(keyValues);
        initWithParameterMap(parameters);
    }

    public final void init() throws ProtocolException {
        KeyValueMap parameters = new KeyValueMap();
        parameters.assertConformsToProtocol(getProtocol());
        initWithParameterMap(parameters);
    }

    public final void updateParameterMap(KeyValueMap parameters) throws ProtocolException {
        mParameters = getProtocol().updateKVMap(mParameters, parameters);
        synchronized(this) {
            setParameters(parameters, true);
        }
        parametersUpdated(parameters.keySet());
    }

    public final void updateParameterList(Object... keyValues) throws ProtocolException {
        KeyValueMap parameters = new KeyValueMap();
        parameters.setKeyValues(keyValues);
        updateParameterMap(parameters);
    }

    public final KeyValueMap getParameters() {
        return mParameters;
    }

    public final Protocol getProtocol() {
        if (mFilterProtocol == null) {
            mFilterProtocol = createParameterProtocol();
        }
        return mFilterProtocol;
    }

    public String getFilterClassName() {
        return getClass().getSimpleName();
    }

    public final String getName() {
        return mName;
    }

    public boolean isOpen() {
        return mIsOpen;
    }

    protected Protocol createParameterProtocol() {
        return Protocol.fromFilter(this);
    }

    protected void setParameters(KeyValueMap parameters, boolean isUpdate) {
        parameters.setFilterParameters(this);
    }

    protected void initFilter() {
    }

    protected void prepare(FilterEnvironment environment) {
    }

    protected void parametersUpdated(Set<String> updated) {
    }

    public abstract String[] getInputNames();

    public abstract String[] getOutputNames();

    public abstract boolean setInputFormat(int index, FrameFormat format);

    public abstract FrameFormat getFormatForOutput(int index);

    public int open(FilterEnvironment env) {
        int result = 0;
        if (getNumberOfInputs() > 0) {
            result |= STATUS_WAIT_FOR_ALL_INPUTS;
        }
        if (getNumberOfOutputs() > 0) {
            result |= STATUS_WAIT_FOR_FREE_OUTPUTS;
        }
        return result;
    }

    public abstract int process(FilterEnvironment env);

    public int getSleepDelay() {
        return 250;
    }

    public void close(FilterEnvironment env) {
    }

    public void tearDown(FilterEnvironment env) {
    }

    public final int getNumberOfInputs() {
        if (mInputCount < 0) {
            String[] names = getInputNames();
            mInputCount = names != null ? names.length : 0;
        }
        return mInputCount;
    }

    public final int getNumberOfOutputs() {
        if (mOutputCount < 0) {
            String[] names = getOutputNames();
            mOutputCount = names != null ? names.length : 0;
        }
        return mOutputCount;
    }

    public final FilterPort getInputPort(String portName) {
        return mInputPortNames.get(portName);
    }

    public final FilterPort getOutputPort(String portName) {
        return mOutputPortNames.get(portName);
    }

    public final FilterPort getInputPortAtIndex(int index) {
        return index < mInputPorts.length ? mInputPorts[index] : null;
    }

    public final FilterPort getOutputPortAtIndex(int index) {
        return index < mOutputPorts.length ? mOutputPorts[index] : null;
    }

    protected final void putOutput(int index, Frame frame) {
        if (index >= mOutputPorts.length) {
            throw new RuntimeException(
                "Attempting to push output to invalid port index " + index + "!");
        } else if (mOutputPorts[index] == null) {
            throw new RuntimeException(
                "Attempting to push output to null port at index " + index + "!");
        } else if (mOutputPorts[index].getConnection().putFrame(frame)) {
            frame.markReadOnly();
        } else {
            throw new RuntimeException("Cannot push output onto blocked connection!");
        }
    }

    protected final Frame pullInput(int index) {
        if (index >= mInputPorts.length) {
            throw new RuntimeException(
                "Attempting to pull input from invalid port index " + index + "!");
        } else if (mInputPorts[index] == null) {
            throw new RuntimeException(
                "Attempting to pull input from null port at index " + index + "!");
        }
        FilterConnection connection = mInputPorts[index].getConnection();
        Frame result = connection.getFrame().retain();
        connection.clearFrame();
        mPulledFrames.add(result);
        return result;
    }

    public String toString() {
        return "'" + getName() + "' (" + getFilterClassName() + ")";
    }

    // Core internal methods ///////////////////////////////////////////////////////////////////////
    final void setInputFrame(int index, Frame frame) {
        if (index < mInputPorts.length && mInputPorts[index] != null) {
            mInputPorts[index].getConnection().putFrame(frame);
        }
    }

    final synchronized boolean checkStatus(int flag) {
        return (mStatus & flag) != 0;
    }

    final synchronized void unsetStatus(int flag) {
        mStatus &= ~flag;
    }

    final boolean performOpen(FilterEnvironment env) {
        synchronized(this) {
            mStatus = open(env);
        }
        mIsOpen = true;
        return mStatus != STATUS_ERROR;
    }

    final int performProcess(FilterEnvironment env) {
        synchronized(this) {
            mStatus = process(env);
            releasePulledFrames(env);
        }
        return mStatus;
    }

    final void performClose(FilterEnvironment env) {
        synchronized(this) {
            mIsOpen = false;
            close(env);
        }
    }

    synchronized final boolean canProcess() {
        // Check general filter state
        if ((mStatus & STATUS_READY) != 0) {
            return true;
        } else if (((mStatus & STATUS_FINISHED) != 0) ||
                   ((mStatus & STATUS_ERROR) != 0)    ||
                   ((mStatus & STATUS_SLEEP) != 0)) {
            return false;
        }

        // Check input waiting state
        if ((mStatus & STATUS_WAIT_FOR_ALL_INPUTS) != 0) {
            if (inputFramesWaiting() != getNumberOfInputs()) return false;
        } else if ((mStatus & STATUS_WAIT_FOR_ONE_INPUT) != 0) {
            if (inputFramesWaiting() == 0) return false;
        }

        // Check output waiting state
        if ((mStatus & STATUS_WAIT_FOR_FREE_OUTPUTS) != 0) {
            if (outputFramesWaiting() != 0) return false;
        } else if ((mStatus & STATUS_WAIT_FOR_FREE_OUTPUT) != 0) {
            if (outputFramesWaiting() == getNumberOfOutputs()) return false;
        }

        return true;
    }

    final boolean allInputsConnected() {
        for (FilterPort port : mInputPorts) {
            if (!port.isConnected()) {
                return false;
            }
        }
        return true;
    }

    final boolean allOutputsConnected() {
        for (FilterPort port : mOutputPorts) {
            if (!port.isConnected()) {
                return false;
            }
        }
        return true;
    }

    final LinkedList<Filter> connectedFilters() {
        LinkedList<Filter> result = new LinkedList<Filter>();
        for (FilterPort port : mInputPorts) {
            if (port.isConnected()) {
                result.add(port.getSourceFilter());
            }
        }
        for (FilterPort port : mOutputPorts) {
            if (port.isConnected()) {
                result.add(port.getTargetFilter());
            }
        }
        return result;
    }

    // Filter internal methods /////////////////////////////////////////////////////////////////////
    private final void setupIOPorts() {
        mInputPorts = new FilterPort[getNumberOfInputs()];
        for (int i = 0; i < mInputPorts.length; ++i) {
            mInputPorts[i] = new FilterPort(this, FilterPort.INPUT_PORT, i);
        }

        mOutputPorts = new FilterPort[getNumberOfOutputs()];
        for (int i = 0; i < mOutputPorts.length; ++i) {
            mOutputPorts[i] = new FilterPort(this, FilterPort.OUTPUT_PORT, i);
        }

        setupIOPortNames();
    }

    private final void setupIOPortNames() {
        mInputPortNames = new HashMap<String, FilterPort>();
        mOutputPortNames = new HashMap<String, FilterPort>();

        setupInputPortNames();
        setupOutputPortNames();
    }

    private final void setupInputPortNames() {
        int i = 0;
        String[] names = getInputNames();
        if (names != null) {
            for (String name : names) {
                if (name != null) {
                    mInputPortNames.put(name, mInputPorts[i++]);
                } else {
                    throw new RuntimeException("Got null name for input port " + i +
                                               " on Filter " + this);
                }
            }
        }
    }

    private final void setupOutputPortNames() {
        int i = 0;
        String[] names = getOutputNames();
        if (names != null) {
            for (String name : names) {
                if (name != null) {
                    mOutputPortNames.put(name, mOutputPorts[i++]);
                } else {
                    throw new RuntimeException("Got null name for output port " + i +
                                               " on Filter " + this);
                }
            }
        }
    }

    private final void releasePulledFrames(FilterEnvironment env) {
        for (Frame frame : mPulledFrames) {
            env.getFrameManager().releaseFrame(frame);
        }
        mPulledFrames.clear();
    }

    private final int inputFramesWaiting() {
        int c = 0;
        for (FilterPort port : mInputPorts) {
            if (port.getConnection().hasFrame()) {
                ++c;
            }
        }
        return c;
    }

    private final int outputFramesWaiting() {
        int c = 0;
        for (FilterPort port : mOutputPorts) {
            if (port.getConnection().hasFrame()) {
                ++c;
            }
        }
        return c;
    }
}

