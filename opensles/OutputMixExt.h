/* Copyright 2010 The Android Open Source Project */

/* OutputMixExt interface */

extern const SLInterfaceID SL_IID_OUTPUTMIXEXT;

typedef const struct SLOutputMixExtItf_ * const * SLOutputMixExtItf;

struct SLOutputMixExtItf_ {
    void (*FillBuffer)(SLOutputMixExtItf self, void *pBuffer, SLuint32 size);
};
