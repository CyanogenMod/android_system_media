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

import android.filterfw.core.Frame;
import android.filterfw.core.Program;

import android.util.Log;

public class NativeProgram extends Program {

    private int nativeProgramId;
    private boolean mHasInitFunction     = false;
    private boolean mHasTeardownFunction = false;
    private boolean mHasSetValueFunction = false;
    private boolean mHasGetValueFunction = false;

    public NativeProgram(String nativeLibName, String nativeFunctionPrefix) {
        // Allocate the native instance
        allocate();

        // Open the native library
        String fullLibName = "lib" + nativeLibName + ".so";
        if (!openNativeLibrary(fullLibName)) {
            throw new RuntimeException("Could not find native library named '" + fullLibName + "' " +
                                       "required for native program!");
        }

        // Bind the native functions
        String processFuncName = nativeFunctionPrefix + "_process";
        if (!bindProcessFunction(processFuncName)) {
            throw new RuntimeException("Could not find native program function name " +
                                       processFuncName + " in library " + fullLibName + "! " +
                                       "This function is required!");
        }

        String initFuncName = nativeFunctionPrefix + "_init";
        mHasInitFunction = bindInitFunction(initFuncName);

        String teardownFuncName = nativeFunctionPrefix + "_teardown";
        mHasTeardownFunction = bindTeardownFunction(teardownFuncName);

        String setValueFuncName = nativeFunctionPrefix + "_setvalue";
        mHasSetValueFunction = bindSetValueFunction(setValueFuncName);

        String getValueFuncName = nativeFunctionPrefix + "_getvalue";
        mHasGetValueFunction = bindGetValueFunction(getValueFuncName);

        // Initialize the native code
        if (mHasInitFunction && !callNativeInit()) {
            throw new RuntimeException("Could not initialize NativeFrame!");
        }
    }

    protected void finalize() throws Throwable {
        if (mHasTeardownFunction && !callNativeTeardown()) {
            throw new RuntimeException("Could not tear down NativeFrame!");
        }
        deallocate();
    }

    public void process(Frame[] inputs, Frame output) {
        NativeFrame[] nativeInputs = new NativeFrame[inputs.length];
        for (int i = 0; i < inputs.length; ++i) {
            if (inputs[i] instanceof NativeFrame) {
                nativeInputs[i] = (NativeFrame)inputs[i];
            } else {
                throw new RuntimeException("NativeProgram got non-native frame as input "+ i +"!");
            }
        }

        // Get the GL output frame
        NativeFrame nativeOutput = null;
        if (output instanceof NativeFrame) {
            nativeOutput = (NativeFrame)output;
        } else {
            throw new RuntimeException("NativeProgram got non-native output frame!");
        }

        // Process!
        if (!callNativeProcess(nativeInputs, nativeOutput)) {
            throw new RuntimeException("Calling native process() caused error!");
        }
    }

    public void setHostValue(String variableName, Object value) {
        if (!mHasSetValueFunction) {
            throw new RuntimeException("Attempting to set native variable, but native code does not " +
                                       "define native setvalue function!");
        }
        if (!callNativeSetValue(variableName, value)) {
            throw new RuntimeException("Error setting native value for variable '" + variableName + "'!");
        }
    }

    public Object getHostValue(String variableName) {
        if (!mHasGetValueFunction) {
            throw new RuntimeException("Attempting to get native variable, but native code does not " +
                                       "define native getvalue function!");
        }
        return callNativeGetValue(variableName);
    }

    static {
        System.loadLibrary("filterfw2-jni");
    }

    private native boolean allocate();

    private native boolean deallocate();

    private native boolean nativeInit();

    private native boolean openNativeLibrary(String libName);

    private native boolean bindInitFunction(String funcName);
    private native boolean bindSetValueFunction(String funcName);
    private native boolean bindGetValueFunction(String funcName);
    private native boolean bindProcessFunction(String funcName);
    private native boolean bindTeardownFunction(String funcName);

    private native boolean callNativeInit();
    private native boolean callNativeSetValue(String key, Object value);
    private native Object  callNativeGetValue(String key);
    private native boolean callNativeProcess(NativeFrame[] inputs, NativeFrame output);
    private native boolean callNativeTeardown();
}
