/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef SYSTEM_CORE_INCLUDE_ANDROID_CAMERA_METADATA_H
#define SYSTEM_CORE_INCLUDE_ANDROID_CAMERA_METADATA_H

#include <string.h>
#include <stdint.h>
#include <cutils/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tag hierarchy and enum definitions for camera_metadata_entry
 * =============================================================================
 */

/**
 * Main enum definitions are in a separate file to make it easy to
 * maintain
 */
#include "camera_metadata_tags.h"

/**
 * Enum range for each top-level category
 */
ANDROID_API
extern unsigned int camera_metadata_section_bounds[ANDROID_SECTION_COUNT][2];
ANDROID_API
extern const char *camera_metadata_section_names[ANDROID_SECTION_COUNT];

/**
 * Type definitions for camera_metadata_entry
 * =============================================================================
 */
enum {
    // Unsigned 8-bit integer (uint8_t)
    TYPE_BYTE = 0,
    // Signed 32-bit integer (int32_t)
    TYPE_INT32 = 1,
    // 32-bit float (float)
    TYPE_FLOAT = 2,
    // Signed 64-bit integer (int64_t)
    TYPE_INT64 = 3,
    // 64-bit float (double)
    TYPE_DOUBLE = 4,
    // A 64-bit fraction (camera_metadata_rational_t)
    TYPE_RATIONAL = 5,
    // Number of type fields
    NUM_TYPES
};

typedef struct camera_metadata_rational {
    int32_t numerator;
    int32_t denominator;
} camera_metadata_rational_t;

/**
 * Size in bytes of each entry type
 */
ANDROID_API
extern size_t camera_metadata_type_sizes[NUM_TYPES];

/**
 * Main definitions for the metadata entry and array structures
 * =============================================================================
 */

/**
 * A packet of metadata. This is a list of metadata entries, each of which has
 * an integer tag to identify its meaning, 'type' and 'count' field, and the
 * data, which contains a 'count' number of entries of type 'type'. The packet
 * has a fixed capacity for entries and for extra data.  A new entry uses up one
 * entry slot, and possibly some amount of data capacity; the function
 * calculate_camera_metadata_entry_data_size() provides the amount of data
 * capacity that would be used up by an entry.
 *
 * Entries are not sorted, and are not forced to be unique - multiple entries
 * with the same tag are allowed. The packet will not dynamically resize when
 * full.
 *
 * The packet is contiguous in memory, with size in bytes given by
 * get_camera_metadata_size(). Therefore, it can be copied safely with memcpy()
 * to a buffer of sufficient size. The copy_camera_metadata() function is
 * intended for eliminating unused capacity in the destination packet.
 */
struct camera_metadata;
typedef struct camera_metadata camera_metadata_t;

/**
 * Functions for manipulating camera metadata
 * =============================================================================
 */

/**
 * Allocate a new camera_metadata structure, with some initial space for entries
 * and extra data. The entry_capacity is measured in entry counts, and
 * data_capacity in bytes. The resulting structure is all contiguous in memory,
 * and can be freed with free_camera_metadata().
 */
ANDROID_API
camera_metadata_t *allocate_camera_metadata(size_t entry_capacity,
        size_t data_capacity);

/**
 * Place a camera metadata structure into an existing buffer. Returns NULL if
 * the buffer is too small for the requested number of reserved entries and
 * bytes of data. The entry_capacity is measured in entry counts, and
 * data_capacity in bytes. If the buffer is larger than the required space,
 * unused space will be left at the end. If successful, returns a pointer to the
 * metadata header placed at the start of the buffer. It is the caller's
 * responsibility to free the original buffer; do not call
 * free_camera_metadata() with the returned pointer.
 */
ANDROID_API
camera_metadata_t *place_camera_metadata(void *dst, size_t dst_size,
        size_t entry_capacity,
        size_t data_capacity);

/**
 * Free a camera_metadata structure. Should only be used with structures
 * allocated with allocate_camera_metadata().
 */
