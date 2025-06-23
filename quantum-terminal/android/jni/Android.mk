LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := quantum-terminal
LOCAL_SRC_FILES := \
    ../../src/terminal.c \
    ../../src/pty.c \
    ../../src/renderer.c \
    ../../src/quantum.c \
    ../../src/input.c \
    ../../src/platform/android.c \
    ../../src/platform/gles_renderer.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../include \
    $(LOCAL_PATH)/../../lib

LOCAL_CFLAGS := -O3 -Wall -DANDROID -DGLES3
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -lm

include $(BUILD_SHARED_LIBRARY)