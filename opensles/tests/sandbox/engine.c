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

#include "SLES/OpenSLES.h"
#include "SLES/OpenSLESUT.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    printf("Get number of available engine interfaces\n");
    SLresult result;
    SLuint32 numSupportedInterfaces = 12345;
    result = slQueryNumSupportedEngineInterfaces(&numSupportedInterfaces);
    assert(SL_RESULT_SUCCESS == result);
    printf("Engine number of supported interfaces %lu\n", numSupportedInterfaces);
    SLInterfaceID *engine_ids = calloc(numSupportedInterfaces, sizeof(SLInterfaceID));
    assert(engine_ids != NULL);
    SLboolean *engine_req = calloc(numSupportedInterfaces, sizeof(SLboolean));
    assert(engine_req != NULL);
    printf("Display the ID of each available interface\n");
    SLuint32 index;
    for (index = 0; index < numSupportedInterfaces + 1; ++index) {
        SLInterfaceID interfaceID;
        memset(&interfaceID, 0x55, sizeof(interfaceID));
        result = slQuerySupportedEngineInterfaces(index, &interfaceID);
        if (index < numSupportedInterfaces) {
            assert(SL_RESULT_SUCCESS == result);
            printf("interface[%lu] ", index);
            slesutPrintIID(interfaceID);
            engine_ids[index] = interfaceID;
            engine_req[index] = SL_BOOLEAN_TRUE;
        } else {
            assert(SL_RESULT_PARAMETER_INVALID == result);
        }
    }
    printf("Create an engine and request all available interfaces\n");
    SLObjectItf engineObject;
    result = slCreateEngine(&engineObject, 0, NULL, numSupportedInterfaces, engine_ids, engine_req);
    assert(SL_RESULT_SUCCESS == result);
    printf("Engine object %p\n", engineObject);
    printf("Get each available interface before realization\n");
    for (index = 0; index < numSupportedInterfaces; ++index) {
        void *interface = NULL;
        // Use the interface ID as returned by slQuerySupportedEngineInterfaces
        result = (*engineObject)->GetInterface(engineObject, engine_ids[index], &interface);
        assert(SL_RESULT_PRECONDITIONS_VIOLATED == result);
    }
    printf("Destroy engine before realization\n");
    (*engineObject)->Destroy(engineObject);
    printf("Create engine again\n");
    result = slCreateEngine(&engineObject, 0, NULL, numSupportedInterfaces, engine_ids, engine_req);
    assert(SL_RESULT_SUCCESS == result);
    printf("Engine object %p\n", engineObject);
    printf("Realize the engine\n");
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    printf("Get each available interface after realization\n");
    for (index = 0; index < numSupportedInterfaces; ++index) {
        void *interface = NULL;
        result = (*engineObject)->GetInterface(engineObject, engine_ids[index], &interface);
        assert(SL_RESULT_SUCCESS == result);
        printf("interface[%lu] %p\n", index, interface);
        // Use a copy of the interface ID to make sure lookup is not purely relying on address
        void *interface_again = NULL;
        struct SLInterfaceID_ copy = *engine_ids[index];
        result = (*engineObject)->GetInterface(engineObject, &copy, &interface_again);
        assert(SL_RESULT_SUCCESS == result);
        // Calling GetInterface multiple times should return the same interface
        assert(interface_again == interface);
    }
    printf("Destroy engine\n");
    (*engineObject)->Destroy(engineObject);
    return EXIT_SUCCESS;
}
