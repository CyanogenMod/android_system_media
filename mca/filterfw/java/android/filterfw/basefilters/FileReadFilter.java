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
import android.content.res.Resources;

import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.GenerateFieldPort;
import android.filterfw.core.GenerateFinalPort;
import android.filterfw.core.KeyValueMap;
import android.filterfw.core.MutableFrameFormat;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Set;

import android.util.Log;

/**
 * @hide
 */
public class FileReadFilter extends Filter {

    @GenerateFinalPort(name = "target")
    private String mTarget;

    @GenerateFieldPort(name = "context")
    private Context mActivityContext;

    @GenerateFieldPort(name = "format")
    private FrameFormat mFrameFormat;

    // TODO: Split this into two (subclass) filters: FileReadFilter and ResourceReadFilter.
    @GenerateFieldPort(name = "resourceId", hasDefault = true)
    private int mResourceId = -1;

    @GenerateFieldPort(name = "fileName", hasDefault = true)
    private String mFileInputName;

    private final int FILE_MODE_FILE      = 0;
    private final int FILE_MODE_RESOURCE  = 1;

    private MutableFrameFormat mOutputFormat;

    private InputStream mInputStream;
    private int mFileMode = FILE_MODE_FILE;

    public FileReadFilter(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        int target = FrameFormat.readTargetString(mTarget);
        mOutputFormat = new MutableFrameFormat(FrameFormat.TYPE_BYTE, target);
        mOutputFormat.setDimensionCount(1);
        addOutputPort("data", mOutputFormat);
    }

    @Override
    public void open(FilterContext context) {
        // Read file input parameter
        if (mFileInputName != null) {
            mFileMode = FILE_MODE_FILE;
        } else if (mResourceId >= 0) {
            mFileMode = FILE_MODE_RESOURCE;
        } else {
            throw new RuntimeException("No input file specified! Please set fileName or resourceId!");
        }

        // Open the file
        try {
            switch (mFileMode) {
                case FILE_MODE_FILE:
                    mInputStream = mActivityContext.openFileInput(mFileInputName);
                    break;
                case FILE_MODE_RESOURCE:
                    mInputStream = mActivityContext.getResources().openRawResource(mResourceId);
                    break;
            }
            Log.i("FileReadFilter", "InputStream = " + mInputStream);
        } catch (FileNotFoundException exception) {
            throw new RuntimeException("FileReader: Could not open file: " + mFileInputName + "!");
        }
    }

    @Override
    public void process(FilterContext context) {
        int fileSize = 0;
        ByteBuffer byteBuffer = null;

        // Read the file
        try {
            ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
            byte[] buffer = new byte[1024];
            int bytesRead;
            while ((bytesRead = mInputStream.read(buffer)) > 0) {
                byteStream.write(buffer, 0, bytesRead);
                fileSize += bytesRead;
            }
            byteBuffer = ByteBuffer.wrap(byteStream.toByteArray());
        } catch (IOException exception) {
            throw new RuntimeException("FileReader: Could not read from file: " +
                                       mFileInputName + "!");
        }

        // Put it into a frame
        mOutputFormat.setDimensions(fileSize);
        Frame output = context.getFrameManager().newFrame(mOutputFormat);
        output.setData(byteBuffer);

        // Push output
        pushOutput("data", output);

        // Release pushed frame
        output.release();

        // Close output port as we are done here
        closeOutputPort("data");
    }

    @Override
    public void close(FilterContext context) {
        try {
            mInputStream.close();
        } catch (IOException exception) {
            throw new RuntimeException("FileReader: Could not close file!");
        }
    }
}
