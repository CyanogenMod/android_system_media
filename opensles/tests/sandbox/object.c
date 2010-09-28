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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "SLES/OpenSLES.h"
#include "SLES/OpenSLESUT.h"

int main(int arg, char **argv)
{
    SLresult result;

    printf("Create engine\n");
    SLObjectItf engineObject;
    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    SLEngineItf engineEngine;
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    // loop through both valid and invalid object IDs
    SLuint32 objectID;
    for (objectID = 0x1000; objectID <= 0x100B; ++objectID) {
        printf("object ID %lx", objectID);
        const char *string = slesutObjectIDToString(objectID);
        if (NULL != string)
            printf(" (%s)", string);
        printf(":\n");
        SLuint32 numSupportedInterfaces = 12345;
        result = (*engineEngine)->QueryNumSupportedInterfaces(engineEngine, objectID,
                &numSupportedInterfaces);
        if (SL_RESULT_FEATURE_UNSUPPORTED == result) {
            printf("  unsupported\n");
            continue;
        }
        assert(SL_RESULT_SUCCESS == result);
        printf("numSupportedInterfaces %lu\n", numSupportedInterfaces);
        SLuint32 i;
        for (i = 0; i < numSupportedInterfaces + 1; ++i) {
            SLInterfaceID interfaceID;
            result = (*engineEngine)->QuerySupportedInterfaces(engineEngine, objectID, i,
                    &interfaceID);
            if (i < numSupportedInterfaces) {
                assert(SL_RESULT_SUCCESS == result);
                printf("    interface %lu ", i);
                slesutPrintIID(interfaceID);
            } else {
                assert(SL_RESULT_PARAMETER_INVALID == result);
            }
        }
    }
    return EXIT_SUCCESS;
}
