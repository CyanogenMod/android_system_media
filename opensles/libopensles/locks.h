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

extern void object_lock_exclusive(IObject *this);
extern void object_unlock_exclusive(IObject *this);
extern void object_cond_wait(IObject *this);
extern void object_cond_signal(IObject *this);

// Currently shared locks are implemented as exclusive, but don't count on it

#define object_lock_shared(this)   object_lock_exclusive(this)
#define object_unlock_shared(this) object_unlock_exclusive(this)

// Currently interface locks are actually on whole object, but don't count on it

#define interface_lock_exclusive(this)   object_lock_exclusive((this)->mThis)
#define interface_unlock_exclusive(this) object_unlock_exclusive((this)->mThis)
#define interface_lock_shared(this)      object_lock_shared((this)->mThis)
#define interface_unlock_shared(this)    object_unlock_shared((this)->mThis)
#define interface_cond_wait(this)        object_cond_wait((this)->mThis)
#define interface_cond_signal(this)      object_cond_signal((this)->mThis)

// Peek and poke are an optimization for small atomic fields that don't "matter"

#define interface_lock_poke(this)   /* interface_lock_exclusive(this) */
#define interface_unlock_poke(this) /* interface_unlock_exclusive(this) */
#define interface_lock_peek(this)   /* interface_lock_shared(this) */
#define interface_unlock_peek(this) /* interface_unlock_shared(this) */
