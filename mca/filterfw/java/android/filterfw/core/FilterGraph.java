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

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Map.Entry;
import java.util.Set;
import java.util.Stack;

import android.filterfw.core.FilterEnvironment;
import android.filterfw.core.FilterPort;
import android.filterfw.core.KeyValueMap;
import android.filterpacks.base.FrameBranch;
import android.filterpacks.base.NullFilter;

import android.util.Log;

public class FilterGraph {

    private HashSet<Filter> mFilters = new HashSet<Filter>();
    private HashSet<Filter> mSourceFilters = new HashSet<Filter>();
    private HashMap<String, Filter> mNameMap = new HashMap<String, Filter>();
    private HashMap<FilterPort, LinkedList<FilterPort>> mPreconnections = new
            HashMap<FilterPort, LinkedList<FilterPort>>();

    public static final int AUTOBRANCH_OFF      = 0;
    public static final int AUTOBRANCH_SYNCED   = 1;
    public static final int AUTOBRANCH_UNSYNCED = 2;

    private boolean mIsOpen = false;
    private int mAutoBranchMode = AUTOBRANCH_OFF;
    private boolean mDiscardUnconnectedFilters = false;
    private boolean mDiscardUnconnectedOutputs = false;

    public FilterGraph() {
    }

    public boolean addFilter(Filter filter) {
        if (!containsFilter(filter)) {
            mFilters.add(filter);
            mNameMap.put(filter.getName(), filter);
            if (filter.getNumberOfInputs() == 0) {
                mSourceFilters.add(filter);
            }
            return true;
        }
        return false;
    }

    public boolean containsFilter(Filter filter) {
        return mFilters.contains(filter);
    }

    public Filter getFilter(String name) {
        return mNameMap.get(name);
    }

    public void connect(Filter source,
                        String outputName,
                        Filter target,
                        String inputName) {
        if (source == null || target == null) {
            throw new IllegalArgumentException("Passing null Filter in connect()!");
        } else if (!containsFilter(source) || !containsFilter(target)) {
            throw new RuntimeException("Attempting to connect filter not in graph!");
        }

        FilterPort outPort = source.getOutputPort(outputName);
        FilterPort inPort = target.getInputPort(inputName);
        if (outPort == null) {
            throw new RuntimeException("Unknown output port '" + outputName + "' on Filter " +
                                       source + "!");
        } else if (inPort == null) {
            throw new RuntimeException("Unknown input port '" + inputName + "' on Filter " +
                                       target + "!");
        }

        preconnect(outPort, inPort);
    }

    public void connect(String sourceName,
                        String outputName,
                        String targetName,
                        String inputName) {
        Filter source = getFilter(sourceName);
        Filter target = getFilter(targetName);
        if (source == null) {
            throw new RuntimeException(
                "Attempting to connect unknown source filter '" + sourceName + "'!");
        } else if (target == null) {
            throw new RuntimeException(
                "Attempting to connect unknown target filter '" + targetName + "'!");
        }
        connect(source, outputName, target, inputName);
    }

    public Set<Filter> getFilters() {
        return mFilters;
    }

    public Set<Filter> getSourceFilters() {
        return mSourceFilters;
    }

    public void openFilters(FilterEnvironment env) {
        for (Filter filter : mFilters) {
            if (!filter.performOpen(env)) {
                throw new RuntimeException("Failed to open filter " + filter + "!");
            }
        }
        mIsOpen = true;
    }

    public void closeFilters(FilterEnvironment env) {
        for (Filter filter : mFilters) {
            filter.performClose(env);
        }
        mIsOpen = false;
    }

    public boolean isOpen() {
        return mIsOpen;
    }

    public void setAutoBranchMode(int autoBranchMode) {
        mAutoBranchMode = autoBranchMode;
    }

    public void setDiscardUnconnectedFilters(boolean discard) {
        mDiscardUnconnectedFilters = discard;
    }

    public void setDiscardUnconnectedOutputs(boolean discard) {
        mDiscardUnconnectedOutputs = discard;
    }

    private void setupPorts() {
        Stack<Filter> filterStack = new Stack<Filter>();
        Set<Filter> processedFilters = new HashSet<Filter>();
        filterStack.addAll(getSourceFilters());

        while (!filterStack.empty()) {
            // Get current filter and mark as processed
            Filter filter = filterStack.pop();
            processedFilters.add(filter);

            // Setup ports
            setupInputPortsFor(filter);
            setupOutputPortsFor(filter);

            // Push connected filters onto stack
            for (int i = 0; i < filter.getNumberOfOutputs(); ++i) {
                Filter target = filter.getOutputPortAtIndex(i).getTargetFilter();
                if (canSetupPortsFor(target, processedFilters)) {
                    filterStack.push(target);
                }
            }
        }

        // Make sure all ports were setup
        if (processedFilters.size() != getFilters().size()) {
            throw new RuntimeException("Could not schedule all filters! Is your graph malformed?");
        }
    }

    private boolean canSetupPortsFor(Filter filter, Set<Filter> processed) {
        // Check if this has been already processed
        if (processed.contains(filter)) {
            return false;
        }

        // Check if all dependencies have been processed
        for (int i = 0; i < filter.getNumberOfInputs(); ++i) {
            Filter dep = filter.getInputPortAtIndex(i).getSourceFilter();
            if (!processed.contains(dep)) {
                return false;
            }
        }
        return true;
    }

