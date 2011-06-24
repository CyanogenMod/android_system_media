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

import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterPort;
import android.filterfw.core.KeyValueMap;
import android.filterfw.io.TextGraphReader;
import android.filterfw.io.GraphIOException;
import android.filterfw.format.ObjectFormat;
import android.util.Log;

import java.lang.annotation.Annotation;
import java.lang.reflect.Field;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map.Entry;
import java.util.LinkedList;
import java.util.Set;

public abstract class Filter {

    static final int STATUS_PREINIT               = 0;
    static final int STATUS_UNPREPARED            = 1;
    static final int STATUS_READY                 = 2;
    static final int STATUS_PROCESSING            = 3;
    static final int STATUS_SLEEPING              = 4;
    static final int STATUS_FINISHED              = 5;
    static final int STATUS_ERROR                 = 6;

    private String mName;

    private int mInputCount = -1;
    private int mOutputCount = -1;

    private HashMap<String, TargetPort> mInputPorts;
    private HashMap<String, SourcePort> mOutputPorts;

    private HashSet<Frame> mFramesToRelease;
    private HashMap<String, Frame> mFramesToSet;

    private int mStatus = 0;
    private boolean mIsOpen = false;
    private int mSleepDelay;

    public Filter(String name) {
        mName = name;
        mFramesToRelease = new HashSet<Frame>();
        mFramesToSet = new HashMap<String, Frame>();
        mStatus = STATUS_PREINIT;
    }

    public final void initWithValueMap(KeyValueMap valueMap) {
        // Initialization
        initFinalPorts(valueMap);

        // Setup remaining ports
        initRemainingPorts(valueMap);

        // This indicates that final ports can no longer be set
        mStatus = STATUS_UNPREPARED;
    }

    public final void initWithAssignmentString(String assignments) {
        try {
            KeyValueMap valueMap = new TextGraphReader().readKeyValueAssignments(assignments);
            initWithValueMap(valueMap);
        } catch (GraphIOException e) {
            throw new IllegalArgumentException(e.getMessage());
        }
    }

    public final void initWithAssignmentList(Object... keyValues) {
        KeyValueMap valueMap = new KeyValueMap();
        valueMap.setKeyValues(keyValues);
        initWithValueMap(valueMap);
    }

