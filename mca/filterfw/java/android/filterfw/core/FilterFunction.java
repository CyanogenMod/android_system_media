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
import android.filterfw.core.FilterConnection;
import android.filterfw.core.FilterContext;
import android.filterfw.core.FrameHandle;

public class FilterFunction {

    private Filter mFilter;
    private FilterContext mFilterContext;
    private boolean mFilterSetup = false;
    private FilterConnection[] mInputStubs;
    private FilterConnection[] mOutputStubs;

    public FilterFunction(FilterContext context, Filter filter) {
        mFilterContext = context;
        mFilter = filter;
        setupConnectionStubs();
    }

    public FrameHandle execute(FrameHandle... inputs) {
        int filterInCount = mFilter.getNumberOfInputs();
        int filterOutCount = mFilter.getNumberOfOutputs();

        // Sanity checks
        if (inputs.length != filterInCount) {
            throw new RuntimeException("Illegal number of arguments passed to function for "
                + "filter " + mFilter + ": Expected " + mFilter.getNumberOfInputs() + " but got "
                + inputs.length + "!");
        } else if (filterOutCount > 1) {
            throw new RuntimeException("Calling execute on filter " + mFilter + " with multiple "
                + "outputs! Use executeMulti() instead!");
        }

        // Setup the filter if it has not been setup already
        if (!mFilterSetup) {
            setupFilterInputs(inputs);
            setupFilterOutputs();
            mFilter.prepare(mFilterContext);
            mFilterSetup = true;
        }

        // Setup the inputs
        for (int i = 0; i < filterInCount; ++i) {
            mInputStubs[i].putFrame(inputs[i].getFrame());
        }

        // Process the filter
        mFilter.performOpen(mFilterContext);
        mFilter.performProcess(mFilterContext);
        mFilter.performClose(mFilterContext);

        // Clear inputs
        for (int i = 0; i < filterInCount; ++i) {
            mInputStubs[i].clearFrame();
        }

        // Create result handle
        FrameHandle result = null;
        if (filterOutCount == 1) {
            result = new FrameHandle(mOutputStubs[0].getFrame());
            mOutputStubs[0].clearFrame();
        }
        return result;
    }

    public FilterContext getContext() {
        return mFilterContext;
    }

    public void updateParameters(Object... keyValues) {
        mFilter.updateParameterList(keyValues);
    }

    public Object getParameterValue(String key) {
        return mFilter.getParameters().get(key);
    }

    @Override
    public String toString() {
        return mFilter.getName();
    }

    private void setupConnectionStubs() {
        mInputStubs = new FilterConnection[mFilter.getNumberOfInputs()];
        mOutputStubs = new FilterConnection[mFilter.getNumberOfOutputs()];
        for (int i = 0; i < mFilter.getNumberOfInputs(); ++i) {
            mInputStubs[i] = new FilterConnection();
        }
        for (int i = 0; i < mFilter.getNumberOfOutputs(); ++i) {
            mOutputStubs[i] = new FilterConnection();
        }
    }

    private void setupFilterInputs(FrameHandle[] inputs) {
        // Set input formats (from input frames)
        for (int i = 0; i < inputs.length; ++i) {
            FrameFormat format = inputs[i].getFrame().getFormat();
            if (!mFilter.acceptsInputFormat(i, format)) {
                throw new RuntimeException("Filter " + mFilter + " does not accept the " +
                                           "input given on port " + i + "!");
            }

            // Connect input to connection stub
            mInputStubs[i].setFormat(format);
            mFilter.getInputPortAtIndex(i).setConnection(mInputStubs[i]);
        }
    }

    private void setupFilterOutputs() {
        // We are actually not interested in the filter output, as we will use the generated output
        // frames themselves to determine the format. Still, as the filter may run critical code
        // in the getOutputFormat() method, we call it here.
        for (int i = 0; i < mFilter.getNumberOfOutputs(); ++i) {
            mFilter.getOutputFormat(i);

            // Connect output to connection stub
            mFilter.getOutputPortAtIndex(i).setConnection(mOutputStubs[i]);
        }
    }
}
