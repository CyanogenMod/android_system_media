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

/* Android stream type implementation */

#ifdef ANDROID

#include "sles_allinclusive.h"


static SLresult IAndroidStreamType_SetStreamType(SLAndroidStreamTypeItf self, SLuint32 type)
{
    SL_ENTER_INTERFACE

    fprintf(stdout, "IAndroidStreamType_SetStreamType for type %lu\n", type);

    if (type >= android::AudioSystem::NUM_STREAM_TYPES) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IAndroidStreamType *this = (IAndroidStreamType *) self;

        interface_lock_exclusive(this);
        this->mStreamType = type;
        switch (InterfaceToObjectID(this)) {
        case SL_OBJECTID_AUDIOPLAYER:
            sles_to_android_audioPlayerSetStreamType_l(InterfaceToCAudioPlayer(this), type);
            break;
        case SL_OBJECTID_MIDIPLAYER:
            // FIXME implement once we support MIDIPlayer
            break;
        default:
            break;
        }
        interface_unlock_exclusive(this);

        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidStreamType_GetStreamType(SLAndroidStreamTypeItf self, SLuint32 *pType)
{
    SL_ENTER_INTERFACE

    if (NULL == pType) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IAndroidStreamType *this = (IAndroidStreamType *) self;

        interface_lock_peek(this);
        SLuint32 type = this->mStreamType;
        interface_unlock_peek(this);

        *pType = type;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static const struct SLAndroidStreamTypeItf_ IAndroidStreamType_Itf = {
    IAndroidStreamType_SetStreamType,
    IAndroidStreamType_GetStreamType
};

void IAndroidStreamType_init(void *self)
{
    IAndroidStreamType *this = (IAndroidStreamType *) self;
    this->mItf = &IAndroidStreamType_Itf;
    this->mStreamType = ANDROID_DEFAULT_OUTPUT_STREAM_TYPE;
}

#endif // #ifdef ANDROID
