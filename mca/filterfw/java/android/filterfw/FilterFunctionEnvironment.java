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

import android.filterfw.core.AsyncRunner;
import android.filterfw.core.CachedFrameManager;
import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.FilterFactory;
import android.filterfw.core.FilterFunction;
import android.filterfw.core.FrameHandle;
import android.filterfw.core.FrameManager;
import android.filterfw.core.GLEnvironment;

public class FilterFunctionEnvironment {

    private FilterContext mFilterContext;

    public FilterFunctionEnvironment() {
        init(null, null);
    }

    public FilterFunctionEnvironment(FrameManager frameManager, GLEnvironment glEnvironment) {
        init(frameManager, glEnvironment);
    }

    public FilterContext getContext() {
        return mFilterContext;
    }

    public FilterFunction createFunction(Class filterClass, Object... parameters) {
        String filterName = "FilterFunction(" + filterClass.getSimpleName() + ")";
        Filter filter = FilterFactory.sharedFactory().createFilterByClass(filterClass, filterName);
        filter.initWithParameterList(parameters);
        return new FilterFunction(mFilterContext, filter);
    }

    public FrameHandle executeSequence(FilterFunction[] functions) {
        FrameHandle oldFrame = null;
        FrameHandle newFrame = null;
        for (FilterFunction filterFunction : functions) {
            if (oldFrame == null) {
                newFrame = filterFunction.execute();
            } else {
                newFrame = filterFunction.execute(oldFrame);
                oldFrame.release();
            }
            oldFrame = newFrame;
        }
        if (oldFrame != null) {
            oldFrame.release();
        }
        return newFrame;
    }

    private void init(FrameManager frameManager, GLEnvironment glEnvironment) {
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

        // Setup the context
        mFilterContext = new FilterContext();
        mFilterContext.setFrameManager(frameManager);
        mFilterContext.setGLEnvironment(glEnvironment);
    }

}
