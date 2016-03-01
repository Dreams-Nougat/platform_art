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

# --- dexfuzz.jar ----------------
include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_JAR_MANIFEST := manifest.txt
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := dexfuzz
include $(BUILD_HOST_JAVA_LIBRARY)

# --- dexfuzz script ----------------
include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := dexfuzz
LOCAL_SRC_FILES := dexfuzz
LOCAL_POST_INSTALL_CMD = $(hide) chmod +x $(LOCAL_INSTALLED_MODULE)
include $(BUILD_PREBUILT)

# --- dexfuzz script with core image dependencies ----------------
fuzzer: $(LOCAL_BUILT_MODULE) $(HOST_CORE_IMG_OUTS)
