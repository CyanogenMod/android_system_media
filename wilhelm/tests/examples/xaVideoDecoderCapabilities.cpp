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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h> // for VP8 definitions

#define NUM_ENGINE_INTERFACES 1

char unknown[50];

//-----------------------------------------------------------------
/* Exits the application if an error is encountered */
#define ExitOnError(x) ExitOnErrorFunc(x,__LINE__)

void ExitOnErrorFunc( XAresult result , int line)
{
    if (XA_RESULT_SUCCESS != result) {
        fprintf(stderr, "Error %u encountered at line %d, exiting\n", result, line);
        exit(EXIT_FAILURE);
    }
}

const char* videoCodecIdToString(XAuint32 decoderId) {
    switch(decoderId) {
    case XA_VIDEOCODEC_MPEG2: return "XA_VIDEOCODEC_MPEG2"; break;
    case XA_VIDEOCODEC_H263: return "XA_VIDEOCODEC_H263"; break;
    case XA_VIDEOCODEC_MPEG4: return "XA_VIDEOCODEC_MPEG4"; break;
    case XA_VIDEOCODEC_AVC: return "XA_VIDEOCODEC_AVC"; break;
    case XA_VIDEOCODEC_VC1: return "XA_VIDEOCODEC_VC1"; break;
    case XA_ANDROID_VIDEOCODEC_VP8: return "XA_ANDROID_VIDEOCODEC_VP8"; break;
    default:
        sprintf(unknown, "Video codec %d unknown to OpenMAX AL", decoderId);
        return unknown;
    }
}

//-----------------------------------------------------------------
void TestVideoDecoderCapabilities() {

    XAObjectItf xa;
    XAresult res;

    /* parameters for the OpenMAX AL engine creation */
    XAEngineOption EngineOption[] = {
            {(XAuint32) XA_ENGINEOPTION_THREADSAFE, (XAuint32) XA_BOOLEAN_TRUE}
    };
    XAInterfaceID itfIidArray[NUM_ENGINE_INTERFACES] = { XA_IID_VIDEODECODERCAPABILITIES };
    XAboolean     itfRequired[NUM_ENGINE_INTERFACES] = { XA_BOOLEAN_TRUE };

    /* create OpenMAX AL engine */
    res = xaCreateEngine( &xa, 1, EngineOption, NUM_ENGINE_INTERFACES, itfIidArray, itfRequired);
    ExitOnError(res);

    /* realize the engine in synchronous mode. */
    res = (*xa)->Realize(xa, XA_BOOLEAN_FALSE); ExitOnError(res);

    /* Get the video decoder capabilities interface which was explicitly requested */
    XAVideoDecoderCapabilitiesItf decItf;
    res = (*xa)->GetInterface(xa, XA_IID_VIDEODECODERCAPABILITIES, (void*)&decItf);
    ExitOnError(res);

    /* Query the platform capabilities */
    XAuint32 numDecoders = 0;
    XAuint32 *decoderIds = NULL;

    /* -> Number of decoders */
    res = (*decItf)->GetVideoDecoders(decItf, &numDecoders, NULL); ExitOnError(res);
    fprintf(stdout, "Found %d video decoders\n", numDecoders);
    if (0 == numDecoders) {
        fprintf(stderr, "0 video decoders is not an acceptable number, exiting\n");
        goto destroyRes;
    }

    /* -> Decoder list */
    decoderIds = (XAuint32 *) malloc(numDecoders * sizeof(XAuint32));
    res = (*decItf)->GetVideoDecoders(decItf, &numDecoders, decoderIds); ExitOnError(res);
    fprintf(stdout, "Decoders:\n");
    for(XAuint32 i = 0 ; i < numDecoders ; i++) {
        fprintf(stdout, "decoder %d is %s\n", i, videoCodecIdToString(decoderIds[i]));
    }

    /* -> Decoder capabilities */
    /*       for each decoder  */
    for(XAuint32 i = 0 ; i < numDecoders ; i++) {
        XAuint32 nbCombinations = 0;
        /* get the number of profile / level combinations */
        res = (*decItf)->GetVideoDecoderCapabilities(decItf, decoderIds[i], &nbCombinations, NULL);
        ExitOnError(res);
        fprintf(stdout, "decoder %s has %d profile/level combinations:\n\t",
                videoCodecIdToString(decoderIds[i]), nbCombinations);
        /* display the profile / level combinations */
        for(XAuint32 pl = 0 ; pl < nbCombinations ; pl++) {
            XAVideoCodecDescriptor decDescriptor;
            res = (*decItf)->GetVideoDecoderCapabilities(decItf, decoderIds[i], &pl, &decDescriptor);
            ExitOnError(res);
            fprintf(stdout, "%u/%u ", decDescriptor.profileSetting, decDescriptor.levelSetting);
            ExitOnError(res);
        }
        fprintf(stdout, "\n");
    }

destroyRes:
    free(decoderIds);

    /* shutdown OpenMAX AL */
    (*xa)->Destroy(xa);
}


//-----------------------------------------------------------------
int main(int argc, char* const argv[])
{
    XAresult    result;
    XAObjectItf sl;

    fprintf(stdout, "OpenMAX AL test %s: exercises SLAudioDecoderCapabiltiesItf ", argv[0]);
    fprintf(stdout, "and displays the list of supported video decoders, and for each, lists the ");
    fprintf(stdout, "profile / levels combinations, that map to the constants defined in ");
    fprintf(stdout, "\"XA_VIDEOPROFILE and XA_VIDEOLEVEL\" section of the specification\n\n");

    TestVideoDecoderCapabilities();

    return EXIT_SUCCESS;
}