    public final void init() throws ProtocolException {
        KeyValueMap valueMap = new KeyValueMap();
        initWithValueMap(valueMap);
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

    public synchronized void setInputFrame(String inputName, Frame frame) {
        FilterPort port = getInputPort(inputName);
        if (!port.isOpen()) {
            port.open();
        }
        port.setFrame(frame);
    }

    public final void setInputValue(String inputName, Object value) {
        Log.v("Filter", "Setting deferred value " + value + " for input '" + inputName + "'!");
        setInputFrame(inputName, JavaFrame.wrapObject(value, null));
    }

    protected void prepare(FilterContext context) {
    }

    protected void parametersUpdated(Set<String> updated) {
    }

    protected void delayNextProcess(int millisecs) {
        mSleepDelay = millisecs;
        mStatus = STATUS_SLEEPING;
    }

    public abstract void setupPorts();

    public FrameFormat getOutputFormat(String portName, FrameFormat inputFormat) {
        return null;
    }

    public final FrameFormat getInputFormat(String portName) {
        TargetPort inputPort = getInputPort(portName);
        return inputPort.getSourceFormat();
    }

    public void open(FilterContext context) {
    }

    public abstract void process(FilterContext context);

    public final int getSleepDelay() {
        return 250;
    }

    public void close(FilterContext context) {
    }

    public void tearDown(FilterContext context) {
    }

    public final int getNumberOfConnectedInputs() {
        int c = 0;
        for (TargetPort inputPort : mInputPorts.values()) {
            if (inputPort.isConnected()) {
                ++c;
            }
        }
        return c;
    }

    public final int getNumberOfConnectedOutputs() {
        int c = 0;
        for (SourcePort outputPort : mOutputPorts.values()) {
            if (outputPort.isConnected()) {
                ++c;
            }
        }
        return c;
    }

    public final int getNumberOfInputs() {
        return mOutputPorts == null ? 0 : mInputPorts.size();
    }

    public final int getNumberOfOutputs() {
        return mInputPorts == null ? 0 : mOutputPorts.size();
    }

    public final TargetPort getInputPort(String portName) {
        TargetPort result = mInputPorts.get(portName);
        if (result == null) {
            throw new IllegalArgumentException("Unknown input port '" + portName + "' on filter "
                + this + "!");
        }
        return result;
    }

    public final SourcePort getOutputPort(String portName) {
        SourcePort result = mOutputPorts.get(portName);
        if (result == null) {
            throw new IllegalArgumentException("Unknown output port '" + portName + "' on filter "
                + this + "!");
        }
        return result;
    }

    protected final void pushOutput(String name, Frame frame) {
        getOutputPort(name).pushFrame(frame);
    }

    protected final Frame pullInput(String name) {
        Frame result = getInputPort(name).pullFrame();

        // As result is retained, we add it to the release pool here
        mFramesToRelease.add(result);

        return result;
    }

    public void fieldPortValueUpdated(String name, FilterContext context) {
    }

    /**
     * Adds an input port to the filter. You should call this from within setupPorts, if your
     * filter has input ports. No type-checking is performed on the input. If you would like to
     * check against a type mask, use
     * {@link #addMaskedInputPort(String, FrameFormat) addMaskedInputPort} instead.
     *
     * @param name the name of the input port
     */
    protected void addInputPort(String name) {
        addMaskedInputPort(name, null);
    }

    /**
     * Adds an input port to the filter. You should call this from within setupPorts, if your
     * filter has input ports. When type-checking is performed, the input format is
     * checked against the provided format mask. An exception is thrown in case of a conflict.
     *
     * @param name the name of the input port
     * @param formatMask a format mask, which filters the allowable input types
     */
    protected void addMaskedInputPort(String name, FrameFormat formatMask) {
        TargetPort port = new InputPort(this, name);
        Log.v("Filter", "Filter " + this + " adding " + port);
        mInputPorts.put(name, port);
        port.setPortFormat(formatMask);
    }

    /**
     * Adds an output port to the filter with a fixed output format. You should call this from
     * within setupPorts, if your filter has output ports. You cannot use this method, if your
     * output format depends on the input format (e.g. in a pass-through filter). In this case, use
     * {@link #addOutputBasedOnInput(String, String) addOutputBasedOnInput} instead.
     *
     * @param name the name of the output port
     * @param format the fixed output format of this port
     */
    protected void addOutputPort(String name, FrameFormat format) {
        OutputPort port = new OutputPort(this, name);
        Log.v("Filter", "Filter " + this + " adding " + port);
        port.setPortFormat(format);
        mOutputPorts.put(name, port);
    }

    /**
     * Adds an output port to the filter. You should call this from within setupPorts, if your
     * filter has output ports. Using this method indicates that the output format for this
     * particular port, depends on the format of an input port. You MUST also override
     * {@link #getOutputFormat(String, FrameFormat) getOutputFormat} to specify what format your
     * filter will output for a given input. If the output format of your filter port does not
     * depend on the input, use {@link #addOutputPort(String, FrameFormat) addOutputPort} instead.
     *
     * @param outputName the name of the output port
     * @param inputName the name of the input port, that this output depends on
     */
    protected void addOutputBasedOnInput(String outputName, String inputName) {
        OutputPort port = new OutputPort(this, outputName);
        Log.v("Filter", "Filter " + this + " adding " + port);
        port.setBasePort(getInputPort(inputName));
        mOutputPorts.put(outputName, port);
    }

    protected void addFieldPort(String name,
                                Field field,
                                boolean hasDefault,
                                boolean isFinal) {
        // Make sure field is accessible
        field.setAccessible(true);

        // Create port for this input
        TargetPort fieldPort = isFinal
            ? new FinalPort(this, name, field, hasDefault)
            : new FieldPort(this, name, field, hasDefault);

        // Create format for this input
        Log.v("Filter", "Filter " + this + " adding " + fieldPort);
        MutableFrameFormat format = ObjectFormat.fromClass(field.getType(),
                                                           FrameFormat.TARGET_JAVA);
        fieldPort.setPortFormat(format);

        // Add port
        mInputPorts.put(name, fieldPort);
    }

    protected void addProgramPort(String name,
                                  String varName,
                                  Field field,
                                  Class varType,
                                  boolean hasDefault) {
        // Make sure field is accessible
        field.setAccessible(true);

        // Create port for this input
        TargetPort programPort = new ProgramPort(this, name, varName, field, hasDefault);

        // Create format for this input
        Log.v("Filter", "Filter " + this + " adding " + programPort);
        MutableFrameFormat format = ObjectFormat.fromClass(varType,
                                                           FrameFormat.TARGET_JAVA);
        programPort.setPortFormat(format);

        // Add port
        mInputPorts.put(name, programPort);
    }

    protected void closeOutputPort(String name) {
        getOutputPort(name).close();
    }

/*
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
*/

    public String toString() {
        return "'" + getName() + "' (" + getFilterClassName() + ")";
    }

    // Core internal methods ///////////////////////////////////////////////////////////////////////
    final Collection<TargetPort> getInputPorts() {
        return mInputPorts.values();
    }

    final Collection<SourcePort> getOutputPorts() {
        return mOutputPorts.values();
    }

    final synchronized int getStatus() {
        return mStatus;
    }

    final synchronized void unsetStatus(int flag) {
        mStatus &= ~flag;
    }

    final synchronized void performOpen(FilterContext context) {
        if (!mIsOpen) {
            if (mStatus == STATUS_UNPREPARED) {
                Log.v("Filter", "Preparing " + this);
                prepare(context);
                mStatus = STATUS_READY;
            }
            if (mStatus == STATUS_READY) {
                Log.v("Filter", "Opening " + this);
                open(context);
                mStatus = STATUS_PROCESSING;
            }
            if (mStatus != STATUS_PROCESSING) {
                throw new RuntimeException("Filter " + this + " was brought into invalid state during "
                    + "opening (state: " + mStatus + ")!");
            }
            mIsOpen = true;
        }
    }

    final synchronized void performProcess(FilterContext context) {
        transferInputFrames(context);
        if (mStatus < STATUS_PROCESSING) {
            performOpen(context);
        }
        Log.v("Filter", "Processing " + this);
        process(context);
        releasePulledFrames(context);
        if (filterMustClose()) {
            performClose(context);
        }
    }

    final synchronized void performClose(FilterContext context) {
        if (mIsOpen) {
            Log.v("Filter", "Closing " + this);
            mIsOpen = false;
            mStatus = STATUS_READY;
            close(context);
            closePorts();
        }
    }

    synchronized final boolean canProcess() {
        Log.v("Filter", "Checking if can process: " + this + ".");
        if (mStatus <= STATUS_PROCESSING) {
            return inputConditionsMet() && outputConditionsMet();
        } else {
            return false;
        }
    }

    final void beginProcessing() {
        openOutputs();
        if (mStatus >= STATUS_PROCESSING) {
            mStatus = STATUS_READY;
        }
    }

    final void notifyFieldPortValueUpdated(String name, FilterContext context) {
        if (mStatus == STATUS_PROCESSING) {
            fieldPortValueUpdated(name, context);
        }
    }

    // Filter internal methods /////////////////////////////////////////////////////////////////////
    private final void initFinalPorts(KeyValueMap values) {
        mInputPorts = new HashMap<String, TargetPort>();
        mOutputPorts = new HashMap<String, SourcePort>();
        addAndSetFinalPorts(values);
    }

    private final void initRemainingPorts(KeyValueMap values) {
        addAnnotatedPorts();
        setupPorts();   // TODO: rename to addFilterPorts() ?
        setInitialInputValues(values);
    }

    private final void addAndSetFinalPorts(KeyValueMap values) {
        Class filterClass = getClass();
        Annotation annotation;
        for (Field field : filterClass.getDeclaredFields()) {
            if ((annotation = field.getAnnotation(GenerateFinalPort.class)) != null) {
                GenerateFinalPort generator = (GenerateFinalPort)annotation;
                String name = generator.name().isEmpty() ? field.getName() : generator.name();
                boolean hasDefault = generator.hasDefault();
                addFieldPort(name, field, hasDefault, true);
                if (values.containsKey(name)) {
                    setImmediateInputValue(name, values.get(name));
                    values.remove(name);
                } else if (!generator.hasDefault()) {
                    throw new RuntimeException("No value specified for final input port '"
                        + name + "' of filter " + this + "!");
                }
            }
        }
    }

    private final void addAnnotatedPorts() {
        Class filterClass = getClass();
        Annotation annotation;
        for (Field field : filterClass.getDeclaredFields()) {
            if ((annotation = field.getAnnotation(GenerateFieldPort.class)) != null) {
                GenerateFieldPort generator = (GenerateFieldPort)annotation;
                addFieldGenerator(generator, field);
            } else if ((annotation = field.getAnnotation(GenerateProgramPort.class)) != null) {
                GenerateProgramPort generator = (GenerateProgramPort)annotation;
                addProgramGenerator(generator, field);
            } else if ((annotation = field.getAnnotation(GenerateProgramPorts.class)) != null) {
                GenerateProgramPorts generators = (GenerateProgramPorts)annotation;
                for (GenerateProgramPort generator : generators.value()) {
                    addProgramGenerator(generator, field);
                }
            }
        }
    }

    private final void addFieldGenerator(GenerateFieldPort generator, Field field) {
        String name = generator.name().isEmpty() ? field.getName() : generator.name();
        boolean hasDefault = generator.hasDefault();
        addFieldPort(name, field, hasDefault, false);
    }

    private final void addProgramGenerator(GenerateProgramPort generator, Field field) {
        String name = generator.name();
        String varName = generator.variableName().isEmpty() ? name
                                                            : generator.variableName();
        Class varType = generator.type();
        boolean hasDefault = generator.hasDefault();
        addProgramPort(name, varName, field, varType, hasDefault);
    }

    private final void setInitialInputValues(KeyValueMap values) {
        for (Entry<String, Object> entry : values.entrySet()) {
            setInputValue(entry.getKey(), entry.getValue());
        }
    }

    private final void setImmediateInputValue(String name, Object value) {
        Log.v("Filter", "Setting immediate value " + value + " for port " + name + "!");
        FilterPort port = getInputPort(name);
        port.open();
        port.setFrame(JavaFrame.wrapObject(value, null));
    }

    private final void transferInputFrames(FilterContext context) {
        for (TargetPort inputPort : mInputPorts.values()) {
            inputPort.transfer(context);
        }
    }

    private final void releasePulledFrames(FilterContext context) {
        for (Frame frame : mFramesToRelease) {
            context.getFrameManager().releaseFrame(frame);
        }
        mFramesToRelease.clear();
    }

    private final boolean inputConditionsMet() {
        for (FilterPort port : mInputPorts.values()) {
            if (!port.isReady()) {
                Log.v("Filter", "Input condition not met: " + port + "!");
                return false;
            }
        }
        return true;
    }

    private final boolean outputConditionsMet() {
        for (FilterPort port : mOutputPorts.values()) {
            if (!port.isReady()) {
                Log.v("Filter", "Output condition not met: " + port + "!");
                return false;
            }
        }
        return true;
    }

    private final void openOutputs() {
        Log.v("Filter", "Opening all output ports on " + this + "!");
        for (SourcePort outputPort : mOutputPorts.values()) {
            if (!outputPort.isOpen()) {
                outputPort.open();
            }
        }
    }

    private final void closePorts() {
        Log.v("Filter", "Closing all ports on " + this + "!");
        for (TargetPort inputPort : mInputPorts.values()) {
            inputPort.close();
        }
        for (SourcePort outputPort : mOutputPorts.values()) {
            outputPort.close();
        }
    }

    private final boolean filterMustClose() {
        for (TargetPort inputPort : mInputPorts.values()) {
            if (inputPort.filterMustClose()) {
                Log.v("Filter", "Filter " + this + " must close due to port " + inputPort);
                return true;
            }
        }
        for (SourcePort outputPort : mOutputPorts.values()) {
            if (outputPort.filterMustClose()) {
                Log.v("Filter", "Filter " + this + " must close due to port " + outputPort);
                return true;
            }
        }
        return false;
    }
}

