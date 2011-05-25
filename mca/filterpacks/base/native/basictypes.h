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

#ifndef BASE_BASIC_TYPES_H_
#define BASE_BASIC_TYPES_H_

#include <stdint.h>

// STL containers
#include <vector>
using std::vector;
#include <map>
using std::map;
#include <string>
using std::string;
#include <sstream>
using std::stringstream;
using std::istringstream;
using std::ostringstream;
#include <utility>
using std::pair;
#include <list>
using std::list;
#include <set>
using std::set;
#include <stack>
using std::stack;

#include <memory>
using std::auto_ptr;

// Integral types
typedef int8_t   int8;
typedef uint8_t  uint8;
typedef int16_t  int16;
typedef uint16_t uint16;
typedef int32_t  int32;
typedef uint32_t uint32;
typedef int64_t  int64;
typedef uint64_t uint64;

#endif // BASE_BASIC_TYPES_H_

