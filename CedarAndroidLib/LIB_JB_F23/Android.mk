LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

include $(TOP)/external/cedarx/Config.mk

LOCAL_PREBUILT_LIBS := libcedarxosal.so libcedarv.so libcedarv_base.so libcedarv_adapter.so libve.so 
LOCAL_PREBUILT_LIBS += libfacedetection.so
LOCAL_PREBUILT_LIBS += libcedarxbase.so libaw_audio.so libaw_audioa.so libswdrm.so libstagefright_soft_cedar_h264dec.so librtmp.so

LOCAL_MODULE_TAGS := optional

include $(BUILD_MULTI_PREBUILT)
