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

// This should be the only global variable
static unsigned slTraceEnabled = SL_TRACE_DEFAULT;

void slTraceSetEnabled(unsigned enabled)
{
    slTraceEnabled = enabled;
}

void slTraceEnterGlobal(const char *function)
{
    if (SL_TRACE_ENTER & slTraceEnabled)
        LOGV("Entering %s\n", function);
}

void slTraceLeaveGlobal(const char *function, SLresult result)
{
    if ((SL_RESULT_SUCCESS != result) && (SL_TRACE_LEAVE_FAILURE & slTraceEnabled)) {
        if (SLESUT_RESULT_MAX > result)
            LOGV("Leaving %s (%s)\n", function, slesutResultStrings[result]);
        else
            LOGV("Leaving %s (0x%X)\n", function, (unsigned) result);
    }
}

void slTraceEnterInterface(const char *function)
{
    if (*function == 'I')
        ++function;
    const char *underscore = function;
    while (*underscore != '\0') {
        if (*underscore == '_') {
            if ((strcmp(function, "BufferQueue_Enqueue") && strcmp(function, "BufferQueue_GetState")
                && strcmp(function, "OutputMixExt_FillBuffer")) &&
                (SL_TRACE_ENTER & slTraceEnabled)) {
                LOGV("Entering %.*s::%s\n", (int) (underscore - function), function,
                    &underscore[1]);
            }
            return;
        }
        ++underscore;
    }
    if (SL_TRACE_ENTER & slTraceEnabled)
        LOGV("Entering %s\n", function);
}

void slTraceLeaveInterface(const char *function, SLresult result)
{
    if ((SL_RESULT_SUCCESS != result) && (SL_TRACE_LEAVE_FAILURE & slTraceEnabled)) {
        if (*function == 'I')
            ++function;
        const char *underscore = function;
        while (*underscore != '\0') {
            if (*underscore == '_') {
                if (SLESUT_RESULT_MAX > result)
                    LOGE("Leaving %.*s::%s (%s)\n", (int) (underscore - function), function,
                        &underscore[1], slesutResultStrings[result]);
                else
                    LOGE("Leaving %.*s::%s (0x%X)\n", (int) (underscore - function), function,
                        &underscore[1], (unsigned) result);
                return;
            }
            ++underscore;
        }
        if (SLESUT_RESULT_MAX > result)
            LOGE("Leaving %s (%s)\n", function, slesutResultStrings[result]);
        else
            LOGE("Leaving %s (0x%X)\n", function, (unsigned) result);
    }
}

void slTraceEnterInterfaceVoid(const char *function)
{
    if (SL_TRACE_ENTER & slTraceEnabled)
        slTraceEnterInterface(function);
}

void slTraceLeaveInterfaceVoid(const char *function)
{
    if (SL_TRACE_LEAVE_VOID & slTraceEnabled)
        slTraceLeaveInterface(function, SL_RESULT_SUCCESS);
}

#else

void slTraceSetEnabled(unsigned enabled)
{
}

#endif // defined(USE_TRACE)
