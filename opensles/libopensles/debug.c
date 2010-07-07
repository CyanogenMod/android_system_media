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

/* debugging */

#include "sles_allinclusive.h"

#ifndef NDEBUG

#define SL_RESULT_MAX (SL_RESULT_CONTROL_LOST + 1)
static const char * const result_strings[SL_RESULT_MAX] = {
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

void slEnterGlobal(const char *function)
{
    printf("Entering %s\n", function);
}

void slLeaveGlobal(const char *function, SLresult result)
{
    if (SL_RESULT_SUCCESS != result) {
        if (SL_RESULT_MAX > result)
            printf("Leaving %s (%s)\n", function, result_strings[result]);
        else
            printf("Leaving %s (0x%X)\n", function, (unsigned) result);
    }
}

void slEnterInterface(const char *function)
{
    if (*function == 'I')
        ++function;
    const char *underscore = function;
    while (*underscore != '\0') {
        if (*underscore == '_') {
            printf("Entering %.*s::%s\n", underscore - function, function, &underscore[1]);
            return;
        }
        ++underscore;
    }
    printf("Entering %s\n", function);
}

void slLeaveInterface(const char *function, SLresult result)
{
    if (SL_RESULT_SUCCESS != result) {
        if (*function == 'I')
            ++function;
        const char *underscore = function;
        while (*underscore != '\0') {
            if (*underscore == '_') {
                if (SL_RESULT_MAX > result)
                    printf("Leaving %.*s::%s (%s)\n", underscore - function, function,
                        &underscore[1], result_strings[result]);
                else
                    printf("Leaving %.*s::%s (0x%X)\n", underscore - function, function,
                        &underscore[1], (unsigned) result);
                return;
            }
            ++underscore;
        }
        if (SL_RESULT_MAX > result)
            printf("Leaving %s (%s)\n", function, result_strings[result]);
        else
            printf("Leaving %s (0x%X)\n", function, (unsigned) result);
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

#endif // !defined(NDEBUG)
