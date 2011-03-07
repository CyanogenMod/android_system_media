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

#include "sles_allinclusive.h"


/** \brief Exclusively lock an object */

#ifdef USE_DEBUG
void object_lock_exclusive_(IObject *thiz, const char *file, int line)
{
    int ok;
    ok = pthread_mutex_trylock(&thiz->mMutex);
    if (0 != ok) {
        // pthread_mutex_timedlock_np is not available, but wait up to 100 ms
        static const useconds_t backoffs[] = {1, 10000, 20000, 30000, 40000};
        unsigned i = 0;
        for (;;) {
            usleep(backoffs[i]);
            ok = pthread_mutex_trylock(&thiz->mMutex);
            if (0 == ok)
                break;
            if (++i >= (sizeof(backoffs) / sizeof(backoffs[0]))) {
                SL_LOGW("%s:%d: object %p was locked by %p at %s:%d\n",
                    file, line, thiz, *(void **)&thiz->mOwner, thiz->mFile, thiz->mLine);
                // attempt one more time; maybe this time we will be successful
                ok = pthread_mutex_lock(&thiz->mMutex);
                assert(0 == ok);
                break;
            }
        }
    }
    pthread_t zero;
    memset(&zero, 0, sizeof(pthread_t));
    if (0 != memcmp(&zero, &thiz->mOwner, sizeof(pthread_t))) {
        if (pthread_equal(pthread_self(), thiz->mOwner)) {
            SL_LOGE("%s:%d: object %p was recursively locked by %p at %s:%d\n",
                file, line, thiz, *(void **)&thiz->mOwner, thiz->mFile, thiz->mLine);
        } else {
            SL_LOGE("%s:%d: object %p was left unlocked in unexpected state by %p at %s:%d\n",
                file, line, thiz, *(void **)&thiz->mOwner, thiz->mFile, thiz->mLine);
        }
        assert(false);
    }
    thiz->mOwner = pthread_self();
    thiz->mFile = file;
    thiz->mLine = line;
}
#else
void object_lock_exclusive(IObject *thiz)
{
    int ok;
    ok = pthread_mutex_lock(&thiz->mMutex);
    assert(0 == ok);
}
#endif


/** \brief Exclusively unlock an object and do not report any updates */

#ifdef USE_DEBUG
void object_unlock_exclusive_(IObject *thiz, const char *file, int line)
{
    assert(pthread_equal(pthread_self(), thiz->mOwner));
    assert(NULL != thiz->mFile);
    assert(0 != thiz->mLine);
    memset(&thiz->mOwner, 0, sizeof(pthread_t));
    thiz->mFile = file;
    thiz->mLine = line;
    int ok;
    ok = pthread_mutex_unlock(&thiz->mMutex);
    assert(0 == ok);
}
#else
void object_unlock_exclusive(IObject *thiz)
{
    int ok;
    ok = pthread_mutex_unlock(&thiz->mMutex);
    assert(0 == ok);
}
#endif


/** \brief Exclusively unlock an object and report updates to the specified bit-mask of
 *  attributes
 */

#ifdef USE_DEBUG
void object_unlock_exclusive_attributes_(IObject *thiz, unsigned attributes,
    const char *file, int line)
