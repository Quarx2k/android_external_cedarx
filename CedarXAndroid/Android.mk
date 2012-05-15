LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include external/cedarx/Config.mk

include $(LOCAL_PATH)/IceCreamSandwich/Android.mk

$(info CEDARX_PRODUCTOR: $(CEDARX_PRODUCTOR))
