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


package android.filterfw;

import android.content.Context;
import android.filterfw.core.AsyncRunner;
import android.filterfw.core.CachedFrameManager;
import android.filterfw.core.FilterGraph;
import android.filterfw.core.FilterContext;
import android.filterfw.core.FrameManager;
import android.filterfw.core.GLEnvironment;
import android.filterfw.core.GraphRunner;
import android.filterfw.core.SimpleScheduler;
import android.filterfw.core.SyncRunner;
import android.filterfw.io.GraphIOException;
import android.filterfw.io.GraphReader;
import android.filterfw.io.TextGraphReader;

import java.util.ArrayList;

public class GraphEnvironment {

    public static final int MODE_ASYNCHRONOUS = 1;
    public static final int MODE_SYNCHRONOUS  = 2;

    private FilterContext mContext;
    private GraphReader mGraphReader;
    private ArrayList<GraphHandle> mGraphs = new ArrayList<GraphHandle>();

    private class GraphHandle {
        private FilterGraph mGraph;
        private AsyncRunner mAsyncRunner;
        private SyncRunner mSyncRunner;

        public GraphHandle(FilterGraph graph) {
            mGraph = graph;
        }

        public FilterGraph getGraph() {
            return mGraph;
        }

        public AsyncRunner getAsyncRunner(FilterContext environment) {
            if (mAsyncRunner == null) {
                mAsyncRunner = new AsyncRunner(environment, SimpleScheduler.class);
                mAsyncRunner.setGraph(mGraph);
            }
            return mAsyncRunner;
        }

        public GraphRunner getSyncRunner(FilterContext environment) {
            if (mSyncRunner == null) {
                mSyncRunner = new SyncRunner(environment, mGraph, SimpleScheduler.class);
            }
            return mSyncRunner;
        }
    };

    public GraphEnvironment() {
        init(null, null, null);
    }

    public GraphEnvironment(FrameManager frameManager,
                            GLEnvironment glEnvironment,
                            GraphReader reader) {
        init(frameManager, glEnvironment, reader);
    }

    public void addReferences(Object... references) {
        mGraphReader.addReferencesByKeysAndValues(references);
    }

    public int loadGraph(Context context, int resourceId) {
        // Read the file into a graph
        FilterGraph graph = null;
        try {
            graph = mGraphReader.readResource(context, resourceId);
        } catch (GraphIOException e) {
            throw new RuntimeException("Could not read graph: " + e.getMessage());
        }

        // Add graph to our list of graphs
        return addGraph(graph);
    }

    public int addGraph(FilterGraph graph) {
        GraphHandle graphHandle = new GraphHandle(graph);
        mGraphs.add(graphHandle);
        return mGraphs.size() - 1;
    }

    public FilterGraph getGraph(int graphId) {
        if (graphId < 0 || graphId >= mGraphs.size()) {
            throw new RuntimeException("Invalid graph ID " + graphId + " specified in runGraph()!");
        }
        return mGraphs.get(graphId).getGraph();
    }

    public GraphRunner getRunner(int graphId, int executionMode) {
        switch (executionMode) {
            case MODE_ASYNCHRONOUS:
                return mGraphs.get(graphId).getAsyncRunner(mContext);

            case MODE_SYNCHRONOUS:
                return mGraphs.get(graphId).getSyncRunner(mContext);

            default:
                throw new RuntimeException(
                    "Invalid execution mode " + executionMode + " specified in getRunner()!");
        }
    }

    public FilterContext getContext() {
        return mContext;
    }

    public void activateGLEnvironment() {
        mContext.getGLEnvironment().activate();
    }

    public void deactivateGLEnvironment() {
        mContext.getGLEnvironment().deactivate();
    }

    private void init(FrameManager frameManager, GLEnvironment glEnvironment, GraphReader reader) {
        // Get or create the frame manager
        if (frameManager == null) {
            frameManager = new CachedFrameManager();
        }

        // Get or create the GL environment
        if (glEnvironment == null) {
            glEnvironment = new GLEnvironment();
            if (!glEnvironment.initWithNewContext()) {
                throw new RuntimeException("Could not init GL environment!");
            }
            glEnvironment.activate();
        }

        // Create new reader
        if (reader == null) {
            reader = new TextGraphReader();
        }
        mGraphReader = reader;

        // Setup the environment
        mContext = new FilterContext();
        mContext.setFrameManager(new CachedFrameManager());
        mContext.setGLEnvironment(glEnvironment);
    }

}
