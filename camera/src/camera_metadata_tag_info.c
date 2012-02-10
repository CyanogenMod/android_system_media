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

/**
 * !! Do not reference this file directly !!
 *
 * It is logically a part of camera_metadata.c.  It is broken out for ease of
 * maintaining the tag info.
 */

const char *camera_metadata_section_names[ANDROID_SECTION_COUNT] = {
    "android.request",
    "android.control",
    "android.control.info",
    "android.sensor",
    "android.sensor.info",
    "android.lens",
    "android.lens.info",
    "android.flash",
    "android.flash.info",
    "android.hotPixel",
    "android.hotPixel.info",
    "android.demosaic",
    "android.demosaic.info",
    "android.noiseReduction",
    "android.noiseReduction.info",
    "android.shadingCorrection",
    "android.shadingCorrection.info",
    "android.geometricCorrection",
    "android.geometricCorrection.info",
    "android.colorCorrection",
    "android.colorCorrection.info",
    "android.tonemap",
    "android.tonemap.info",
    "android.edge",
    "android.edge.info",
    "android.scaler",
    "android.scaler.info",
    "android.jpeg",
    "android.jpeg.info",
    "android.statistics",
    "android.statistics.info"
};

unsigned int camera_metadata_section_bounds[ANDROID_SECTION_COUNT][2] = {
    { ANDROID_REQUEST_START,        ANDROID_REQUEST_END },
    { ANDROID_LENS_START,           ANDROID_LENS_END },
    { ANDROID_LENS_INFO_START,      ANDROID_LENS_INFO_END },
    { ANDROID_SENSOR_START,         ANDROID_SENSOR_END },
    { ANDROID_SENSOR_INFO_START,    ANDROID_SENSOR_INFO_END },
    { ANDROID_FLASH_START,          ANDROID_FLASH_END },
    { ANDROID_FLASH_INFO_START,     ANDROID_FLASH_INFO_END },
    { ANDROID_HOT_PIXEL_START,      ANDROID_HOT_PIXEL_END },
    { ANDROID_HOT_PIXEL_INFO_START, ANDROID_HOT_PIXEL_INFO_END },
    { ANDROID_DEMOSAIC_START,       ANDROID_DEMOSAIC_END },
    { ANDROID_DEMOSAIC_INFO_START,  ANDROID_DEMOSAIC_INFO_END },
    { ANDROID_NOISE_START,          ANDROID_NOISE_END },
    { ANDROID_NOISE_INFO_START,     ANDROID_NOISE_INFO_END },
    { ANDROID_SHADING_START,        ANDROID_SHADING_END },
    { ANDROID_SHADING_INFO_START,   ANDROID_SHADING_INFO_END },
    { ANDROID_GEOMETRIC_START,      ANDROID_GEOMETRIC_END },
    { ANDROID_GEOMETRIC_INFO_START, ANDROID_GEOMETRIC_INFO_END },
    { ANDROID_COLOR_START,          ANDROID_COLOR_END },
    { ANDROID_COLOR_INFO_START,     ANDROID_COLOR_INFO_END },
    { ANDROID_TONEMAP_START,        ANDROID_TONEMAP_END },
    { ANDROID_TONEMAP_INFO_START,   ANDROID_TONEMAP_INFO_END },
    { ANDROID_EDGE_START,           ANDROID_EDGE_END },
    { ANDROID_EDGE_INFO_START,      ANDROID_EDGE_INFO_END },
    { ANDROID_SCALER_START,         ANDROID_SCALER_END },
    { ANDROID_SCALER_INFO_START,    ANDROID_SCALER_INFO_END },
    { ANDROID_JPEG_START,           ANDROID_JPEG_END },
    { ANDROID_JPEG_INFO_START,      ANDROID_JPEG_INFO_END },
    { ANDROID_STATS_START,          ANDROID_STATS_END },
    { ANDROID_STATS_INFO_START,     ANDROID_STATS_INFO_END },
    { ANDROID_CONTROL_START,        ANDROID_CONTROL_END },
    { ANDROID_CONTROL_INFO_START,   ANDROID_CONTROL_INFO_END }
};

tag_info_t android_request[ANDROID_REQUEST_END -
        ANDROID_REQUEST_START] = {
    { "id",                          TYPE_INT32 },
    { "metadataMode",                TYPE_BYTE },
    { "outputStreams",               TYPE_BYTE },
    { "frameCount",                  TYPE_INT32 }
};

tag_info_t android_control[ANDROID_CONTROL_END -
        ANDROID_CONTROL_START] = {
    { "mode",                        TYPE_BYTE },
    { "aeMode",                      TYPE_BYTE },
    { "aeRegions",                   TYPE_INT32 },
    { "aeExposureCompensation",      TYPE_INT32 },
    { "aeTargetFpsRange",            TYPE_INT32 },
    { "aeAntibandingMode",           TYPE_BYTE },
    { "awbMode",                     TYPE_BYTE },
    { "awbRegions",                  TYPE_INT32 },
    { "afMode",                      TYPE_BYTE },
    { "afRegions",                   TYPE_INT32 },
    { "afTrigger",                   TYPE_BYTE },
    { "afState",                     TYPE_BYTE },
    { "videoStabilizationMode",      TYPE_BYTE }
};

