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


package android.filterpacks.imageproc;

import android.content.Context;
import android.filterfw.core.Filter;
import android.filterfw.core.FilterContext;
import android.filterfw.core.Frame;
import android.filterfw.core.FrameFormat;
import android.filterfw.core.GenerateFieldPort;
import android.filterfw.core.KeyValueMap;
import android.filterfw.format.ImageFormat;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;

import android.util.Log;

import java.io.FileOutputStream;
import java.io.BufferedOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * @hide
 */
public class ImageEncoder extends Filter {

    @GenerateFieldPort(name = "fileName")
    private String mOutputName;

    @GenerateFieldPort(name = "context")
    private Context mContext;

    public ImageEncoder(String name) {
        super(name);
    }

    @Override
    public void setupPorts() {
        addMaskedInputPort("image", ImageFormat.create(ImageFormat.COLORSPACE_RGBA,
                                                       FrameFormat.TARGET_UNSPECIFIED));
    }

    @Override
    public void process(FilterContext env) {
        Frame input = pullInput("image");
        Bitmap bitmap = input.getBitmap();
        FileOutputStream outStream = null;

        try {
            outStream = mContext.openFileOutput(mOutputName, Context.MODE_PRIVATE);
        } catch (FileNotFoundException exception) {
            throw new RuntimeException("ImageEncoder: Could not open file: " + mOutputName + "!");
        }

        BufferedOutputStream bufferedStream = new BufferedOutputStream(outStream);
        bitmap.compress(CompressFormat.JPEG, 80, bufferedStream);

        try {
            bufferedStream.flush();
            bufferedStream.close();
        } catch (IOException exception) {
            throw new RuntimeException("ImageEncoder: Could not write to file: " +
                                       mOutputName + "!");
        }
    }

}
