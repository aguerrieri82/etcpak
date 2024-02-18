#------------


LOCAL_PATH := $(call my-dir)


NDK_PROJECT_PATH := $(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE := etcpack

LOCAL_CFLAGS += -Wno-implicit-function-declaration
LOCAL_CPPFLAGS += -D__ARM_NEON


LOCAL_C_INCLUDES := $(LOCAL_PATH)/../zlib

LOCAL_SRC_FILES	:= 	$(wildcard $(LOCAL_PATH)/../zlib/*.c) \
					$(wildcard $(LOCAL_PATH)/../*.cpp) \
					$(wildcard $(LOCAL_PATH)/../libpng/*.c) \
					$(wildcard $(LOCAL_PATH)/../getopt/*.c) \
					$(wildcard $(LOCAL_PATH)/../lz4/*.c) 
					

LOCAL_LDLIBS := -lc++_static -lc++abi

include $(BUILD_SHARED_LIBRARY)
