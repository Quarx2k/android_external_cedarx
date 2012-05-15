LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include frameworks/base/media/libstagefright/codecs/common/Config.mk
include frameworks/base/media/CedarX-Projects/Config.mk

CEDARA_VERSION_TAG = _ICS_

LOCAL_SRC_FILES:=                \
                 CedarARender.cpp \
                 CedarAPlayer.cpp				  


LOCAL_C_INCLUDES:= \
        $(JNI_H_INCLUDE) \
        $(LOCAL_PATH)/include \
        ${CEDARX_TOP}/libcodecs/include \
        $(TOP)/frameworks/base/include/media/stagefright \
        $(TOP)/frameworks/base/include/media/stagefright/openmax

LOCAL_SHARED_LIBRARIES := \
        libbinder         \
        libmedia          \
        libutils          \
        libcutils         \
        libui

ifneq ($(CEDARX_DEBUG_ENABLE),Y)
LOCAL_LDFLAGS += \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libcedara_decoder.a
endif

LOCAL_LDFLAGS += \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libac3.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libdts.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libwma.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libaac.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libmp3.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libatrc.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libcook.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libsipr.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libamr.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libape.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libogg.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libflac.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libwav.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libGetAudio_format.a \
	$(LOCAL_PATH)/../CedarAndroidLib/LIB$(CEDARA_VERSION_TAG)$(CEDAR_AUDIOLIB_PATH)/libaacenc.a

ifeq ($(CEDARX_DEBUG_ENABLE),Y)
LOCAL_STATIC_LIBRARIES += \
	libcedara_decoder
endif

ifeq ($(CEDARX_ENABLE_MEMWATCH),Y)
LOCAL_STATIC_LIBRARIES += libmemwatch
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
        LOCAL_LDLIBS += -lpthread -ldl
        LOCAL_SHARED_LIBRARIES += libdvm
        LOCAL_CPPFLAGS += -DANDROID_SIMULATOR
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
        LOCAL_LDLIBS += -lpthread
endif

LOCAL_CFLAGS += -Wno-multichar 

LOCAL_CFLAGS += $(CEDARX_EXT_CFLAGS)
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libCedarA

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
