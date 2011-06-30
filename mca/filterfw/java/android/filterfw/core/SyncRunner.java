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

/**
 * @hide
 */
public class SyncRunner extends GraphRunner {

    private Scheduler mScheduler = null;

    private OnRunnerDoneListener mDoneListener = null;
    private ScheduledThreadPoolExecutor mWakeExecutor = new ScheduledThreadPoolExecutor(1);
    private ConditionVariable mWakeCondition = new ConditionVariable();

    private StopWatchMap mTimer = null;


    // TODO: Provide factory based constructor?
    public SyncRunner(FilterContext context, FilterGraph graph, Class schedulerClass) {
        super(context);

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
        graph.setupFilters();
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

    public int step() {
        assertReadyToStep();
        return performStep(true);
    }

    public void beginProcessing() {
        getGraph().beginProcessing();
    }

    public void close() {
        // Close filters
        getGraph().closeFilters(mFilterContext);
    }

    @Override
    public void run() {
        // Preparation
        beginProcessing();
        assertReadyToStep();
        activateGlContext();

        // Run
        int status = RESULT_RUNNING;
        while (status == RESULT_RUNNING) {
            status = performStep(false);
        }

        // Close
        close();
        deactivateGlContext();

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
        if (filter.getStatus() == Filter.STATUS_ERROR) {
            throw new RuntimeException("There was an error executing " + filter + "!");
        } else if (filter.getStatus() == Filter.STATUS_SLEEPING) {
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
                filterToSchedule.unsetStatus(Filter.STATUS_SLEEPING);
                conditionToWake.open();
            }
        }, delay, TimeUnit.MILLISECONDS);
    }

    private int determineGraphState() {
        boolean isBlocked = false;
        for (Filter filter : mScheduler.getGraph().getFilters()) {
            if (filter.isOpen()) {
                if (filter.getStatus() == Filter.STATUS_SLEEPING) {
                    // If ANY node is sleeping, we return our state as sleeping
                    return RESULT_SLEEPING;
                } else {
                    // TODO: Be more precise whether this is blocked or starved
                    return RESULT_BLOCKED;
                }
            }
        }
        return RESULT_FINISHED;
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
        } else if (!getGraph().isReady() ) {
            throw new RuntimeException("Trying to process graph that is not open!");
        }
    }
}