ANDROID_API
void free_camera_metadata(camera_metadata_t *metadata);

/**
 * Calculate the buffer size needed for a metadata structure of entry_count
 * metadata entries, needing a total of data_count bytes of extra data storage.
 */
ANDROID_API
size_t calculate_camera_metadata_size(size_t entry_count,
        size_t data_count);

/**
 * Get current size of entire metadata structure in bytes, including reserved
 * but unused space.
 */
ANDROID_API
size_t get_camera_metadata_size(const camera_metadata_t *metadata);

/**
 * Get size of entire metadata buffer in bytes, not including reserved but
 * unused space. This is the amount of space needed by copy_camera_metadata for
 * its dst buffer.
 */
ANDROID_API
size_t get_camera_metadata_compact_size(const camera_metadata_t *metadata);

/**
 * Get the current number of entries in the metadata packet.
 */
ANDROID_API
size_t get_camera_metadata_entry_count(const camera_metadata_t *metadata);

/**
 * Get the maximum number of entries that could fit in the metadata packet.
 */
ANDROID_API
size_t get_camera_metadata_entry_capacity(const camera_metadata_t *metadata);

/**
 * Get the current count of bytes used for value storage in the metadata packet.
 */
ANDROID_API
size_t get_camera_metadata_data_count(const camera_metadata_t *metadata);

/**
 * Get the maximum count of bytes that could be used for value storage in the
 * metadata packet.
 */
ANDROID_API
size_t get_camera_metadata_data_capacity(const camera_metadata_t *metadata);

/**
 * Copy a metadata structure to a memory buffer, compacting it along the
 * way. That is, in the copied structure, entry_count == entry_capacity, and
 * data_count == data_capacity.
 *
 * If dst_size > get_camera_metadata_compact_size(), the unused bytes are at the
 * end of the buffer. If dst_size < get_camera_metadata_compact_size(), returns
 * NULL. Otherwise returns a pointer to the metadata structure header placed at
 * the start of dst.
 *
 * Since the buffer was not allocated by allocate_camera_metadata, the caller is
 * responsible for freeing the underlying buffer when needed; do not call
 * free_camera_metadata.
 */
ANDROID_API
camera_metadata_t *copy_camera_metadata(void *dst, size_t dst_size,
        const camera_metadata_t *src);

/**
 * Append camera metadata in src to an existing metadata structure in dst.  This
 * does not resize the destination structure, so if it is too small, a non-zero
 * value is returned. On success, 0 is returned. Appending onto a sorted
 * structure results in a non-sorted combined structure.
 */
ANDROID_API
int append_camera_metadata(camera_metadata_t *dst, const camera_metadata_t *src);

/**
 * Calculate the number of bytes of extra data a given metadata entry will take
 * up. That is, if entry of 'type' with a payload of 'data_count' values is
 * added, how much will the value returned by get_camera_metadata_data_count()
 * be increased? This value may be zero, if no extra data storage is needed.
 */
ANDROID_API
size_t calculate_camera_metadata_entry_data_size(uint8_t type,
        size_t data_count);

/**
 * Add a metadata entry to a metadata structure. Returns 0 if the addition
 * succeeded. Returns a non-zero value if there is insufficient reserved space
 * left to add the entry, or if the tag is unknown.  data_count is the number of
 * entries in the data array of the tag's type, not a count of
 * bytes. Vendor-defined tags can not be added using this method, unless
 * set_vendor_tag_query_ops() has been called first. Entries are always added to
 * the end of the structure (highest index), so after addition, a
 * previously-sorted array will be marked as unsorted.
 */
ANDROID_API
int add_camera_metadata_entry(camera_metadata_t *dst,
        uint32_t tag,
        const void *data,
        size_t data_count);

/**
 * Sort the metadata buffer for fast searching. If already sorted, does
 * nothing. Adding or appending entries to the buffer will place the buffer back
 * into an unsorted state.
 */
ANDROID_API
int sort_camera_metadata(camera_metadata_t *dst);

