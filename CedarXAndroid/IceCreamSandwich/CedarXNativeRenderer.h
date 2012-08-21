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

#ifndef CEDARXNATIVE_RENDERER_H_

#define CEDARXNATIVE_RENDERER_H_


#include <utils/RefBase.h>
#if (CEDARX_ANDROID_VERSION < 7)
#include <ui/android_native_buffer.h>
#endif
#include <hardware/hwcomposer.h>

namespace android {

enum VIDEORENDER_CMD
{
	VIDEORENDER_CMD_UNKOWN = 0,
    VIDEORENDER_CMD_ROTATION_DEG    = HWC_LAYER_ROTATION_DEG    ,
    VIDEORENDER_CMD_DITHER          = HWC_LAYER_DITHER          ,
    VIDEORENDER_CMD_SETINITPARA     = HWC_LAYER_SETINITPARA     ,
    VIDEORENDER_CMD_SETVIDEOPARA    = HWC_LAYER_SETVIDEOPARA    ,
    VIDEORENDER_CMD_SETFRAMEPARA    = HWC_LAYER_SETFRAMEPARA    ,
    VIDEORENDER_CMD_GETCURFRAMEPARA = HWC_LAYER_GETCURFRAMEPARA ,
    VIDEORENDER_CMD_QUERYVBI        = HWC_LAYER_QUERYVBI        ,
    VIDEORENDER_CMD_SETSCREEN       = HWC_LAYER_SETMODE       ,
    VIDEORENDER_CMD_SHOW            = HWC_LAYER_SHOW            ,
    VIDEORENDER_CMD_RELEASE         = HWC_LAYER_RELEASE         ,
    VIDEORENDER_CMD_SET3DMODE       = HWC_LAYER_SET3DMODE       ,
    VIDEORENDER_CMD_SETFORMAT       = HWC_LAYER_SETFORMAT       ,
    VIDEORENDER_CMD_VPPON           = HWC_LAYER_VPPON           ,
    VIDEORENDER_CMD_VPPGETON        = HWC_LAYER_VPPGETON        ,
    VIDEORENDER_CMD_SETLUMASHARP    = HWC_LAYER_SETLUMASHARP    ,
    VIDEORENDER_CMD_GETLUMASHARP    = HWC_LAYER_GETLUMASHARP    ,
    VIDEORENDER_CMD_SETCHROMASHARP  = HWC_LAYER_SETCHROMASHARP  ,
    VIDEORENDER_CMD_GETCHROMASHARP  = HWC_LAYER_GETCHROMASHARP  ,
    VIDEORENDER_CMD_SETWHITEEXTEN   = HWC_LAYER_SETWHITEEXTEN   ,
    VIDEORENDER_CMD_GETWHITEEXTEN   = HWC_LAYER_GETWHITEEXTEN   ,
    VIDEORENDER_CMD_SETBLACKEXTEN   = HWC_LAYER_SETBLACKEXTEN   ,
    VIDEORENDER_CMD_GETBLACKEXTEN   = HWC_LAYER_GETBLACKEXTEN   ,
    VIDEORENDER_CMD_SET_CROP		= 0x1000         ,
};

struct MetaData;

class CedarXNativeRenderer {
public:
    CedarXNativeRenderer(
            const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta);

    ~CedarXNativeRenderer();

    void render(
            const void *data, size_t size, void *platformPrivate);

    int control(int cmd, int para);
private:
    //OMX_COLOR_FORMATTYPE mColorFormat;
    sp<ANativeWindow> mNativeWindow;
    int32_t mWidth, mHeight;
    int32_t mCropLeft, mCropTop, mCropRight, mCropBottom;
    int32_t mCropWidth, mCropHeight;

    CedarXNativeRenderer(const CedarXNativeRenderer &);
    CedarXNativeRenderer &operator=(const CedarXNativeRenderer &);
};

}  // namespace android

#endif  // SOFTWARE_RENDERER_H_
