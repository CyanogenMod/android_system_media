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

import android.os.ConditionVariable;

import java.lang.reflect.Constructor;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class SyncRunner extends GraphRunner {

    private Scheduler mScheduler = null;
    private FilterContext mFilterContext = null;

    private OnRunnerDoneListener mDoneListener = null;
    private ScheduledThreadPoolExecutor mWakeExecutor = new ScheduledThreadPoolExecutor(1);
    private ConditionVariable mWakeCondition = new ConditionVariable();

    private StopWatchMap mTimer = null;


    // TODO: Provide factory based constructor?
    public SyncRunner(FilterContext context, FilterGraph graph, Class schedulerClass) {
        // Create the scheduler
        if (Scheduler.class.isAssignableFrom(schedulerClass)) {
            try {
                Constructor schedulerConstructor = schedulerClass.getConstructor(FilterGraph.class);
                mScheduler = (Scheduler)schedulerConstructor.newInstance(graph);
            } catch (NoSuchMethodException e) {
                throw new RuntimeException("Scheduler does not have constructor <init>(FilterGraph)!");
            } catch (InstantiationException e) {
                throw new RuntimeException("Could not instantiate the Scheduler instance!");
            } catch (IllegalAccessException e) {
                throw new RuntimeException("Cannot access Scheduler constructor!");
            } catch (Exception e) {
                throw new RuntimeException("Could not instantiate Scheduler: " + e.getMessage());
            }
        } else {
            throw new IllegalArgumentException("Class provided is not a Scheduler subclass!");
        }

        // Add us to the specified context
        mFilterContext = context;

        mTimer = new StopWatchMap();

        // Setup graph filters
        graph.setupFilters(mFilterContext);
    }

    @Override
    protected void finalize() throws Throwable {
        // TODO: Is this the best place to do this, or should we make it explicit?
        getGraph().tearDownFilters(mFilterContext);
    }

    @Override
    public FilterGraph getGraph() {
        return mScheduler != null ? mScheduler.getGraph() : null;
    }

    @Override
    public FilterContext getContext() {
        return mFilterContext;
    }

    public void open() {
        if (!getGraph().isOpen()) {
            getGraph().openFilters(mFilterContext);
        }
    }

    public int step() {
        assertReadyToStep();
        return performStep(true);
    }

    public void close() {
        // Close filters
        getGraph().closeFilters(mFilterContext);
    }

    @Override
    public void run() {
        // Open filters
        open();

        // Make sure we are ready to run
        assertReadyToStep();

        // Run
        int status = RESULT_RUNNING;
        while (status == RESULT_RUNNING) {
            status = performStep(false);
        }
        close();

        // Call completion callback if set
        if (mDoneListener != null) {
            mDoneListener.onRunnerDone(status);
        }
    }


    @Override
    public boolean isRunning() {
        return false;
    }

    @Override
    public void setDoneCallback(OnRunnerDoneListener listener) {
        mDoneListener = listener;
    }

    @Override
    public void stop() {
        throw new RuntimeException("SyncRunner does not support stopping a graph!");
    }

    public int getGraphState() {
        return determineGraphState();
    }

    public void waitUntilWake() {
        mWakeCondition.block();
    }

    private void processFilterNode(Filter filter) {
        LogMCA.perFrame("Processing " + filter);
        filter.performProcess(mFilterContext);
        if (filter.checkStatus(Filter.STATUS_ERROR)) {
            throw new RuntimeException("There was an error executing " + filter + "!");
        } else if (filter.checkStatus(Filter.STATUS_SLEEP)) {
            scheduleFilterWake(filter, filter.getSleepDelay());
        }
    }

    private void scheduleFilterWake(Filter filter, int delay) {
        // Close the wake condition
        mWakeCondition.close();

        // Schedule the wake-up
        final Filter filterToSchedule = filter;
        final ConditionVariable conditionToWake = mWakeCondition;

        mWakeExecutor.schedule(new Runnable() {
          @Override 
          public void run() {
                filterToSchedule.unsetStatus(Filter.STATUS_SLEEP);
                conditionToWake.open();
            }
        }, delay, TimeUnit.MILLISECONDS);
    }

    private int determineGraphState() {
        boolean isBlocked = false;
        for (Filter filter : mScheduler.getGraph().getFilters()) {
            if (filter.checkStatus(Filter.STATUS_SLEEP)) {
                // If ANY node is sleeping, we return our state as sleeping
                return RESULT_SLEEPING;
            } else if (filter.checkStatus(Filter.STATUS_WAIT_FOR_ALL_INPUTS) ||
                       filter.checkStatus(Filter.STATUS_WAIT_FOR_ONE_INPUT)) {
                // Currently, we ignore waiting-for-input nodes
                continue;
            } else if (filter.checkStatus(Filter.STATUS_WAIT_FOR_FREE_OUTPUTS) ||
                       filter.checkStatus(Filter.STATUS_WAIT_FOR_FREE_OUTPUT)) {
                // If a node is blocked on output, we will report it if no node is sleeping.
                isBlocked = true;
            }
        }
        return isBlocked ? RESULT_BLOCKED : RESULT_FINISHED;
    }

    // Core internal methods ///////////////////////////////////////////////////////////////////////
    int performStep(boolean detailedState) {
        Filter filter = mScheduler.scheduleNextNode();
        if (filter != null) {
            mTimer.start(filter.getName());
            processFilterNode(filter);
            mTimer.stop(filter.getName());
            return RESULT_RUNNING;
        } else {
            return detailedState ? determineGraphState() : RESULT_FINISHED;
        }
    }

    void assertReadyToStep() {
        if (mScheduler == null) {
            throw new RuntimeException("Attempting to run schedule with no scheduler in place!");
        } else if (getGraph() == null) {
            throw new RuntimeException("Calling step on scheduler with no graph in place!");
        } else if (!getGraph().isOpen() ) {
            throw new RuntimeException("Trying to process graph that is not open!");
        }
    }
}