/**
 * Get pointers to the fields for a metadata entry at position index in the
 * entry array.  The data pointer points either to the entry's data.value field
 * or to the right offset in camera_metadata_t.data. Returns 0 on
 * success. Data_count is the number of entries in the data array when cast to
 * the tag's type, not a count of bytes.
 *
 * src and index are inputs; tag, type, data, and data_count are outputs. Any of
 * the outputs can be set to NULL to skip reading that value.
 */
ANDROID_API
int get_camera_metadata_entry(camera_metadata_t *src,
        uint32_t index,
        uint32_t *tag,
        uint8_t *type,
        void **data,
        size_t *data_count);

/**
 * Find an entry with given tag value. If not found, returns -ENOENT. Otherwise,
 * returns entry contents like get_camera_metadata_entry. Any of
 * the outputs can be set to NULL to skip reading that value.
 *
 * Note: Returns only the first entry with a given tag. To speed up searching
 * for tags, sort the metadata structure first by calling
 * sort_camera_metadata().
 */
ANDROID_API
int find_camera_metadata_entry(camera_metadata_t *src,
        uint32_t tag,
        uint8_t *type,
        void **data,
        size_t *data_count);

/**
 * Retrieve human-readable name of section the tag is in. Returns NULL if
 * no such tag is defined. Returns NULL for tags in the vendor section, unless
 * set_vendor_tag_query_ops() has been used.
 */
ANDROID_API
const char *get_camera_metadata_section_name(uint32_t tag);

/**
 * Retrieve human-readable name of tag (not including section). Returns NULL if
 * no such tag is defined. Returns NULL for tags in the vendor section, unless
 * set_vendor_tag_query_ops() has been used.
 */
ANDROID_API
const char *get_camera_metadata_tag_name(uint32_t tag);

/**
 * Retrieve the type of a tag. Returns -1 if no such tag is defined. Returns -1
 * for tags in the vendor section, unless set_vendor_tag_query_ops() has been
 * used.
 */
ANDROID_API
int get_camera_metadata_tag_type(uint32_t tag);

/**
 * Set up vendor-specific tag query methods. These are needed to properly add
 * entries with vendor-specified tags and to use the
 * get_camera_metadata_section_name, _tag_name, and _tag_type methods with
 * vendor tags. Returns 0 on success.
 */
typedef struct vendor_tag_query_ops vendor_tag_query_ops_t;
struct vendor_tag_query_ops {
    /**
     * Get vendor section name for a vendor-specified entry tag. Only called for
     * tags >= 0x80000000. The section name must start with the name of the
     * vendor in the Java package style. For example, CameraZoom inc must prefix
     * their sections with "com.camerazoom." Must return NULL if the tag is
     * outside the bounds of vendor-defined sections.
     */
    const char *(*get_camera_vendor_section_name)(
        const vendor_tag_query_ops_t *v,
        uint32_t tag);
    /**
     * Get tag name for a vendor-specified entry tag. Only called for tags >=
     * 0x80000000. Must return NULL if the tag is outside the bounds of
     * vendor-defined sections.
     */
    const char *(*get_camera_vendor_tag_name)(
        const vendor_tag_query_ops_t *v,
        uint32_t tag);
    /**
     * Get tag type for a vendor-specified entry tag. Only called for tags >=
     * 0x80000000. Must return -1 if the tag is outside the bounds of
     * vendor-defined sections.
     */
    int (*get_camera_vendor_tag_type)(
        const vendor_tag_query_ops_t *v,
        uint32_t tag);
};

ANDROID_API
int set_camera_metadata_vendor_tag_ops(const vendor_tag_query_ops_t *query_ops);

/**
 * Print fields in the metadata to the log.
 * verbosity = 0: Only tag entry information
 * verbosity = 1: Tag entry information plus at most 16 data values
 * verbosity = 2: All information
 */
ANDROID_API
void dump_camera_metadata(const camera_metadata_t *metadata,
        int fd,
        int verbosity);

#ifdef __cplusplus
}
#endif

#endif
