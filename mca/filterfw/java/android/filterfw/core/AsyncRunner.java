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

import android.util.Log;

import java.lang.InterruptedException;
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

    private FilterContext.OnFrameReceivedListener mUiListener;
    private OnRunnerDoneListener mDoneListener;

    private FilterContext.OnFrameReceivedListener mBackgroundListener =
            new FilterContext.OnFrameReceivedListener() {
        @Override
        public void onFrameReceived(Filter filter, Frame frame, Object userData) {
            if (mLogVerbose) Log.v(TAG, "Received callback at forwarding listener.");
            if (mRunTask != null) {
                mRunTask.onFrameReceived(filter, frame, userData);
            }
        }
    };

    private class FrameReceivedContents {
        public Filter filter;
        public Frame frame;
        public Object userData;
    }

    private class AsyncRunnerTask extends AsyncTask<SyncRunner, FrameReceivedContents, Integer> {

        private static final String TAG = "AsyncRunnerTask";

        @Override
        protected Integer doInBackground(SyncRunner... runner) {
            if (runner.length > 1) {
                throw new RuntimeException("More than one callback data set received!");
            }

            // Preparation
            if (mLogVerbose) Log.v(TAG, "Starting background graph processing.");
            activateGlContext();

            if (mLogVerbose) Log.v(TAG, "Preparing filter graph for processing.");
            runner[0].beginProcessing();

            if (mLogVerbose) Log.v(TAG, "Running graph.");
            runner[0].assertReadyToStep();

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
                if (mLogVerbose) Log.v(TAG, "Closing filters.");
                runner[0].close();
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
            if (mDoneListener != null) {
                if (mLogVerbose) Log.v(TAG, "Invoking graph done callback.");
                mDoneListener.onRunnerDone(result);
            }
        }

        /** Runs in async thread to receive filter callbacks */
        public void onFrameReceived(Filter filter, Frame frame, Object userData) {
            if (mUiListener != null) {
                FrameReceivedContents publishPacket = new FrameReceivedContents();
                publishPacket.filter = filter;
                publishPacket.frame = frame;
                publishPacket.userData = userData;
                // Sending frame across thread boundary, need to explicitly retain
                publishPacket.frame.retain();
                publishProgress(publishPacket);
            }
        }

        /** Runs in UI thread to pass on frame callbacks */
        @Override
        public void onProgressUpdate(FrameReceivedContents... publishPacket) {
            if (publishPacket.length > 1) {
                throw new RuntimeException("More than one callback data set received!");
            }
            if (mUiListener != null) {
                mUiListener.onFrameReceived(publishPacket[0].filter,
                                            publishPacket[0].frame,
                                            publishPacket[0].userData);
            }
            // Release frame now that we're done with it in UI thread.
            publishPacket[0].frame.release();
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
    public void run() {
        if (mLogVerbose) Log.v(TAG, "Running graph.");
        if (mRunTask != null &&
            mRunTask.getStatus() != AsyncTask.Status.FINISHED) {
            throw new RuntimeException("Graph is already running!");
        }
        if (mRunner == null) {
            throw new RuntimeException("Cannot run before a graph is set!");
        }
        mRunTask = this.new AsyncRunnerTask();

        mRunTask.execute(mRunner);
    }

    /** Stop graph execution. This is an asynchronous call; register a callback
     * with setDoneCallback to be notified of when the background processing has
     * been completed. */
    @Override
    public void stop() {
        if (mLogVerbose) Log.v(TAG, "Stopping graph.");
        if (mRunTask != null) {
            mRunTask.cancel(false);
        }
    }

    /** Check if background processing is happening */
    @Override
    public boolean isRunning() {
        if (mRunTask == null) return false;
        else return (mRunTask.getStatus() != AsyncTask.Status.FINISHED);
    }

    /** Get the forwarding listener to use for frame-producing callbacks such as
     * the one from CallbackFilter. This listener will forward callbacks to the
     * UI thread and call the callback set by
     * {@link #setUiThreadCallback}. The listener returned by this method
     * should only be invoked from within the graph running in this
     * AsyncRunner. */
    public FilterContext.OnFrameReceivedListener getForwardingCallback() {
        return mBackgroundListener;
    }

    /** Set the UI thread listener for graph callbacks. This callback will be
     * called in the UI thread when the forwarding listener given by
     * {@link #getForwardingCallback} is called by a filter such as a
     * CallbackFilter. If the frame needs to be kept around by the UI thread,
     * make sure to call retain() on it before the callback exists, and then
     * call release() when done with it. */
    public void setUiThreadCallback(FilterContext.OnFrameReceivedListener uiListener) {
        mUiListener = uiListener;
    }

}