    private void setupInputPortsFor(Filter filter) {
        for (int i = 0; i < filter.getNumberOfInputs(); ++i) {
            if (!filter.setInputFormat(i, filter.getInputPortAtIndex(i).getFormat())) {
                throw new RuntimeException("Filter " + filter + " does not accept the " +
                                           "input given on port " + i + "!");
            }
        }
    }

    private void setupOutputPortsFor(Filter filter) {
        for (int i = 0; i < filter.getNumberOfOutputs(); ++i) {
            FrameFormat format = filter.getFormatForOutput(i);
            if (format == null) {
                throw new RuntimeException("Filter " + filter + " did not produce an " +
                                           "output format for port " + i + "!");
            }
            filter.getOutputPortAtIndex(i).setFormat(format);
        }
    }

    private void checkConnections() {
        // TODO
    }

    private void prepareFilters(FilterEnvironment env) {
      for (Filter filter : mFilters) {
        filter.prepare(env);
      }
    }

    private void discardUnconnectedFilters() {
        // First, find all filters that are not fully connected and add them to the remove list.
        Stack<Filter> filtersToRemove = new Stack<Filter>();
        for (Filter filter : mFilters) {
            if (!filter.allInputsConnected() || !filter.allOutputsConnected()) {
                filtersToRemove.push(filter);
            }
        }

        // Recursively remove the filters in the list and all filters connected
        while (!filtersToRemove.empty()) {
            Filter filter = filtersToRemove.pop();
            if (mFilters.contains(filter)) {
                for (Filter connected : filter.connectedFilters()) {
                    filtersToRemove.push(connected);
                }
                Log.v("FilterGraph", "Discarding unconnected filter " + filter + "!");
                removeFilter(filter);
            }
        }

        if (mFilters.isEmpty()) {
            throw new RuntimeException("Discarding unconnected filters caused all filters to be "
                + "removed!");
        }
    }

    private void discardUnconnectedOutputs() {
        // Connect unconnected ports to Null filters
        LinkedList<Filter> addedFilters = new LinkedList<Filter>();
        for (Filter filter : mFilters) {
            for (int i = 0; i < filter.getNumberOfOutputs(); ++i) {
                FilterPort port = filter.getOutputPortAtIndex(i);
                if (!port.isConnected()) {
                    Log.v("FilterGraph", "Autoconnecting unconnected output port " + i + " of "
                        + "filter " + filter + " to Null filter.");
                    NullFilter nullFilter = new NullFilter(filter.getName() + "ToNull" + i);
                    nullFilter.init();
                    addedFilters.add(nullFilter);
                    port.connectTo(nullFilter.getInputPort("frame"));
                }
            }
        }
        // Add all added filters to this graph
        for (Filter filter : addedFilters) {
            addFilter(filter);
        }
    }

    private void removeFilter(Filter filter) {
        mFilters.remove(filter);
        mSourceFilters.remove(filter);
        mNameMap.remove(filter.getName());
    }

    private void preconnect(FilterPort outPort, FilterPort inPort) {
        LinkedList<FilterPort> targets;
        targets = mPreconnections.get(outPort);
        if (targets == null) {
            targets = new LinkedList<FilterPort>();
            mPreconnections.put(outPort, targets);
        }
        targets.add(inPort);
    }

    private void connectPorts() {
        int branchId = 1;
        for (Entry<FilterPort, LinkedList<FilterPort>> connection : mPreconnections.entrySet()) {
            FilterPort sourcePort = connection.getKey();
            LinkedList<FilterPort> targetPorts = connection.getValue();
            if (targetPorts.size() == 1) {
                sourcePort.connectTo(targetPorts.get(0));
            } else if (mAutoBranchMode == AUTOBRANCH_OFF) {
                throw new RuntimeException("Attempting to connect " + sourcePort + " to multiple "
                                         + "filter ports! Enable auto-branching to allow this.");
            } else {
                Log.v("FilterGraph", "Creating branch for " + sourcePort + "!");
                FrameBranch branch = null;
                if (mAutoBranchMode == AUTOBRANCH_SYNCED) {
                    branch = new FrameBranch("branch" + branchId++);
                } else {
                    throw new RuntimeException("TODO: Unsynced branches not implemented yet!");
                }
                KeyValueMap branchParams = new KeyValueMap();
                branch.initWithParameterList("outputs", targetPorts.size());
                addFilter(branch);
                sourcePort.connectTo(branch.getInputPortAtIndex(0));
                int portIndex = 0;
                for (FilterPort targetPort : targetPorts) {
                    branch.getOutputPortAtIndex(portIndex++).connectTo(targetPort);
                }
            }
        }
        mPreconnections.clear();
    }

    // Core internal methods /////////////////////////////////////////////////////////////////////////
    void setupFilters(FilterEnvironment env) {
        if (mDiscardUnconnectedFilters) {
            discardUnconnectedFilters();
        }
        if (mDiscardUnconnectedOutputs) {
            discardUnconnectedOutputs();
        }
        connectPorts();
        checkConnections();
        setupPorts();
        prepareFilters(env);
    }

    void tearDownFilters(FilterEnvironment env) {
        for (Filter filter : mFilters) {
            filter.tearDown(env);
        }
    }
}
