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

/* OpenSL ES Utility Toolkit */

#include "OpenSLES.h"
#include "OpenSLUT.h"
#include <stdio.h>
#include <string.h>

const char * const slutResultStrings[SLUT_RESULT_MAX] = {
    "SUCCESS",
    "PRECONDITIONS_VIOLATED",
    "PARAMETER_INVALID",
    "MEMORY_FAILURE",
    "RESOURCE_ERROR",
    "RESOURCE_LOST",
    "IO_ERROR",
    "BUFFER_INSUFFICIENT",
    "CONTENT_CORRUPTED",
    "CONTENT_UNSUPPORTED",
    "CONTENT_NOT_FOUND",
    "PERMISSION_DENIED",
    "FEATURE_UNSUPPORTED",
    "INTERNAL_ERROR",
    "UNKNOWN_ERROR",
    "OPERATION_ABORTED",
    "CONTROL_LOST"
};

typedef struct
{
    const SLInterfaceID *iid;
    const char *name;
} Pair;

// ## is token concatenation e.g. a##b becomes ab
// # is stringize operator to convert a symbol to a string constant e.g. #a becomes "a"

#define _(x) { &SL_IID_##x, #x }

Pair pairs[] = {
    _(3DCOMMIT),
    _(3DDOPPLER),
    _(3DGROUPING),
    _(3DLOCATION),
    _(3DMACROSCOPIC),
    _(3DSOURCE),
    _(AUDIODECODERCAPABILITIES),
    _(AUDIOENCODER),
    _(AUDIOENCODERCAPABILITIES),
    _(AUDIOIODEVICECAPABILITIES),
    _(BASSBOOST),
    _(BUFFERQUEUE),
    _(DEVICEVOLUME),
    _(DYNAMICINTERFACEMANAGEMENT),
    _(DYNAMICSOURCE),
    _(EFFECTSEND),
    _(ENGINE),
    _(ENGINECAPABILITIES),
    _(ENVIRONMENTALREVERB),
    _(EQUALIZER),
    _(LED),
    _(METADATAEXTRACTION),
    _(METADATATRAVERSAL),
    _(MIDIMESSAGE),
    _(MIDIMUTESOLO),
    _(MIDITEMPO),
    _(MIDITIME),
    _(MUTESOLO),
    _(NULL),
    _(OBJECT),
    _(OUTPUTMIX),
    _(PITCH),
    _(PLAY),
    _(PLAYBACKRATE),
    _(PREFETCHSTATUS),
    _(PRESETREVERB),
    _(RATEPITCH),
    _(RECORD),
    _(SEEK),
    _(THREADSYNC),
    _(VIBRA),
    _(VIRTUALIZER),
    _(VISUALIZATION),
    _(VOLUME)
};

void slutPrintIID(SLInterfaceID iid)
{
    Pair *p;
    const Pair *end = &pairs[sizeof(pairs)/sizeof(pairs[0])];
    for (p = pairs; p != end; ++p) {
        if (!memcmp(*p->iid, iid, sizeof(struct SLInterfaceID_))) {
            printf("SL_IID_%s = ", p->name);
            break;
        }
    }
    printf(
        "{ 0x%08X, 0x%04X, 0x%04X, 0x%04X, { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X } }\n",
        (unsigned) iid->time_low, iid->time_mid, iid->time_hi_and_version, iid->clock_seq,
        iid->node[0], iid->node[1], iid->node[2], iid->node[3], iid->node[4], iid->node[5]);
}
