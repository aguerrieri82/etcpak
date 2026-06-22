LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := etcpack

LOCAL_CFLAGS +=$(ANDROID_C_FLAGS)
LOCAL_CFLAGS += -Wno-implicit-function-declaration

LOCAL_CPPFLAGS +=$(ANDROID_CPP_FLAGS)
LOCAL_CPPFLAGS += -D__ARM_NEON

LOCAL_LDFLAGS += $(ANDROID_LD_FLAGS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../zlib \
				    $(LOCAL_PATH)/../libpng

LOCAL_SRC_FILES	:= 	$(wildcard $(LOCAL_PATH)/../../zlib/*.c) \
					$(wildcard $(LOCAL_PATH)/../*.cpp) \
			        $(wildcard $(LOCAL_PATH)/../*.c) \
					$(wildcard $(LOCAL_PATH)/../libpng/*.c) \
					$(wildcard $(LOCAL_PATH)/../getopt/*.c) \
					$(wildcard $(LOCAL_PATH)/../lz4/*.c) 
					
LOCAL_SRC_FILES := $(filter-out \
	$(LOCAL_PATH)/../libpng/pngtest.c \
	$(LOCAL_PATH)/../Application.cpp \
   ,$(LOCAL_SRC_FILES))

include $(BUILD_SHARED_LIBRARY)
