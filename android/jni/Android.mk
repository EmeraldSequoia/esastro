LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := esastro
LOCAL_CFLAGS    := -Werror -DES_ANDROID
LOCAL_SRC_FILES := \
../../src/ESAstronomy.cpp \
../../src/ESAstronomyCache.cpp \
../../Willmann-Bell/ESWillmannBell.cpp \

# Leave a blank line before this one
LOCAL_LDLIBS    := -llog
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../src ../../deps/esutil/src ../../deps/estime/src ../../deps/eslocation/src

include $(BUILD_STATIC_LIBRARY)
