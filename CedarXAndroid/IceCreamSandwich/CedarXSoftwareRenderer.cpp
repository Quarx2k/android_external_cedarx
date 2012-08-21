/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CedarXSoftwareRenderer"
#include <CDX_Debug.h>

#include <binder/MemoryHeapBase.h>

#if (CEDARX_ANDROID_VERSION < 7)
#include <binder/MemoryHeapPmem.h>
#include <surfaceflinger/Surface.h>
#include <ui/android_native_buffer.h>
#else
#include <system/window.h>
#endif

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/ISurfaceTexture.h>

#include "CedarXSoftwareRenderer.h"

namespace android {

CedarXSoftwareRenderer::CedarXSoftwareRenderer(
        const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
    : mYUVMode(None),
      mNativeWindow(nativeWindow) {
    int32_t tmp;
    CHECK(meta->findInt32(kKeyColorFormat, &tmp));
    //mColorFormat = (OMX_COLOR_FORMATTYPE)tmp;

    //CHECK(meta->findInt32(kKeyScreenID, &screenID));
    //CHECK(meta->findInt32(kKeyColorFormat, &halFormat));
    CHECK(meta->findInt32(kKeyWidth, &mWidth));
    CHECK(meta->findInt32(kKeyHeight, &mHeight));

    int32_t rotationDegrees;
    if (!meta->findInt32(kKeyRotation, &rotationDegrees)) {
        rotationDegrees = 0;
    }

    int halFormat;
    size_t bufWidth, bufHeight;

    halFormat = HAL_PIXEL_FORMAT_YV12;
    bufWidth = mWidth;
    bufHeight = mHeight;

    CHECK(mNativeWindow != NULL);

    CHECK_EQ(0,
            native_window_set_usage(
            mNativeWindow.get(),
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP));

    CHECK_EQ(0,
            native_window_set_scaling_mode(
            mNativeWindow.get(),
            NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));

    // Width must be multiple of 32???
    CHECK_EQ(0, native_window_set_buffers_geometry(
                mNativeWindow.get(),
                bufWidth,
                bufHeight,
                halFormat));

    uint32_t transform;
    switch (rotationDegrees) {
        case 0: transform = 0; break;
        case 90: transform = HAL_TRANSFORM_ROT_90; break;
        case 180: transform = HAL_TRANSFORM_ROT_180; break;
        case 270: transform = HAL_TRANSFORM_ROT_270; break;
        default: transform = 0; break;
    }

    if (transform) {
        CHECK_EQ(0, native_window_set_buffers_transform(
                    mNativeWindow.get(), transform));
    }
}

CedarXSoftwareRenderer::~CedarXSoftwareRenderer() {
}

static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

void CedarXSoftwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {
    ANativeWindowBuffer *buf;
    int err;
    if ((err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf)) != 0) {
        LOGW("Surface::dequeueBuffer returned error %d", err);
        return;
    }

    CHECK_EQ(0, mNativeWindow->lockBuffer(mNativeWindow.get(), buf));

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    Rect bounds(mWidth, mHeight);

    void *dst;
    CHECK_EQ(0, mapper.lock(
                buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst));

    //LOGV("mColorFormat: %d", mColorFormat);
    size_t dst_y_size = buf->stride * buf->height;
    size_t dst_c_stride = ALIGN(buf->stride / 2, 16);
    size_t dst_c_size = dst_c_stride * buf->height / 2;

    //LOGV("buf->stride:%d buf->height:%d WXH:%dx%d dst:%p data:%p", buf->stride, buf->height, mWidth, mHeight, dst, data);
    memcpy(dst, data, dst_y_size + dst_c_size*2);
    //memcpy(dst, data, dst_y_size * 3 / 2); LOGV("render size error!");

#if 0
		{
			static int dec_count = 0;
			static FILE *fp;

			if(dec_count == 60)
			{
				fp = fopen("/data/camera/t.yuv","wb");
				LOGD("write start fp:%d addr:%p",fp,data);
				fwrite(dst,1,buf->stride * buf->height,fp);
				fwrite(dst + buf->stride * buf->height * 5/4,1,buf->stride * buf->height / 4,fp);
				fwrite(dst + buf->stride * buf->height,1,buf->stride * buf->height / 4,fp);
				fclose(fp);
				LOGD("write finish");
			}

			dec_count++;
		}
#endif

    CHECK_EQ(0, mapper.unlock(buf->handle));

    if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf)) != 0) {
        LOGW("Surface::queueBuffer returned error %d", err);
    }
    buf = NULL;


}

}  // namespace android
