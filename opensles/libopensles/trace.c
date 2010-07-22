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

/* trace debugging */

#if 0 // ifdef ANDROID  // FIXME requires Stagefright LOG update
//#define LOG_NDEBUG 0
#define LOG_TAG "libOpenSLES"
#else
#define LOGV printf
#define LOGE printf
#endif

#include "sles_allinclusive.h"

#ifdef USE_TRACE

void slEnterGlobal(const char *function)
{
    LOGV("Entering %s\n", function);
}

void slLeaveGlobal(const char *function, SLresult result)
{
    if (SL_RESULT_SUCCESS != result) {
        if (SLUT_RESULT_MAX > result)
            LOGV("Leaving %s (%s)\n", function, slutResultStrings[result]);
        else
            LOGV("Leaving %s (0x%X)\n", function, (unsigned) result);
    }
}

void slEnterInterface(const char *function)
{
    if (*function == 'I')
        ++function;
    const char *underscore = function;
    while (*underscore != '\0') {
        if (*underscore == '_') {
            if (strcmp(function, "BufferQueue_Enqueue") && strcmp(function, "BufferQueue_GetState")
                && strcmp(function, "OutputMixExt_FillBuffer") ) {
                LOGV("Entering %.*s::%s\n", underscore - function, function, &underscore[1]);
            }
            return;
        }
        ++underscore;
    }
    LOGV("Entering %s\n", function);
}

void slLeaveInterface(const char *function, SLresult result)
{
    if (SL_RESULT_SUCCESS != result) {
        if (*function == 'I')
            ++function;
        const char *underscore = function;
        while (*underscore != '\0') {
            if (*underscore == '_') {
                if (SLUT_RESULT_MAX > result)
                    LOGE("Leaving %.*s::%s (%s)\n", underscore - function, function,
                        &underscore[1], slutResultStrings[result]);
                else
                    LOGE("Leaving %.*s::%s (0x%X)\n", underscore - function, function,
                        &underscore[1], (unsigned) result);
                return;
            }
            ++underscore;
        }
        if (SLUT_RESULT_MAX > result)
            LOGE("Leaving %s (%s)\n", function, slutResultStrings[result]);
        else
            LOGE("Leaving %s (0x%X)\n", function, (unsigned) result);
    }
}

void slEnterInterfaceVoid(const char *function)
{
    slEnterInterface(function);
}

void slLeaveInterfaceVoid(const char *function)
{
    slLeaveInterface(function, SL_RESULT_SUCCESS);
}

#endif // defined(USE_TRACE)
