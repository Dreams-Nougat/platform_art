#
# Copyright (C) 2014 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

include art/build/Android.common_path.mk

# Copy the art shell script to the host's bin directory
include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := art
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): \
	$(HOST_OUT_EXECUTABLES)/dalvikvm \
	$(HOST_OUT_EXECUTABLES)/dalvikvm32 \
	$(HOST_OUT_EXECUTABLES)/dalvikvm64 \
	$(HOST_OUT_SHARED_LIBRARIES)/libart.so \
	$(HOST_CORE_IMG_OUT_BASE)-optimizing-pic$(CORE_IMG_SUFFIX)
$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/art $(ACP)
	@echo "Copy: $(PRIVATE_MODULE) ($@)"
	$(copy-file-to-new-target)
	$(hide) chmod 755 $@

# Copy the art shell script to the target's bin directory
include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := art
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/art $(ACP)
	@echo "Copy: $(PRIVATE_MODULE) ($@)"
	$(copy-file-to-new-target)
	$(hide) chmod 755 $@
