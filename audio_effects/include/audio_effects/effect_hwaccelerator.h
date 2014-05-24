/*
 * Copyright (C) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ANDROID_EFFECT_HWACCELERATOR_H_
#define ANDROID_EFFECT_HWACCELERATOR_H_

#include <hardware/audio_effect.h>

#if __cplusplus
extern "C" {
#endif

#define EFFECT_UIID_HWACCELERATOR__ { 0xd0af43f1, 0x7331, 0x4e42, 0xbdeb, \
                                    { 0x7d, 0xa1, 0xd4, 0x2e, 0x5c, 0x74 } }
static const effect_uuid_t EFFECT_UIID_HWACCELERATOR_ = EFFECT_UIID_HWACCELERATOR__;
const effect_uuid_t * const EFFECT_UIID_HWACCELERATOR = &EFFECT_UIID_HWACCELERATOR_;

typedef enum
{
    HW_ACCELERATOR_FD,
    HW_ACCELERATOR_HPX_STATE
} t_hw_accelerator_params;

#if __cplusplus
}  // extern "C"
#endif


#endif /*ANDROID_EFFECT_HWACCELERATOR_H_*/
