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

import android.os.AsyncTask;
import android.os.Handler;

import android.util.Log;

import java.lang.InterruptedException;
import java.lang.Runnable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.TimeUnit;

/**
 * @hide
 */
public class AsyncRunner extends GraphRunner{

    private Class mSchedulerClass;
    private SyncRunner mRunner;
    private AsyncRunnerTask mRunTask;

    private OnRunnerDoneListener mDoneListener;
    private boolean isProcessing;

    private class AsyncRunnerTask extends AsyncTask<SyncRunner, Void, Integer> {

        private static final String TAG = "AsyncRunnerTask";

        @Override
        protected Integer doInBackground(SyncRunner... runner) {
            if (runner.length > 1) {
                throw new RuntimeException("More than one runner received!");
            }

            runner[0].assertReadyToStep();

            // Preparation
            if (mLogVerbose) Log.v(TAG, "Starting background graph processing.");
            activateGlContext();

            if (mLogVerbose) Log.v(TAG, "Preparing filter graph for processing.");
            runner[0].beginProcessing();

            if (mLogVerbose) Log.v(TAG, "Running graph.");

            // Run loop
            int status = RESULT_RUNNING;
            while (!isCancelled() && status == RESULT_RUNNING) {
                if (!runner[0].performStep()) {
                    status = runner[0].determinePostRunState();
                    if (status == GraphRunner.RESULT_SLEEPING) {
                        runner[0].waitUntilWake();
                        status = RESULT_RUNNING;
                    }
                }
            }

            // Cleanup
            if (isCancelled()) {
                status = RESULT_STOPPED;
            }

            deactivateGlContext();

            if (mLogVerbose) Log.v(TAG, "Done with background graph processing.");
            return status;
        }

        @Override
        protected void onCancelled(Integer result) {
            onPostExecute(result);
        }

        @Override
        protected void onPostExecute(Integer result) {
            if (mLogVerbose) Log.v(TAG, "Starting post-execute.");
            setRunning(false);

            if (result == RESULT_STOPPED) {
                if (mLogVerbose) Log.v(TAG, "Closing filters.");
                mRunner.close();
            }
            if (mDoneListener != null) {
                if (mLogVerbose) Log.v(TAG, "Calling graph done callback.");
                mDoneListener.onRunnerDone(result);
            }
            if (mLogVerbose) Log.v(TAG, "Completed post-execute.");
        }
    }

    private boolean mLogVerbose;
    private static final String TAG = "AsyncRunner";

    /** Create a new asynchronous graph runner with the given filter
     * context, and the given scheduler class.
     *
     * Must be created on the UI thread.
     */
    public AsyncRunner(FilterContext context, Class schedulerClass) {
        super(context);

        mSchedulerClass = schedulerClass;
        mLogVerbose = Log.isLoggable(TAG, Log.VERBOSE);
    }

    /** Create a new asynchronous graph runner with the given filter
     * context. Uses a default scheduler.
     *
     * Must be created on the UI thread.
     */
    public AsyncRunner(FilterContext context) {
        super(context);

        mSchedulerClass = SimpleScheduler.class;
        mLogVerbose = Log.isLoggable(TAG, Log.VERBOSE);
    }

    /** Set a callback to be called in the UI thread once the AsyncRunner
     * completes running a graph, whether the completion is due to a stop() call
     * or the filters running out of data to process.
     */
    @Override
    public void setDoneCallback(OnRunnerDoneListener listener) {
        mDoneListener = listener;
    }

    /** Sets the graph to be run. Will call prepare() on graph. Cannot be called
     * when a graph is already running.
     */
    public void setGraph(FilterGraph graph) {
        if (mRunTask != null &&
            mRunTask.getStatus() != AsyncTask.Status.FINISHED) {
            throw new RuntimeException("Graph is already running!");
        }
        mRunner = new SyncRunner(mFilterContext, graph, mSchedulerClass);
    }

    @Override
    public FilterGraph getGraph() {
        return mRunner != null ? mRunner.getGraph() : null;
    }

    /** Execute the graph in a background thread. */
    @Override
    synchronized public void run() {
        if (mLogVerbose) Log.v(TAG, "Running graph.");
        if (isRunning()) {
            throw new RuntimeException("Graph is already running!");
        }
        if (mRunner == null) {
            throw new RuntimeException("Cannot run before a graph is set!");
        }
        mRunTask = this.new AsyncRunnerTask();

        setRunning(true);
        mRunTask.execute(mRunner);
    }

    /** Stop graph execution. This is an asynchronous call; register a callback
     * with setDoneCallback to be notified of when the background processing has
     * been completed. Calling stop will close the filter graph. */
    @Override
    synchronized public void stop() {
        if (mRunTask != null && !mRunTask.isCancelled() ) {
            if (mLogVerbose) Log.v(TAG, "Stopping graph.");
            mRunTask.cancel(false);
        }
    }

    @Override
    synchronized public void close() {
        if (isRunning()) {
            throw new RuntimeException("Cannot close graph while it is running!");
        }
        if (mLogVerbose) Log.v(TAG, "Closing filters.");
        mRunner.close();
    }

    /** Check if background processing is happening */
    @Override
    synchronized public boolean isRunning() {
        return isProcessing;
    }

    synchronized private void setRunning(boolean running) {
        isProcessing = running;
    }

}
