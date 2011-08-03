# -*- mode: makefile -*-
#
# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# List of mobile filter framework directories to include in documentation/API/SDK.
# Shared between mobile filter framework and frameworks/base.

define libfilterfw-all-java-files-under
$(patsubst ./%,%, \
  $(shell cd $(1) ; \
          find $(2) -name "*.java" -and -not -name ".*") \
 )
endef

# List of mobile filter framework source files to include in the public API/SDK.
#
#
# $(1): directory for search (to support use from frameworks/base)
define libfilterfw_to_document
 $(call libfilterfw-all-java-files-under,$(1), \
   filterfw/java \
   effect/java \
   filterpacks/imageproc/java \
   filterpacks/numeric/java \
   filterpacks/performance/java \
   filterpacks/text/java \
   filterpacks/ui/java \
   filterpacks/videosrc/java \
   filterpacks/videoproc/java \
   filterpacks/videosink/java )
endef
