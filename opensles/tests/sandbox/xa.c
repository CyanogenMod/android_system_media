#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "OMXAL/OpenMAXAL.h"

int main(int argc, char **argv)
{
    XAresult result;
    XAObjectItf engineObject;
    printf("xaCreateEngine\n");
    result = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    printf("result = %ld\n", result);
    assert(XA_RESULT_SUCCESS == result);
    printf("engineObject = %p\n", engineObject);
    printf("realize\n");
    result = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    printf("result = %ld\n", result);
    printf("GetInterface for ENGINE\n");
    XAEngineItf engineEngine;
    result = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    printf("result = %ld\n", result);
    printf("engineEngine = %p\n", engineEngine);
    assert(XA_RESULT_SUCCESS == result);

    XAObjectItf outputMixObject;
    printf("CreateOutputMix");
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    printf("result = %ld, outputMixObject=%p\n", result, outputMixObject);

    XAObjectItf deviceObject;
    printf("CreateCameraDevice\n");
    result = (*engineEngine)->CreateCameraDevice(engineEngine, &deviceObject,
            XA_DEFAULTDEVICEID_CAMERA, 0, NULL, NULL);
    printf("result = %ld, deviceObject=%p\n", result, deviceObject);

    printf("CreateRadioDevice\n");
    result = (*engineEngine)->CreateRadioDevice(engineEngine, &deviceObject, 0, NULL, NULL);
    printf("result = %ld, deviceObject=%p\n", result, deviceObject);

    printf("CreateLEDDevice\n");
    result = (*engineEngine)->CreateLEDDevice(engineEngine, &deviceObject, XA_DEFAULTDEVICEID_LED,
            0, NULL, NULL);
    printf("result = %ld, deviceObject=%p\n", result, deviceObject);

    printf("CreateVibraDevice\n");
    result = (*engineEngine)->CreateVibraDevice(engineEngine, &deviceObject,
            XA_DEFAULTDEVICEID_VIBRA, 0, NULL, NULL);
    printf("result = %ld, deviceObject=%p\n", result, deviceObject);

    printf("CreateMediaPlayer\n");
    XAObjectItf playerObject;
    XADataLocator_URI locUri;
    locUri.locatorType = XA_DATALOCATOR_URI;
    locUri.URI = (XAchar *) "/sdcard/hello.wav";
    XADataFormat_MIME fmtMime;
    fmtMime.formatType = XA_DATAFORMAT_MIME;
    fmtMime.mimeType = NULL;
    fmtMime.containerType = XA_CONTAINERTYPE_UNSPECIFIED;
    XADataSource dataSrc;
    dataSrc.pLocator = &locUri;
    dataSrc.pFormat = &fmtMime;
    XADataSink audioSnk;
    XADataLocator_OutputMix locOM;
    locOM.locatorType = XA_DATALOCATOR_OUTPUTMIX;
    locOM.outputMix = outputMixObject;
    audioSnk.pLocator = &locOM;
    audioSnk.pFormat = NULL;
    XADataLocator_NativeDisplay locND;
    locND.locatorType = XA_DATALOCATOR_NATIVEDISPLAY;
    locND.hWindow = NULL;
    locND.hDisplay = NULL;
    XADataSink imageVideoSink;
    imageVideoSink.pLocator = &locND;
    imageVideoSink.pFormat = NULL;
    result = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObject, &dataSrc, NULL,
            &audioSnk, &imageVideoSink, NULL, NULL, 0, NULL, NULL);
    printf("result = %ld, playerObject=%p\n", result, playerObject);

    printf("destroying output mix\n");
    (*outputMixObject)->Destroy(outputMixObject);

    printf("destroying engine\n");
    (*engineObject)->Destroy(engineObject);
    printf("exit\n");

    return EXIT_SUCCESS;
}
