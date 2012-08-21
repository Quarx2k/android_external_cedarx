LOCAL_PATH:= $(call my-dir)

include $(TOP)/external/cedarx/Config.mk

ifneq ($(CEDARX_DEBUG_CEDARV),Y)
ifeq ($(CEDARX_ANDROID_VERSION),4)
include $(LOCAL_PATH)/LIB_F23/Android.mk
else
include $(LOCAL_PATH)/LIB_$(CEDARX_ANDROID_CODE)_F23/Android.mk
endif
endif