tag_info_t android_control_info[ANDROID_CONTROL_INFO_END -
        ANDROID_CONTROL_INFO_START] = {
    { "availableModes",              TYPE_BYTE },
    { "maxRegions",                  TYPE_INT32 },
    { "aeAvailableModes",            TYPE_BYTE },
    { "aeCompensationStep",          TYPE_RATIONAL },
    { "aeCompensationRange",         TYPE_INT32 },
    { "aeAvailableTargetFpsRanges",  TYPE_INT32 },
    { "aeAvailableAntibandingModes", TYPE_BYTE },
    { "awbAvailableModes",           TYPE_BYTE },
    { "afAvailableModes",            TYPE_BYTE }
};

tag_info_t android_sensor[ANDROID_SENSOR_END -
        ANDROID_SENSOR_START] = {
    { "exposureTime",  TYPE_INT64 },
    { "frameDuration", TYPE_INT64 },
    { "sensitivity",   TYPE_INT32 },
    { "timestamp",     TYPE_INT64 }
};

tag_info_t android_sensor_info[ANDROID_SENSOR_INFO_END -
        ANDROID_SENSOR_INFO_START] = {
    { "exposureTimeRange",      TYPE_INT64 },
    { "maxFrameDuration",       TYPE_INT64 },
    { "sensitivityRange",       TYPE_INT32 },
    { "colorFilterArrangement", TYPE_BYTE },
    { "pixelArraySize",         TYPE_INT32 },
    { "activeArraySize",        TYPE_INT32 },
    { "whiteLevel",             TYPE_INT32 },
    { "blackLevelPattern",      TYPE_INT32 },
    { "colorTransform1",        TYPE_RATIONAL },
    { "colorTransform2",        TYPE_RATIONAL },
    { "referenceIlluminant1",   TYPE_BYTE },
    { "referenceIlluminant2",   TYPE_BYTE },
    { "forwardMatrix1",         TYPE_RATIONAL },
    { "forwardMatrix2",         TYPE_RATIONAL },
    { "calibrationTransform1",  TYPE_RATIONAL },
    { "calibrationTransform2",  TYPE_RATIONAL },
    { "baseGainFactor",         TYPE_RATIONAL },
    { "maxAnalogSensitivity",   TYPE_INT32 },
    { "noiseModelCoefficients", TYPE_FLOAT },
    { "orientation",            TYPE_INT32 }
};

tag_info_t android_lens[ANDROID_LENS_END -
        ANDROID_LENS_START] = {
    { "focusDistance",            TYPE_FLOAT },
    { "aperture",                 TYPE_FLOAT },
    { "focalLength",              TYPE_FLOAT },
    { "filterDensity",            TYPE_FLOAT },
    { "opticalStabilizationMode", TYPE_BYTE },
    { "focusRange",               TYPE_FLOAT }
};

tag_info_t android_lens_info[ANDROID_LENS_INFO_END -
        ANDROID_LENS_INFO_START] = {
    { "minimumFocusDistance",               TYPE_FLOAT },
    { "availableFocalLengths",              TYPE_FLOAT },
    { "availableApertures",                 TYPE_FLOAT },
    { "availableFilterDensities",           TYPE_FLOAT },
    { "availableOpticalStabilizationModes", TYPE_BYTE },
    { "shadingMap",                         TYPE_FLOAT },
    { "geometricCorrectionMap",             TYPE_FLOAT },
    { "facing",                             TYPE_BYTE },
    { "position",                           TYPE_FLOAT }
};

tag_info_t android_flash[ANDROID_FLASH_END -
        ANDROID_FLASH_START] = {
    { "mode",          TYPE_BYTE },
    { "firingPower",   TYPE_BYTE },
    { "firingTime",    TYPE_INT64 }
};

tag_info_t android_flash_info[ANDROID_FLASH_INFO_END -
        ANDROID_FLASH_INFO_START] = {
    { "available",      TYPE_BYTE },
    { "chargeDuration", TYPE_INT64 },
};

tag_info_t android_hot_pixel[ANDROID_HOT_PIXEL_END -
        ANDROID_HOT_PIXEL_START] = {
    { "mode",           TYPE_BYTE }
};

tag_info_t android_hot_pixel_info[ANDROID_HOT_PIXEL_INFO_END -
        ANDROID_HOT_PIXEL_INFO_START];

tag_info_t android_demosaic[ANDROID_DEMOSAIC_END -
        ANDROID_DEMOSAIC_START] = {
    { "mode",          TYPE_BYTE }
};

tag_info_t android_demosaic_info[ANDROID_DEMOSAIC_INFO_END -
        ANDROID_DEMOSAIC_INFO_START];

tag_info_t android_noise[ANDROID_NOISE_END -
        ANDROID_NOISE_START] = {
    { "mode",          TYPE_BYTE },
    { "strength",      TYPE_BYTE }
};

tag_info_t android_noise_info[ANDROID_NOISE_INFO_END -
        ANDROID_NOISE_INFO_START];

