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

import android.filterfw.core.Filter;
import android.util.Log;

import java.lang.reflect.Constructor;
import java.util.HashSet;

/**
 * @hide
 */
public class FilterFactory {

    private static FilterFactory mSharedFactory;
    private HashSet<String> mPackages = new HashSet<String>();

    public static FilterFactory sharedFactory() {
        if (mSharedFactory == null) {
            mSharedFactory = new FilterFactory();
        }
        return mSharedFactory;
    }

    public void addPackage(String packageName) {
        Log.i("FilterFactory", "Adding package " + packageName);
        /*
        Package pkg = Package.getPackage(packageName);
        if (pkg == null) {
            throw new IllegalArgumentException("Unknown filter package '" + packageName + "'!");
        }
        */
        mPackages.add(packageName);
    }

    public Filter createFilterByClassName(String className, String filterName) {
        Log.i("FilterFactory", "Looking up class " + className);
        Class filterClass = null;
        // Look for the class in the imported packages
        for (String packageName : mPackages) {
            try {
                Log.i("FilterFactory", "Trying "+packageName + "." + className);
                filterClass = Class.forName(packageName + "." + className);
            } catch (ClassNotFoundException e) {
                continue;
            }
            // Exit loop if class was found.
            if (filterClass != null) {
                break;
            }
        }
        if (filterClass == null) {
            throw new IllegalArgumentException("Unknown filter class '" + className + "'!");
        }
        return createFilterByClass(filterClass, filterName);
    }

    public Filter createFilterByClass(Class filterClass, String filterName) {
        // Make sure this is a Filter subclass
        try {
            filterClass.asSubclass(Filter.class);
        } catch (ClassCastException e) {
            throw new IllegalArgumentException("Attempting to allocate class '" + filterClass
                + "' which is not a subclass of Filter!");
        }

        // Look for the correct constructor
        Constructor filterConstructor = null;
        try {
            filterConstructor = filterClass.getConstructor(String.class);
        } catch (NoSuchMethodException e) {
            throw new IllegalArgumentException("The filter class '" + filterClass
                + "' does not have a constructor of the form <init>(String name)!");
        }

        // Construct the filter
        Filter filter = null;
        try {
            filter = (Filter)filterConstructor.newInstance(filterName);
        } catch (Throwable t) {
            // Condition checked below
        }

        if (filter == null) {
            throw new IllegalArgumentException("Could not construct the filter '"
                + filterName + "'!");
        }
        return filter;
    }
}
