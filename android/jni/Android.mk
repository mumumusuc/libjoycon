LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := joycon
LOCAL_LDLIBS += -ldl -llog
LOCAL_CPPFLAGS += -D__android__=1
LOCAL_CPPFLAGS += -std=c++11 -fvisibility=hidden -fexceptions
LOCAL_CPPFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-mismatched-tags -Wno-unused-private-field -Wno-missing-braces
LOCAL_C_INCLUDES := ../../include
LOCAL_SRC_FILES := 			\
	../../src/session2.cc 		\
	../../src/controller.cc	\
	joycon_jni.cc
include $(BUILD_SHARED_LIBRARY)
