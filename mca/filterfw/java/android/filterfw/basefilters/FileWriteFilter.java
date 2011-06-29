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


package android.filterpacks.base;

import android.content.Context;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.GenerateFieldPort;

import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Set;

/**
 * @hide
 */
public class FileWriteFilter extends Filter {

    @GenerateFieldPort(name = "fileName")
    private String mOutputName;

    @GenerateFieldPort(name = "context")
    private Context mActivityContext;

    private BufferedOutputStream mWriter;

    public FileWriteFilter(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        addInputPort("data");
    }

    @Override
    public void fieldPortValueUpdated(String name, FilterContext context) {
        if (isOpen()) {
            throw new RuntimeException("Cannot update parameters while filter is open!");
        }
    }

    @Override
    public void open(FilterContext context) {
        FileOutputStream outStream = null;
        try {
            outStream = mActivityContext.openFileOutput(mOutputName, Context.MODE_PRIVATE);
        } catch (FileNotFoundException exception) {
            throw new RuntimeException("FileWriter: Could not open file: " + mOutputName + ": " +
                                       exception.getMessage());
        }
        mWriter = new BufferedOutputStream(outStream);
    }

    @Override
    public void process(FilterContext context) {
        Frame input = pullInput("data");
        ByteBuffer data;

        if (input.getFormat().getObjectClass() == String.class) {
            String stringVal = (String)input.getObjectValue();
            data = ByteBuffer.wrap(stringVal.getBytes());
        } else {
            data = input.getData();
        }
        try {
            mWriter.write(data.array(), 0, data.limit());
            mWriter.flush();
        } catch (IOException exception) {
            throw new RuntimeException("FileWriter: Could not write to file: " + mOutputName + "!");
        }
    }

    @Override
    public void close(FilterContext context) {
        try {
            mWriter.close();
        } catch (IOException exception) {
            throw new RuntimeException("FileWriter: Could not close file!");
        }
    }
}
