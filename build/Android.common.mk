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

ifndef ANDROID_COMMON_MK
ANDROID_COMMON_MK = true

ART_TARGET_SUPPORTED_ARCH := arm arm64 mips x86 x86_64
ART_HOST_SUPPORTED_ARCH := x86 x86_64

ifeq (,$(filter $(TARGET_ARCH),$(ART_TARGET_SUPPORTED_ARCH)))
$(warning unsupported TARGET_ARCH=$(TARGET_ARCH))
endif
ifeq (,$(filter $(HOST_ARCH),$(ART_HOST_SUPPORTED_ARCH)))
$(warning unsupported HOST_ARCH=$(HOST_ARCH))
endif

# Primary vs. secondary
2ND_TARGET_ARCH := $(TARGET_2ND_ARCH)
ifdef TARGET_2ND_ARCH
  ifneq ($(filter %64,$(TARGET_ARCH)),)
    ART_PHONY_TEST_TARGET_SUFFIX := 64
    2ND_ART_PHONY_TEST_TARGET_SUFFIX := 32
    ART_TARGET_ARCH_32 := $(TARGET_2ND_ARCH)
    ART_TARGET_ARCH_64 := $(TARGET_ARCH)
  else
    # TODO: ???
    $(error Do not know what to do with this multi-target configuration!)
  endif
else
  ART_PHONY_TEST_TARGET_SUFFIX := 32
  2ND_ART_PHONY_TEST_TARGET_SUFFIX :=
  ART_TARGET_ARCH_32 := $(TARGET_ARCH)
  ART_TARGET_ARCH_64 :=
endif

ifeq ($(HOST_PREFER_32_BIT),true)
  ART_PHONY_TEST_HOST_SUFFIX := 32
  2ND_ART_PHONY_TEST_HOST_SUFFIX :=
  ART_HOST_ARCH_32 := x86
  ART_HOST_ARCH_64 :=
  ART_HOST_ARCH := x86
  2ND_ART_HOST_ARCH :=
  2ND_HOST_ARCH :=
else
  ART_PHONY_TEST_HOST_SUFFIX := 64
  2ND_ART_PHONY_TEST_HOST_SUFFIX := 32
  ART_HOST_ARCH_32 := x86
  ART_HOST_ARCH_64 := x86_64
  ART_HOST_ARCH := x86_64
  2ND_ART_HOST_ARCH := x86
  2ND_HOST_ARCH := x86
endif

endif # ANDROID_COMMON_MK
