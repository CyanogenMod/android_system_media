/*
 * Copyright (C) 2010 The Android Open Source Project
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

/* Our own merged version of SLDataSource and SLDataSink */

typedef union {
    SLuint32 mLocatorType;
    SLDataLocator_Address mAddress;
    SLDataLocator_BufferQueue mBufferQueue;
    SLDataLocator_IODevice mIODevice;
    SLDataLocator_MIDIBufferQueue mMIDIBufferQueue;
    SLDataLocator_OutputMix mOutputMix;
    SLDataLocator_URI mURI;
    XADataLocator_NativeDisplay mNativeDisplay;
#ifdef ANDROID
    SLDataLocator_AndroidFD mFD;
    SLDataLocator_AndroidBufferQueue mBQ;
#endif
} DataLocator;

typedef union {
    SLuint32 mFormatType;
    SLDataFormat_PCM mPCM;
    SLDataFormat_MIME mMIME;
    XADataFormat_RawImage mRawImage;
} DataFormat;

typedef struct {
    union {
        SLDataSource mSource;
        SLDataSink mSink;
        struct {
            DataLocator *pLocator;
            DataFormat *pFormat;
        } mNeutral;
    } u;
    DataLocator mLocator;
    DataFormat mFormat;
} DataLocatorFormat;
