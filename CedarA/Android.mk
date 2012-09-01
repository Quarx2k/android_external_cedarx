LOCAL_PATH:= $(call my-dir)
include $(LOCAL_PATH)/../Config.mk
CEDARA_VERSION_TAG = _$(CEDARX_ANDROID_CODE)_

#######################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        CedarARender.cpp \
        CedarAPlayer.cpp                                  

LOCAL_C_INCLUDES:= \
        $(JNI_H_INCLUDE) \
        $(LOCAL_PATH)/include \
        ${CEDARX_TOP}/ \
        ${CEDARX_TOP}/libcodecs/include \
        $(TOP)/frameworks/${AV_BASE_PATH}/include/media/stagefright \
        $(TOP)/frameworks/${AV_BASE_PATH}/include/media/stagefright/openmax

LOCAL_SHARED_LIBRARIES := \
        libdl             \
        libbinder         \
        libmedia          \
        libutils          \
        libcutils         \
        libui             \
        libCedarX

LOCAL_LDFLAGS += \
        $(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDARX_CHIP_VERSION)/libcedara_decoder.a \
        $(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDARX_CHIP_VERSION)/libGetAudio_format.a \
        $(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDARX_CHIP_VERSION)/libaacenc.a

LOCAL_CFLAGS += $(CEDARX_EXT_CFLAGS) -Wno-multichar 
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libCedarA

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
