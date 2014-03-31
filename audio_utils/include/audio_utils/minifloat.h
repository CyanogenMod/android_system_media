/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_MINIFLOAT_H
#define ANDROID_AUDIO_MINIFLOAT_H

#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* Convert a float to the internal representation used for attenuations.
 * The nominal range [0.0, 1.0], but the hard range is [0.0, 2.0).
 * Negative and underflow values are converted to 0.0,
 * and values larger than the hard maximum are truncated to the hard maximum.
 *
 * Minifloats are ordered, and standard comparisons may be used between them
 * in the uint16_t representation.
 *
 * Details on internal representation of attenuations, based on mini-floats:
 * The nominal maximum is 1.0 and the hard maximum is 1 lsb less than 2.0.
 * Negative numbers, infinity, and NaN are not supported.
 * There are 13 significand bits specified, 1 implied hidden bit, 3 exponent bits,
 * and no sign bit.  Denormals are supported.
 */
uint16_t attenuation_from_float(float f);

/* Convert the internal representation used for attenuation to float */
float float_from_attenuation(uint16_t attenuation);

__END_DECLS

#endif  // ANDROID_AUDIO_MINIFLOAT_H
