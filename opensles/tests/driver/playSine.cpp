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

#define LOG_TAG "playSine"

#include <utils/Log.h>
/*
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/TextOutput.h>
#include <utils/Vector.h>
*/
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

//#include <AudioTrack.h>

#include "OpenSLES.h"

/*using namespace android;*/

extern "C" {
    extern void CheckErr( SLresult res );
    void TestPlayMusicBufferQueue( SLObjectItf sl );
}


static long gSampleRate;

static void usage(const char *me) {
    LOGE("usage: %s\n", me);
    LOGE("       -f frequency (in Hertz)\n");
}

int main(int argc, char* const argv[])
{
    //int res;
    LOGE("Starting playSine\n");
    /*while ((res = getopt(argc, argv, "f:")) >= 0) {
        switch (res) {
        case 'f':
            {
                char *end;
                long f = strtol(optarg, &end, 10);
                break;
            }

        case '?':
        case 'h':
        default:
            {
                usage(argv[0]);
                exit(1);
                break;
            }
        }
    }*/

    SLresult    res;
    SLObjectItf sl;

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE,
            (SLuint32) SL_BOOLEAN_TRUE}};

    res = slCreateEngine( &sl, 1, EngineOption, 0, NULL, NULL);
    CheckErr(res);
    /* Realizing the SL Engine in synchronous mode. */
    res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE); CheckErr(res);
    TestPlayMusicBufferQueue(sl);
    /* Shutdown OpenSL ES */
    (*sl)->Destroy(sl);
    exit(0);


    return 0;
}
