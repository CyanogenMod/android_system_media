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

/* 3DLocation implementation */

#include "sles_allinclusive.h"

static SLresult I3DLocation_SetLocationCartesian(SL3DLocationItf self,
    const SLVec3D *pLocation)
{
    if (NULL == pLocation)
        return SL_RESULT_PARAMETER_INVALID;
    I3DLocation *this = (I3DLocation *) self;
    SLVec3D locationCartesian = *pLocation;
    interface_lock_exclusive(this);
    this->mLocationCartesian = locationCartesian;
    this->mLocationActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_SetLocationSpherical(SL3DLocationItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter distance)
{
    I3DLocation *this = (I3DLocation *) self;
    interface_lock_exclusive(this);
    this->mLocationSpherical.mAzimuth = azimuth;
    this->mLocationSpherical.mElevation = elevation;
    this->mLocationSpherical.mDistance = distance;
    this->mLocationActive = CARTESIAN_UNKNOWN_SPHERICAL_SET;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_Move(SL3DLocationItf self, const SLVec3D *pMovement)
{
    if (NULL == pMovement)
        return SL_RESULT_PARAMETER_INVALID;
    I3DLocation *this = (I3DLocation *) self;
    SLVec3D movementCartesian = *pMovement;
    interface_lock_exclusive(this);
    for (;;) {
        enum CartesianSphericalActive locationActive = this->mLocationActive;
        switch (locationActive) {
        case CARTESIAN_COMPUTED_SPHERICAL_SET:
        case CARTESIAN_SET_SPHERICAL_COMPUTED:  // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_REQUESTED: // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_UNKNOWN:
            this->mLocationCartesian.x += movementCartesian.x;
            this->mLocationCartesian.y += movementCartesian.y;
            this->mLocationCartesian.z += movementCartesian.z;
            this->mLocationActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
            break;
        case CARTESIAN_UNKNOWN_SPHERICAL_SET:
            this->mLocationActive = CARTESIAN_REQUESTED_SPHERICAL_SET;
            // fall through
        case CARTESIAN_REQUESTED_SPHERICAL_SET:
            // matched by cond_broadcast in case multiple requesters
            interface_cond_wait(this);
            continue;
        default:
            assert(SL_BOOLEAN_FALSE);
            break;
        }
        break;
    }
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_GetLocationCartesian(SL3DLocationItf self,
    SLVec3D *pLocation)
{
    if (NULL == pLocation)
        return SL_RESULT_PARAMETER_INVALID;
    I3DLocation *this = (I3DLocation *) self;
    interface_lock_exclusive(this);
    for (;;) {
        enum CartesianSphericalActive locationActive = this->mLocationActive;
        switch (locationActive) {
        case CARTESIAN_COMPUTED_SPHERICAL_SET:
        case CARTESIAN_SET_SPHERICAL_COMPUTED:  // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_REQUESTED: // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_UNKNOWN:
            {
            SLVec3D locationCartesian = this->mLocationCartesian;
            interface_unlock_exclusive(this);
            *pLocation = locationCartesian;
            }
            break;
        case CARTESIAN_UNKNOWN_SPHERICAL_SET:
            this->mLocationActive = CARTESIAN_REQUESTED_SPHERICAL_SET;
            // fall through
        case CARTESIAN_REQUESTED_SPHERICAL_SET:
            // matched by cond_broadcast in case multiple requesters
            interface_cond_wait(this);
            continue;
        default:
            assert(SL_BOOLEAN_FALSE);
            interface_unlock_exclusive(this);
            pLocation->x = 0;
            pLocation->y = 0;
            pLocation->z = 0;
            break;
        }
    }
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_SetOrientationVectors(SL3DLocationItf self,
    const SLVec3D *pFront, const SLVec3D *pAbove)
{
    if (NULL == pFront || NULL == pAbove)
        return SL_RESULT_PARAMETER_INVALID;
    SLVec3D front = *pFront;
    SLVec3D above = *pAbove;
    I3DLocation *this = (I3DLocation *) self;
    interface_lock_exclusive(this);
    this->mOrientationVectors.mFront = front;
    this->mOrientationVectors.mAbove = above;
    this->mOrientationActive = ANGLES_UNKNOWN_VECTORS_SET;
    this->mRotatePending = SL_BOOLEAN_FALSE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_SetOrientationAngles(SL3DLocationItf self,
    SLmillidegree heading, SLmillidegree pitch, SLmillidegree roll)
{
    I3DLocation *this = (I3DLocation *) self;
    interface_lock_exclusive(this);
    this->mOrientationAngles.mHeading = heading;
    this->mOrientationAngles.mPitch = pitch;
    this->mOrientationAngles.mRoll = roll;
    this->mOrientationActive = ANGLES_SET_VECTORS_UNKNOWN;
    this->mRotatePending = SL_BOOLEAN_FALSE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_Rotate(SL3DLocationItf self, SLmillidegree theta,
    const SLVec3D *pAxis)
{
    if (NULL == pAxis)
        return SL_RESULT_PARAMETER_INVALID;
    SLVec3D axis = *pAxis;
    I3DLocation *this = (I3DLocation *) self;
    interface_lock_exclusive(this);
    while (this->mRotatePending)
        interface_cond_wait(this);
    this->mTheta = theta;
    this->mAxis = axis;
    this->mRotatePending = SL_BOOLEAN_TRUE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DLocation_GetOrientationVectors(SL3DLocationItf self,
    SLVec3D *pFront, SLVec3D *pUp)
{
    if (NULL == pFront || NULL == pUp)
        return SL_RESULT_PARAMETER_INVALID;
    I3DLocation *this = (I3DLocation *) self;
    interface_lock_shared(this);
    SLVec3D front = this->mOrientationVectors.mFront;
    SLVec3D up = this->mOrientationVectors.mUp;
    interface_unlock_shared(this);
    *pFront = front;
    *pUp = up;
    return SL_RESULT_SUCCESS;
}

static const struct SL3DLocationItf_ I3DLocation_Itf = {
    I3DLocation_SetLocationCartesian,
    I3DLocation_SetLocationSpherical,
    I3DLocation_Move,
    I3DLocation_GetLocationCartesian,
    I3DLocation_SetOrientationVectors,
    I3DLocation_SetOrientationAngles,
    I3DLocation_Rotate,
    I3DLocation_GetOrientationVectors
};

void I3DLocation_init(void *self)
{
    I3DLocation *this = (I3DLocation *) self;
    this->mItf = &I3DLocation_Itf;
#ifndef NDEBUG
    this->mLocationCartesian.x = 0;
    this->mLocationCartesian.y = 0;
    this->mLocationCartesian.z = 0;
    memset(&this->mLocationSpherical, 0x55, sizeof(this->mLocationSpherical));
    this->mOrientationAngles.mHeading = 0;
    this->mOrientationAngles.mPitch = 0;
    this->mOrientationAngles.mRoll = 0;
    this->mOrientationVectors.mFront.x = 0;
    this->mOrientationVectors.mFront.y = 0;
    this->mOrientationVectors.mAbove.x = 0;
    this->mOrientationVectors.mAbove.y = 0;
    this->mOrientationVectors.mUp.x = 0;
    this->mOrientationVectors.mUp.z = 0;
    this->mTheta = 0x55555555;
    this->mAxis.x = 0x55555555;
    this->mAxis.y = 0x55555555;
    this->mAxis.z = 0x55555555;
    this->mRotatePending = SL_BOOLEAN_FALSE;
#endif
    this->mLocationActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
    this->mOrientationVectors.mFront.z = -1000;
    this->mOrientationVectors.mUp.y = 1000;
    this->mOrientationVectors.mAbove.z = -1000;
    this->mOrientationActive = ANGLES_SET_VECTORS_UNKNOWN;
}