#else
void object_unlock_exclusive_attributes(IObject *thiz, unsigned attributes)
#endif
{

#ifdef USE_DEBUG
    assert(pthread_equal(pthread_self(), thiz->mOwner));
    assert(NULL != thiz->mFile);
    assert(0 != thiz->mLine);
#endif

    int ok;
    SLuint32 objectID = IObjectToObjectID(thiz);
    CAudioPlayer *ap;

    // FIXME The endless if statements are getting ugly, should use bit search

    // Android likes to see certain updates synchronously

    if (attributes & ATTR_GAIN) {
        switch (objectID) {
        case SL_OBJECTID_AUDIOPLAYER:
            attributes &= ~ATTR_GAIN;   // no need to process asynchronously also
            ap = (CAudioPlayer *) thiz;
#ifdef ANDROID
            android_audioPlayer_volumeUpdate(ap);
#else
            audioPlayerGainUpdate(ap);
#endif
            break;
        case SL_OBJECTID_OUTPUTMIX:
            // FIXME update gains on all players attached to this outputmix
            SL_LOGD("[ FIXME: gain update on an SL_OBJECTID_OUTPUTMIX to be implemented ]");
            break;
        case SL_OBJECTID_MIDIPLAYER:
            // MIDI
            SL_LOGD("[ FIXME: gain update on an SL_OBJECTID_MIDIPLAYER to be implemented ]");
            break;
        default:
            break;
        }
    }

    if (attributes & ATTR_POSITION) {
        switch (objectID) {
        case SL_OBJECTID_AUDIOPLAYER:
#ifdef ANDROID
            ap = (CAudioPlayer *) thiz;
            attributes &= ~ATTR_POSITION;   // no need to process asynchronously also
            android_audioPlayer_seek(ap, ap->mSeek.mPos);
#else
            //audioPlayerTransportUpdate(ap);
#endif
            break;
        case SL_OBJECTID_MIDIPLAYER:
            // MIDI
            SL_LOGD("[ FIXME: position update on an SL_OBJECTID_MIDIPLAYER to be implemented ]");
            break;
        default:
            break;
        }
    }

    if (attributes & ATTR_TRANSPORT) {
        switch (objectID) {
        case SL_OBJECTID_AUDIOPLAYER:
#ifdef ANDROID
            attributes &= ~ATTR_TRANSPORT;   // no need to process asynchronously also
            ap = (CAudioPlayer *) thiz;
            // FIXME should only call when state changes
            android_audioPlayer_setPlayState(ap, false /*lockAP*/);
            // FIXME ditto, but for either eventflags or marker position
            android_audioPlayer_useEventMask(ap);
#else
            //audioPlayerTransportUpdate(ap);
#endif
            break;
        case SL_OBJECTID_AUDIORECORDER:
#ifdef ANDROID
            {
            attributes &= ~ATTR_TRANSPORT;   // no need to process asynchronously also
            CAudioRecorder* ar = (CAudioRecorder *) thiz;
            android_audioRecorder_useEventMask(ar);
            }
#endif
            break;
        case XA_OBJECTID_MEDIAPLAYER:
#ifdef ANDROID
            {
            attributes &= ~ATTR_TRANSPORT;   // no need to process asynchronously also
            CMediaPlayer *mp = (CMediaPlayer *) thiz;
            android::GenericPlayer* avp = mp->mAVPlayer.get();
            if (avp != NULL) {
                android_Player_setPlayState(avp, mp->mPlay.mState, &(mp->mAndroidObjState));
            }
            }
#endif
            break;
        default:
            break;
        }
    }

    if (attributes & ATTR_BQ_ENQUEUE) {
        // ( buffer queue count is non-empty and play state == PLAYING ) became true
        if (SL_OBJECTID_AUDIOPLAYER == objectID) {
            attributes &= ~ATTR_BQ_ENQUEUE;
            ap = (CAudioPlayer *) thiz;
            if (SL_PLAYSTATE_PLAYING == ap->mPlay.mState) {
#ifdef ANDROID
                android_audioPlayer_bufferQueue_onRefilled_l(ap);
#endif
            }
        }
#ifndef ANDROID
    }
#else
    } else if (attributes & ATTR_ABQ_ENQUEUE) {
        // (Android buffer queue count is non-empty and play state == PLAYING ) became true
        if (SL_OBJECTID_AUDIOPLAYER == objectID) {
            attributes &= ~ATTR_ABQ_ENQUEUE;
            ap = (CAudioPlayer *) thiz;
            if (SL_PLAYSTATE_PLAYING == ap->mPlay.mState) {
                android_audioPlayer_androidBufferQueue_onRefilled_l(ap);
            }
        } else if (XA_OBJECTID_MEDIAPLAYER == objectID) {
            attributes &= ~ATTR_ABQ_ENQUEUE;
            CMediaPlayer* mp = (CMediaPlayer *)thiz;
            if (SL_PLAYSTATE_PLAYING == mp->mPlay.mState) {
                android_Player_androidBufferQueue_onRefilled_l(mp);
            }
        }
    }
#endif

    if (attributes) {
        unsigned oldAttributesMask = thiz->mAttributesMask;
        thiz->mAttributesMask = oldAttributesMask | attributes;
        if (oldAttributesMask)
            attributes = ATTR_NONE;
    }
#ifdef USE_DEBUG
    memset(&thiz->mOwner, 0, sizeof(pthread_t));
    thiz->mFile = file;
    thiz->mLine = line;
#endif
    ok = pthread_mutex_unlock(&thiz->mMutex);
    assert(0 == ok);
    // first update to this interface since previous sync
    if (attributes) {
        unsigned id = thiz->mInstanceID;
        if (0 != id) {
            --id;
            assert(MAX_INSTANCE > id);
            IEngine *thisEngine = &thiz->mEngine->mEngine;
            interface_lock_exclusive(thisEngine);
            thisEngine->mChangedMask |= 1 << id;
            interface_unlock_exclusive(thisEngine);
        }
    }
}


/** \brief Wait on the condition variable associated with the object; see pthread_cond_wait */

#ifdef USE_DEBUG
void object_cond_wait_(IObject *thiz, const char *file, int line)
{
    // note that this will unlock the mutex, so we have to clear the owner
    assert(pthread_equal(pthread_self(), thiz->mOwner));
    assert(NULL != thiz->mFile);
    assert(0 != thiz->mLine);
    memset(&thiz->mOwner, 0, sizeof(pthread_t));
    thiz->mFile = file;
    thiz->mLine = line;
    // alas we don't know the new owner's identity
    int ok;
    ok = pthread_cond_wait(&thiz->mCond, &thiz->mMutex);
    assert(0 == ok);
    // restore my ownership
    thiz->mOwner = pthread_self();
    thiz->mFile = file;
    thiz->mLine = line;
}
#else
void object_cond_wait(IObject *thiz)
{
    int ok;
    ok = pthread_cond_wait(&thiz->mCond, &thiz->mMutex);
    assert(0 == ok);
}
#endif


/** \brief Signal the condition variable associated with the object; see pthread_cond_signal */

void object_cond_signal(IObject *thiz)
{
    int ok;
    ok = pthread_cond_signal(&thiz->mCond);
    assert(0 == ok);
}


/** \brief Broadcast the condition variable associated with the object;
 *  see pthread_cond_broadcast
 */

void object_cond_broadcast(IObject *thiz)
{
    int ok;
    ok = pthread_cond_broadcast(&thiz->mCond);
    assert(0 == ok);
}