tag_info_t android_shading[ANDROID_SHADING_END -
        ANDROID_SHADING_START] = {
    { "mode",          TYPE_BYTE }
};

tag_info_t android_shading_info[ANDROID_SHADING_INFO_END -
        ANDROID_SHADING_INFO_START];

tag_info_t android_geometric[ANDROID_GEOMETRIC_END -
        ANDROID_GEOMETRIC_START] = {
    { "mode",          TYPE_BYTE }
};

tag_info_t android_geometric_info[ANDROID_GEOMETRIC_INFO_END -
        ANDROID_GEOMETRIC_INFO_START];

tag_info_t android_color[ANDROID_COLOR_END -
        ANDROID_COLOR_START] = {
    { "mode",          TYPE_BYTE },
    { "transform",     TYPE_FLOAT }
};

tag_info_t android_color_info[ANDROID_COLOR_INFO_END -
        ANDROID_COLOR_INFO_START] = {
    { "availableModes", TYPE_INT32 }
};

tag_info_t android_tonemap[ANDROID_TONEMAP_END -
        ANDROID_TONEMAP_START] = {
    { "mode",          TYPE_BYTE },
    { "curveRed",      TYPE_FLOAT },
    { "curveGreen",    TYPE_FLOAT },
    { "curveBlue",     TYPE_FLOAT }
};

tag_info_t android_tonemap_info[ANDROID_TONEMAP_INFO_END -
        ANDROID_TONEMAP_INFO_START] = {
    { "maxCurvePoints", TYPE_INT32 }
};

tag_info_t android_edge[ANDROID_EDGE_END -
        ANDROID_EDGE_START] = {
    { "mode",          TYPE_BYTE },
    { "strength",      TYPE_BYTE }
};

tag_info_t android_edge_info[ANDROID_EDGE_INFO_END -
        ANDROID_EDGE_INFO_START];

tag_info_t android_scaler[ANDROID_SCALER_END -
        ANDROID_SCALER_START] = {
    { "size",          TYPE_INT32 },
    { "format",        TYPE_BYTE },
    { "cropRegion",    TYPE_INT32 },
    { "rotation",      TYPE_INT32 },
};

tag_info_t android_scaler_info[ANDROID_SCALER_INFO_END -
        ANDROID_SCALER_INFO_START] = {
    { "availableFormats",            TYPE_INT32 },
    { "availableSizesPerFormat",     TYPE_INT32 },
    { "availableSizes",              TYPE_INT32 },
    { "availableMinFrameDurations",  TYPE_INT32 },
    { "availableMaxDigitalZoom",     TYPE_INT32 }
};

tag_info_t android_jpeg[ANDROID_JPEG_END -
        ANDROID_JPEG_START] = {
    { "quality",             TYPE_INT32 },
    { "thumbnailSize",       TYPE_INT32 },
    { "thumbnailQuality",    TYPE_INT32 },
    { "gpsCoordinates",      TYPE_DOUBLE },
    { "gpsProcessingMethod", TYPE_BYTE },
    { "gpsTimestamp",        TYPE_INT64 },
    { "orientation",         TYPE_INT32 }
};

tag_info_t android_jpeg_info[ANDROID_JPEG_INFO_END -
        ANDROID_JPEG_INFO_START] = {
    { "availableThumbnailSizes", TYPE_INT32 }
};

tag_info_t android_stats[ANDROID_STATS_END -
        ANDROID_STATS_START] = {
    { "faceDetectMode",      TYPE_BYTE },
    { "faceRectangles",      TYPE_INT32 },
    { "faceScores",          TYPE_BYTE },
    { "faceLandmarks",       TYPE_INT32 },
    { "faceIds",             TYPE_INT32 },
    { "histogramMode",       TYPE_BYTE },
    { "histogram",           TYPE_INT32 },
    { "sharpnessMapMode",    TYPE_BYTE },
    { "sharpnessMap",        TYPE_INT32 }
};

tag_info_t android_stats_info[ANDROID_STATS_INFO_END -
        ANDROID_STATS_INFO_START] = {
    { "availableFaceDetectModes",    TYPE_BYTE },
    { "maxFaceCount",                TYPE_INT32 },
    { "histogramBucketCount",        TYPE_INT32 },
    { "maxHistogramCount",           TYPE_INT32 },
    { "sharpnessMapSize",            TYPE_INT32 },
    { "maxSharpnessMapValue",        TYPE_INT32 }
};

tag_info_t *tag_info[ANDROID_SECTION_COUNT] = {
    android_request,
    android_lens,
    android_lens_info,
    android_sensor,
    android_sensor_info,
    android_flash,
    android_flash_info,
    android_hot_pixel,
    android_hot_pixel_info,
    android_demosaic,
    android_demosaic_info,
    android_noise,
    android_noise_info,
    android_shading,
    android_shading_info,
    android_geometric,
    android_geometric_info,
    android_color,
    android_color_info,
    android_tonemap,
    android_tonemap_info,
    android_edge,
    android_edge_info,
    android_scaler,
    android_scaler_info,
    android_jpeg,
    android_jpeg_info,
    android_stats,
    android_stats_info,
    android_control,
    android_control_info
};
