LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := libcedarxosal.so libcedarv.so 
ifeq ($(CEDARX_DEBUG_ENABLE),N)
LOCAL_PREBUILT_LIBS += libcedarxbase.so libswdrm.so libcedarxsftdemux.so
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)
