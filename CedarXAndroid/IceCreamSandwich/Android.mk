LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(TOP)/external/cedarx/Config.mk

LOCAL_SRC_FILES:=                    \
        CedarXAudioPlayer.cpp        \
        CedarXMetadataRetriever.cpp  \
        CedarXMetaData.cpp           \
        CedarXSoftwareRenderer.cpp   \
        CedarXRecorder.cpp           \
        CedarXPlayer.cpp             \
        CedarXNativeRenderer.cpp

LOCAL_C_INCLUDES += \
    $(JNI_H_INCLUDE) \
    $(TOP)/frameworks/${AV_BASE_PATH}/include/media/stagefright \
    $(CEDARX_TOP)/include \
    $(CEDARX_TOP)/libutil \
    $(CEDARX_TOP)/include/include_cedarv \
    $(CEDARX_TOP)/include/include_audio \
    ${CEDARX_TOP}/include/include_camera \
    ${CEDARX_TOP}/include/include_omx \
    $(TOP)/frameworks/${AV_BASE_PATH}/media/libstagefright/include \
    $(TOP)/frameworks/${AV_BASE_PATH} \
    $(TOP)/frameworks/${AV_BASE_PATH}/include \
    $(TOP)/external/openssl/include \
    $(TOP)/frameworks/native/include/media/hardware 

LOCAL_SHARED_LIBRARIES := \
        libbinder         \
        libmedia          \
        libutils          \
        libcutils         \
        libui             \
        libgui		  \
        libcamera_client  \
        libstagefright_foundation \
        libicuuc \
        libskia  \
        libdl \
        libstagefright_foundation \
        libstagefright \
        libdrmframework \

LOCAL_LDFLAGS += \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxplayer.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxcomponents.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxdemuxers.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxsftdemux.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxwvmdemux.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxrender.a	\
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libsub.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libsub_inline.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libiconv.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libh264enc.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libmuxers.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libmp4_muxer.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libawts_muxer.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libraw_muxer.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libjpgenc.a	\
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libuserdemux.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxstream.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libthirdpartstream.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_cedarm.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_asf.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_avi.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_flv.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_mkv.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_ogg.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_mov.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_mpg.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_ts.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_pmp.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_idxsub.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libdemux_awts.a\
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libaacenc.a \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxbase.so \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libswdrm.so \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarv.so \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarxosal.so \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libve.so \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarv_base.so \
	$(CEDARX_TOP)/../CedarAndroidLib/LIB_$(CEDARX_ANDROID_CODE)_$(CEDARX_CHIP_VERSION)/libcedarv_adapter.so \
        $(OUT)/obj/STATIC_LIBRARIES/libstagefright_rtsp_intermediates/libstagefright_rtsp.a \

LOCAL_CFLAGS := $(CEDARX_EXT_CFLAGS) -Wno-multichar -D__ANDROID_VERSION_2_3_4
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libCedarX

include $(BUILD_SHARED_LIBRARY)

#include $(call all-makefiles-under,$(LOCAL_PATH))
