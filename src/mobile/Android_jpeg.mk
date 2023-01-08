
LOCAL_PATH := $(call my-dir)/../../deps/jpeg-6b

include $(CLEAR_VARS)

LOCAL_MODULE := jpeg6b

LOCAL_CFLAGS = -Wall

LOCAL_C_INCLUDES :=

LOCAL_SRC_FILES =  \
    jcomapi.c \
    jdapimin.c \
    jdapistd.c \
    jdatasrc.c \
    jdcoefct.c \
    jdcolor.c \
    jddctmgr.c \
    jdhuff.c \
    jdinput.c \
    jdmainct.c \
    jdmarker.c \
    jdmaster.c \
    jdmerge.c \
    jdphuff.c \
    jdpostct.c \
    jdsample.c \
    jerror.c \
    jidctint.c \
    jmemmgr.c \
    jutils.c

LOCAL_CFLAGS += -fvisibility=hidden -fdata-sections -ffunction-sections

include $(BUILD_STATIC_LIBRARY)








