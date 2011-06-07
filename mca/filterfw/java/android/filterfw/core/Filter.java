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
import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterPort;
import android.filterfw.core.FrameManager;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.Protocol;
import android.filterfw.core.ProtocolException;
import android.filterfw.io.TextGraphReader;
import android.filterfw.io.GraphIOException;
import android.util.Log;

import java.lang.annotation.Annotation;
import java.lang.reflect.Field;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map.Entry;
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
    private KeyValueMap mUpdatedParams;

    private Protocol mFilterProtocol;

    private int mStatus = 0;
    private boolean mIsOpen = false;

    public Filter(String name) {
        mName = name;
        mPulledFrames = new HashSet<Frame>();
        mStatus = STATUS_READY;
    }

    public final void initWithParameterMap(KeyValueMap parameters) {
        getProtocol().assertKeyValueMapConforms(parameters);
        mParameters = parameters;
        setParameters(parameters, false);
        initFilter();
        setupIOPorts();
    }

    public final void initWithParameterString(String assignments) {
        try {
            KeyValueMap parameters = new TextGraphReader().readKeyValueAssignments(assignments);
            initWithParameterMap(parameters);
        } catch (GraphIOException e) {
            throw new IllegalArgumentException(e.getMessage());
        }
    }

    public final void initWithParameterList(Object... keyValues) {
        KeyValueMap parameters = new KeyValueMap();
        parameters.setKeyValues(keyValues);
        initWithParameterMap(parameters);
    }

    public final void init() throws ProtocolException {
        KeyValueMap parameters = new KeyValueMap();
        getProtocol().assertKeyValueMapConforms(parameters);
        initWithParameterMap(parameters);
    }

    public final void updateParameterMap(KeyValueMap parameters) {
        mParameters = getProtocol().updateKVMap(mParameters, parameters);
        mUpdatedParams = parameters;
    }

    public final void updateParameterList(Object... keyValues) {
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
        setExposedParameters(parameters, isUpdate);
        if (isUpdate) {
            parametersUpdated(parameters.keySet());
        }
    }

    protected void initFilter() {
    }

    protected void prepare(FilterContext context) {
    }

    protected void parametersUpdated(Set<String> updated) {
    }

    public abstract String[] getInputNames();

    public abstract String[] getOutputNames();

    public abstract boolean acceptsInputFormat(int index, FrameFormat format);

    public FrameFormat getInputFormat(int index) {
        FilterPort inPort = getInputPortAtIndex(index);
        return inPort.getFormat();
    }

    public abstract FrameFormat getOutputFormat(int index);

    public int open(FilterContext context) {
        int result = 0;
        if (getNumberOfInputs() > 0) {
            result |= STATUS_WAIT_FOR_ALL_INPUTS;
        }
        if (getNumberOfOutputs() > 0) {
            result |= STATUS_WAIT_FOR_FREE_OUTPUTS;
        }
        return result;
    }

    public abstract int process(FilterContext context);

    public int getSleepDelay() {
        return 250;
    }

    public void close(FilterContext context) {
    }

    public void tearDown(FilterContext context) {
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
        FilterPort result = mInputPortNames.get(portName);
        if (result == null) {
            throw new IllegalArgumentException("Unknown input port '" + portName + "' on filter "
                + this + "!");
        }
        return result;
    }

    public final FilterPort getOutputPort(String portName) {
        FilterPort result = mOutputPortNames.get(portName);
        if (result == null) {
            throw new IllegalArgumentException("Unknown output port '" + portName + "' on filter "
                + this + "!");
        }
        return result;
    }

    public final FilterPort getInputPortAtIndex(int index) {
        if (index >= mInputPorts.length) {
            throw new IllegalArgumentException("Input port index " + index + " on filter " + this
                + " out of bounds!");
        }
        return mInputPorts[index];
    }

    public final FilterPort getOutputPortAtIndex(int index) {
        if (index >= mOutputPorts.length) {
            throw new IllegalArgumentException("Output port index " + index + " on filter " + this
                + " out of bounds!");
        }
        return mOutputPorts[index];
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

    protected final Field getParameterField(String name) {
        // TODO: Add cache
        Class filterClass = getClass();
        FilterParameter param;
        for (Field field : filterClass.getDeclaredFields()) {
            if ((param = field.getAnnotation(FilterParameter.class)) != null) {
                String paramName = param.name().isEmpty() ? field.getName() : param.name();
                if (name.equals(paramName)) {
                    return field;
                }
            }
        }
        return null;
    }

    protected final ProgramVariable getProgramVariable(String name) {
        // TODO: Add cache
        Class filterClass = getClass();
        for (Field field : filterClass.getDeclaredFields()) {
            ProgramParameter[] params = getProgramParameters(field);
            for (ProgramParameter param : params) {
                String varName = param.name();
                String exposedName = param.exposedName().isEmpty() ? varName : param.exposedName();
                if (name.equals(exposedName)) {
                    field.setAccessible(true);
                    try {
                        return new ProgramVariable((Program)field.get(this), varName);
                    } catch (IllegalAccessException e) {
                        throw new RuntimeException(
                            "Access to field '" + field.getName() + "' was denied!");
                    } catch (ClassCastException e) {
                        throw new RuntimeException(
                            "Non Program field '" + field.getName() + "' annotated with "
                            + " ProgramParameter!");
                    }
                }
            }
        }
        return null;
    }

    protected final ProgramParameter[] getProgramParameters(Field field) {
        Annotation annotation;
        if ((annotation = field.getAnnotation(ProgramParameter.class)) != null) {
            ProgramParameter[] result = new ProgramParameter[1];
            result[0] = (ProgramParameter)annotation;
            return result;
        } else if ((annotation = field.getAnnotation(ProgramParameters.class)) != null) {
            return ((ProgramParameters)annotation).value();
        } else {
            return new ProgramParameter[0];
        }
    }

    protected final boolean setParameterField(String name, Object value) {
        Field field = getParameterField(name);
        if (field != null) {
            field.setAccessible(true);
            try {
                field.set(this, value);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(
                    "Access to field '" + field.getName() + "' was denied!");
            }
            return true;
        }
        return false;
    }

    protected final boolean setProgramVariable(String name, Object value, boolean isUpdate) {
        ProgramVariable programVar = getProgramVariable(name);
        if (programVar != null) {
            // If this is not an update, postpone until process to ensure program is setup
            if (!isUpdate) {
                if (mUpdatedParams == null) {
                    mUpdatedParams = new KeyValueMap();
                }
                mUpdatedParams.put(name, value);
            } else {
                programVar.setValue(value);
            }
            return true;
        }
        return false;
    }

    protected final void setExposedParameters(KeyValueMap keyValueMap, boolean isUpdate) {
        // Go through all parameter entries
        for (Entry<String, Object> entry : keyValueMap.entrySet()) {
            // Try setting filter field
            if (!setParameterField(entry.getKey(), entry.getValue())) {
                // If that fails, try setting a program variable
                if (!setProgramVariable(entry.getKey(), entry.getValue(), isUpdate)) {
                    // If that fails too, throw an exception
                    throw new RuntimeException("Attempting to set unknown parameter '"
                        + entry.getKey() + "' on filter " + this + "!");
                }
            }
        }
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

    final boolean performOpen(FilterContext context) {
        synchronized(this) {
            mStatus = open(context);
        }
        mIsOpen = true;
        return mStatus != STATUS_ERROR;
    }

    final int performProcess(FilterContext context) {
        synchronized(this) {
            if (mUpdatedParams != null) {
                setParameters(mUpdatedParams, true);
                mUpdatedParams = null;
            }
            mStatus = process(context);
            releasePulledFrames(context);
        }
        return mStatus;
    }

    final void performClose(FilterContext context) {
        synchronized(this) {
            mIsOpen = false;
            close(context);
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

    private final void releasePulledFrames(FilterContext context) {
        for (Frame frame : mPulledFrames) {
            context.getFrameManager().releaseFrame(frame);
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

