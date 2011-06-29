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

import android.util.Log;

/**
 * Settings for MCA-specific logging.
 *
 * @hide
 */
public class LogMCA {

    // Set this to true to enable per-frame logging.
    public static final boolean LOG_EVERY_FRAME = false;

    public static final String TAG = "MCA";

    public static final void perFrame(String message) {
        if (LOG_EVERY_FRAME) {
            Log.v(TAG, message);
        }
    }

}
