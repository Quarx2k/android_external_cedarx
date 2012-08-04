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

#include <CDX_LogNDebug.h>
#define LOG_TAG "CedarXPlayer"
#include <utils/Log.h>
 
#include <dlfcn.h>

#include "CedarXPlayer.h"
#include "CedarXNativeRenderer.h"
#include "CedarXSoftwareRenderer.h"
#include <libcedarv.h>

#include <binder/IPCThreadState.h>
#include <media/stagefright/CedarXAudioPlayer.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaExtractor.h>
//#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>

#include <gui/Surface.h>
#include <gui/ISurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <gui/ISurfaceComposer.h>

#include <media/stagefright/foundation/ALooper.h>

#include <OMX_IVCommon.h>

#include <hardware/hwcomposer.h>
#include <cutils/properties.h>

#define PROP_CHIP_VERSION_KEY  "media.cedarx.chipver"

namespace android {

#define LOGH LOGV("H/%s line:%d\n",__FILE__,__LINE__)

//static int64_t kLowWaterMarkUs = 2000000ll; // 2secs
//static int64_t kHighWaterMarkUs = 10000000ll; // 10secs

extern "C" int CedarXPlayerCallbackWrapper(void *cookie, int event, void *info);

struct CedarXDirectHwRenderer : public CedarXRenderer {
    CedarXDirectHwRenderer(
            const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
        : mTarget(new CedarXNativeRenderer(nativeWindow, meta)) {
    }

    int control(int cmd, int para) {
        return mTarget->control(cmd, para);
    }

    void render(const void *data, size_t size) {
        mTarget->render(data, size, NULL);
    }

protected:
    virtual ~CedarXDirectHwRenderer() {
        delete mTarget;
        mTarget = NULL;
    }

private:
    CedarXNativeRenderer *mTarget;

    CedarXDirectHwRenderer(const CedarXDirectHwRenderer &);
    CedarXDirectHwRenderer &operator=(const CedarXDirectHwRenderer &);;
};

struct CedarXLocalRenderer : public CedarXRenderer {
    CedarXLocalRenderer(
            const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
        : mTarget(new CedarXSoftwareRenderer(nativeWindow, meta)) {
    }

    int control(int cmd, int para) {
        //return mTarget->control(cmd, para);
    	return 0;
    }

    void render(const void *data, size_t size) {
        mTarget->render(data, size, NULL);
    }

protected:
    virtual ~CedarXLocalRenderer() {
        delete mTarget;
        mTarget = NULL;
    }

private:
    CedarXSoftwareRenderer *mTarget;

    CedarXLocalRenderer(const CedarXLocalRenderer &);
    CedarXLocalRenderer &operator=(const CedarXLocalRenderer &);;
};

CedarXPlayer::CedarXPlayer() :
	mQueueStarted(false), mVideoRendererIsPreview(false),
	mAudioPlayer(NULL), mFlags(0), mExtractorFlags(0), mCanSeek(0){

	LOGV("Construction");

	mExtendMember = (CedarXPlayerExtendMember *)malloc(sizeof(CedarXPlayerExtendMember));

	reset_l();
	CDXPlayer_Create((void**)&mPlayer);

	mPlayer->control(mPlayer, CDX_CMD_REGISTER_CALLBACK, (unsigned int)&CedarXPlayerCallbackWrapper, (unsigned int)this);
	isCedarXInitialized = true;
	mMaxOutputWidth = 0;
	mMaxOutputHeight = 0;
	mDisableXXXX = 0;

    _3d_mode							= 0;
    display_3d_mode						= 0;
    anaglagh_type						= 0;
    anaglagh_en							= 0;
    wait_anaglagh_display_change		= 0;
}

CedarXPlayer::~CedarXPlayer() {

	if(isCedarXInitialized){
		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
		CDXPlayer_Destroy(mPlayer);
		mPlayer = NULL;
		isCedarXInitialized = false;
	}

	if (mAudioPlayer) {
		LOGV("delete mAudioPlayer");
		delete mAudioPlayer;
		mAudioPlayer = NULL;
	}

	if(mExtendMember != NULL){
		free(mExtendMember);
		mExtendMember = NULL;
	}
	LOGV("Deconstruction %x",mFlags);
}

void CedarXPlayer::setUID(uid_t uid) {
    LOGV("CedarXPlayer running on behalf of uid %d", uid);

    mUID = uid;
    mUIDValid = true;
}

void CedarXPlayer::setListener(const wp<MediaPlayerBase> &listener) {
	Mutex::Autolock autoLock(mLock);
	mListener = listener;
}

status_t CedarXPlayer::setDataSource(const char *uri, const KeyedVector<
		String8, String8> *headers) {
	//Mutex::Autolock autoLock(mLock);
	LOGV("CedarXPlayer::setDataSource (%s)", uri);
	mUri = uri;
	mIsUri = true;
	if (headers) {
	    mUriHeaders = *headers;
	}
#if 0
	const char* dbg_url = "http://192.168.1.147/tx5.mp4";
	//const char* dbg_url = "/mnt/extsd/hb.mp4";
	mPlayer->control(mPlayer, CDX_SET_DATASOURCE_URL, (unsigned int)dbg_url, 0);
#else
	mPlayer->control(mPlayer, CDX_SET_DATASOURCE_URL, (unsigned int)uri, 0);
#endif
	return OK;
}

status_t CedarXPlayer::setDataSource(int fd, int64_t offset, int64_t length) {
	//Mutex::Autolock autoLock(mLock);
	LOGV("CedarXPlayer::setDataSource fd");
	CedarXExternFdDesc ext_fd_desc;
	ext_fd_desc.fd = fd;
	ext_fd_desc.offset = offset;
	ext_fd_desc.length = length;
	mIsUri = false;
	mPlayer->control(mPlayer, CDX_SET_DATASOURCE_FD, (unsigned int)&ext_fd_desc, 0);
	return OK;
}

status_t CedarXPlayer::setDataSource(const sp<IStreamSource> &source) {
    return INVALID_OPERATION;
}

status_t CedarXPlayer::setParameter(int key, const Parcel &request)
{
	return ERROR_UNSUPPORTED;
}

status_t CedarXPlayer::getParameter(int key, Parcel *reply) {
	return ERROR_UNSUPPORTED;
}

void CedarXPlayer::reset() {
	//Mutex::Autolock autoLock(mLock);
	LOGV("RESET????, context: %p",this);

	if(mPlayer != NULL){
		mPlayer->control(mPlayer, CDX_CMD_RESET, 0, 0);
		if(isCedarXInitialized){
			mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
			CDXPlayer_Destroy(mPlayer);
			mPlayer = NULL;
			isCedarXInitialized = false;
		}
	}

	reset_l();
}

void CedarXPlayer::reset_l() {
	LOGV("reset_l");

	mPlayerState = PLAYER_STATE_UNKOWN;
	pause_l(true);
	mVideoRenderer.clear();
	mVideoRenderer = NULL;
	if(mAudioPlayer){
		delete mAudioPlayer;
		mAudioPlayer = NULL;
	}
	LOGV("RESET End");

	mDurationUs = 0;
	mFlags = 0;
	mFlags |= RESTORE_CONTROL_PARA;
	mVideoWidth = mVideoHeight = -1;
	mVideoTimeUs = 0;

	mTagPlay = true;
	mSeeking = false;
	mSeekNotificationSent = false;
	mSeekTimeUs = 0;
	mLastValidPosition = 0;

	mAudioTrackIndex = 0;
	mBitrate = -1;
	mUri.setTo("");
	mUriHeaders.clear();

	memset(&mSubtitleParameter, 0, sizeof(struct SubtitleParameter));
	mSubtitleParameter.mSubtitleGate = 1;
}

void CedarXPlayer::notifyListener_l(int msg, int ext1, int ext2) {
	if (mListener != NULL) {
		sp<MediaPlayerBase> listener = mListener.promote();

		if (listener != NULL) {
			listener->sendEvent(msg, ext1, ext2);
		}
	}
}

status_t CedarXPlayer::play() {
	Mutex::Autolock autoLock(mLock);
	LOGV("CedarXPlayer::play()");

	if(mFlags & NATIVE_SUSPENDING) {
		LOGW("you has been suspend by other's");
		return UNKNOWN_ERROR;
	}

	mFlags &= ~CACHE_UNDERRUN;

	status_t ret = play_l(CDX_CMD_START_ASYNC);

	LOGV("CedarXPlayer::play() end");
	return ret;
}

status_t CedarXPlayer::play_l(int command) {
	LOGV("CedarXPlayer::play_l()");

	if (mFlags & PLAYING) {
		return OK;
	}

    if (!(mFlags & PREPARED)) {
        status_t err = prepare_l();

        if (err != OK) {
            return err;
        }
    }

	mFlags |= PLAYING;

	if(mAudioPlayer){
		mAudioPlayer->resume();
	}

	if(mFlags & RESTORE_CONTROL_PARA){
		if(mMediaInfo.mSubtitleStreamCount > 0) {
			LOGV("Restore control parameter!");
			if(mSubtitleParameter.mSubtitleDelay != 0){
				setSubDelay(mSubtitleParameter.mSubtitleDelay);
			}

			if(mSubtitleParameter.mSubtitleColor != 0){
				LOGV("-------mSubtitleParameter.mSubtitleColor: %x",mSubtitleParameter.mSubtitleColor);
				setSubColor(mSubtitleParameter.mSubtitleColor);
			}

			if(mSubtitleParameter.mSubtitleFontSize != 0){
				setSubFontSize(mSubtitleParameter.mSubtitleFontSize);
			}

			if(mSubtitleParameter.mSubtitlePosition != 0){
				setSubPosition(mSubtitleParameter.mSubtitlePosition);
			}

			setSubGate(mSubtitleParameter.mSubtitleGate);
		}
		mFlags &= ~RESTORE_CONTROL_PARA;
	}

	if(mSeeking && mTagPlay && mSeekTimeUs > 0){
		mPlayer->control(mPlayer, CDX_CMD_TAG_START_ASYNC, (unsigned int)&mSeekTimeUs, 0);
		LOGD("--tag play %lldus",mSeekTimeUs);
	}
	else if(mPlayerState == PLAYER_STATE_SUSPEND || mPlayerState == PLAYER_STATE_RESUME){
		mPlayer->control(mPlayer, CDX_CMD_TAG_START_ASYNC, (unsigned int)&mSuspensionState.mPositionUs, 0);
		LOGD("--tag play %lldus",mSuspensionState.mPositionUs);
	}
	else {
		mPlayer->control(mPlayer, command, (unsigned int)&mSuspensionState.mPositionUs, 0);
	}

	mTagPlay = false;
	mPlayerState = PLAYER_STATE_PLAYING;
	mFlags &= ~PAUSING;

	return OK;
}

status_t CedarXPlayer::stop() {
	LOGV("CedarXPlayer::stop");

	if(mPlayer != NULL){
		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
	}
	stop_l();

	if(this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_3D)
	{
		set3DMode(CEDARV_3D_MODE_NONE, CEDARX_DISPLAY_3D_MODE_2D);
	}

	this->_3d_mode 							= CEDARV_3D_MODE_NONE;
	this->display_3d_mode 					= CEDARX_DISPLAY_3D_MODE_2D;
	this->anaglagh_en						= 0;
	this->anaglagh_type						= 0;
	this->wait_anaglagh_display_change 		= 0;

	//* need to reset the display?
	//* TODO.

	return OK;
}

status_t CedarXPlayer::stop_l() {
	LOGV("stop() status:%x", mFlags & PLAYING);

	notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
	LOGD("MEDIA_PLAYBACK_COMPLETE");
	notifyListener_l(MEDIA_PLAYBACK_COMPLETE);

	pause_l(true);

	mVideoRenderer.clear();
	mVideoRenderer = NULL;
	mFlags &= ~SUSPENDING;
	LOGV("stop finish 1...");

	return OK;
}

status_t CedarXPlayer::pause() {
	//Mutex::Autolock autoLock(mLock);
	LOGV("pause cmd");
#if 0
	mFlags &= ~CACHE_UNDERRUN;
	mPlayer->control(mPlayer, CDX_CMD_PAUSE, 0, 0);

	return pause_l(false);
#else
	if (!(mFlags & PLAYING)) {
		return OK;
	}

	pause_l(false);
	mPlayer->control(mPlayer, CDX_CMD_PAUSE_ASYNC, 0, 0);

	return OK;
#endif
}

status_t CedarXPlayer::pause_l(bool at_eos) {

	mPlayerState = PLAYER_STATE_PAUSE;

	if (!(mFlags & PLAYING)) {
		return OK;
	}

	if (mAudioPlayer != NULL) {
		if (at_eos) {
			// If we played the audio stream to completion we
			// want to make sure that all samples remaining in the audio
			// track's queue are played out.
			mAudioPlayer->pause(true /* playPendingSamples */);
		} else {
			mAudioPlayer->pause();
		}
	}

	mFlags &= ~PLAYING;
	mFlags |= PAUSING;

	return OK;
}

bool CedarXPlayer::isPlaying() const {
	//LOGV("isPlaying cmd mFlags=0x%x",mFlags);
	return (mFlags & PLAYING) || (mFlags & CACHE_UNDERRUN);
}

status_t CedarXPlayer::setNativeWindow_l(const sp<ANativeWindow> &native) {
    mNativeWindow = native;

//    if (mVideoSource == NULL) {
//        return OK;
//    }
//
//    LOGV("attempting to reconfigure to use new surface");
//
//    bool wasPlaying = (mFlags & PLAYING) != 0;
//
//    pause_l();
    mVideoRenderer.clear();

//    shutdownVideoDecoder_l();
//
//    status_t err = initVideoDecoder();
//
//    if (err != OK) {
//        LOGE("failed to reinstantiate video decoder after surface change.");
//        return err;
//    }
//
//    if (mLastVideoTimeUs >= 0) {
//        mSeeking = SEEK;
//        mSeekTimeUs = mLastVideoTimeUs;
//        modifyFlags((AT_EOS | AUDIO_AT_EOS | VIDEO_AT_EOS), CLEAR);
//    }
//
//    if (wasPlaying) {
//        play_l();
//    }

    return OK;
}

status_t CedarXPlayer::setSurface(const sp<Surface> &surface) {
    Mutex::Autolock autoLock(mLock);

    LOGV("setSurface");
    mSurface = surface;
    return setNativeWindow_l(surface);
}

status_t CedarXPlayer::setSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture) {
    Mutex::Autolock autoLock(mLock);

    mSurface.clear();

    LOGV("setSurfaceTexture");

    status_t err;
    if (surfaceTexture != NULL) {
        err = setNativeWindow_l(new SurfaceTextureClient(surfaceTexture));
    } else {
        err = setNativeWindow_l(NULL);
    }

    return err;
}

void CedarXPlayer::setAudioSink(const sp<MediaPlayerBase::AudioSink> &audioSink) {
	//Mutex::Autolock autoLock(mLock);
	mAudioSink = audioSink;
}

status_t CedarXPlayer::setLooping(bool shouldLoop) {
	//Mutex::Autolock autoLock(mLock);

	mFlags = mFlags & ~LOOPING;

	if (shouldLoop) {
		mFlags |= LOOPING;
	}

	return OK;
}

status_t CedarXPlayer::getDuration(int64_t *durationUs) {

	mPlayer->control(mPlayer, CDX_CMD_GET_DURATION, (unsigned int)durationUs, 0);
	*durationUs *= 1000;
	mDurationUs = *durationUs;

	return OK;
}

status_t CedarXPlayer::getPosition(int64_t *positionUs) {

	if(mFlags & AT_EOS){
		*positionUs = mDurationUs;
		return OK;
	}

	{
		//Mutex::Autolock autoLock(mLock);
		if(mPlayer != NULL){
			mPlayer->control(mPlayer, CDX_CMD_GET_POSITION, (unsigned int)positionUs, 0);
		}
	}

	if(*positionUs == -1){
		*positionUs = mSeekTimeUs; //temp to fix sohuvideo bug
	}

	//LOGV("getPosition: %lld",*positionUs / 1000);

	return OK;
}

status_t CedarXPlayer::seekTo(int64_t timeUs) {

	int64_t currPositionUs;
	getPosition(&currPositionUs);

	{
		Mutex::Autolock autoLock(mLock);
		mSeekNotificationSent = false;
		LOGV("seek cmd0 to %lldms", timeUs);

		if (mFlags & CACHE_UNDERRUN) {
			mFlags &= ~CACHE_UNDERRUN;
			play_l(CDX_CMD_START_ASYNC);
		}

		mSeekTimeUs = timeUs * 1000;
		mSeeking = true;
		mFlags &= ~(AT_EOS | AUDIO_AT_EOS | VIDEO_AT_EOS);

		if (!(mFlags & PLAYING)) {
			LOGV( "seeking while paused, sending SEEK_COMPLETE notification"
						" immediately.");

			notifyListener_l(MEDIA_SEEK_COMPLETE);
			mSeekNotificationSent = true;
			return OK;
		}
	}

	//mPlayer->control(mPlayer, CDX_CMD_SET_AUDIOCHANNEL_MUTE, 3, 0);
	mPlayer->control(mPlayer, CDX_CMD_SEEK_ASYNC, (int)timeUs, (int)(currPositionUs/1000));

	LOGV("--------- seek cmd1 to %lldms end -----------", timeUs);

	return OK;
}

void CedarXPlayer::finishAsyncPrepare_l(int err){

	LOGV("finishAsyncPrepare_l");
	if(err == -1){
		LOGE("CedarXPlayer:prepare error!");
		abortPrepare(UNKNOWN_ERROR);
		return;
	}

	mPlayer->control(mPlayer, CDX_CMD_GET_STREAM_TYPE, (unsigned int)&mStreamType, 0);
	mPlayer->control(mPlayer, CDX_CMD_GET_MEDIAINFO, (unsigned int)&mMediaInfo, 0);
	//if(mStreamType != CEDARX_STREAM_LOCALFILE) {
	mFlags &= ~CACHE_UNDERRUN;
	//}
	mVideoWidth  = mMediaInfo.mVideoInfo[0].mFrameWidth; //TODO: temp assign
	mVideoHeight = mMediaInfo.mVideoInfo[0].mFrameHeight;
	mCanSeek = mMediaInfo.mFlags & 1;
	notifyListener_l(MEDIA_SET_VIDEO_SIZE, mVideoWidth, mVideoHeight);
	mFlags &= ~(PREPARING|PREPARE_CANCELLED);
	mFlags |= PREPARED;

	//mPlayer->control(mPlayer, CDX_CMD_SET_AUDIOCHANNEL_MUTE, 1, 0);

	if(mIsAsyncPrepare){
		notifyListener_l(MEDIA_PREPARED);
	}

	return;
}

void CedarXPlayer::finishSeek_l(int err){
	Mutex::Autolock autoLock(mLock);
	LOGV("finishSeek_l");

	if(mAudioPlayer){
		mAudioPlayer->seekTo(0);
	}
	mSeeking = false;
	if (!mSeekNotificationSent) {
		LOGV("MEDIA_SEEK_COMPLETE return");
		notifyListener_l(MEDIA_SEEK_COMPLETE);
		mSeekNotificationSent = true;
	}
	//mPlayer->control(mPlayer, CDX_CMD_SET_AUDIOCHANNEL_MUTE, 0, 0);

	return;
}

status_t CedarXPlayer::prepareAsync() {
	Mutex::Autolock autoLock(mLock);
	int outputSetting = 0;
	char prop_value[4];

	if ((mFlags & PREPARING) || (mPlayer == NULL)) {
		return UNKNOWN_ERROR; // async prepare already pending
	}
	mFlags |= PREPARING;
	mIsAsyncPrepare = true;

	property_get(PROP_CHIP_VERSION_KEY, prop_value, "3");
	mPlayer->control(mPlayer, CDX_CMD_SET_SOFT_CHIP_VERSION, atoi(prop_value), 0);

	//0: no rotate, 1: 90 degree (clock wise), 2: 180, 3: 270, 4: horizon flip, 5: vertical flig;
	//mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_ROTATION, 2, 0);

	//outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;
	if(outputSetting & CEDARX_OUTPUT_SETTING_MODE_PLANNER) {
		if(mScreenID != SLAVE_SCREEN){//TODO: we must consider hot-plugin
			outputSetting |= CEDARX_OUTPUT_SETTING_HARDWARE_CONVERT;
		}
	}
	mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_OUTPUT_SETTING, outputSetting, 0);

	//mPlayer->control(mPlayer, CDX_CMD_SET_NETWORK_ENGINE, CEDARX_NETWORK_ENGINE_SFT, 0);

	//mMaxOutputWidth = 720;
	//mMaxOutputHeight = 576;
	if(mMaxOutputWidth && mMaxOutputHeight) {
		LOGV("Max ouput size %dX%d", mMaxOutputWidth, mMaxOutputHeight);
		mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_MAXWIDTH, mMaxOutputWidth, 0);
		mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_MAXHEIGHT, mMaxOutputHeight, 0);
	}

	//mPlayer->control(mPlayer, CDX_CMD_DISABLE_XXXX, mDisableXXXX, 0);

	return (mPlayer->control(mPlayer, CDX_CMD_PREPARE_ASYNC, 0, 0) == 0 ? OK : UNKNOWN_ERROR);
}

status_t CedarXPlayer::prepare() {
	status_t ret;

	Mutex::Autolock autoLock(mLock);
	LOGV("prepare");
	this->_3d_mode 							= CEDARV_3D_MODE_NONE;
	this->display_3d_mode 					= CEDARX_DISPLAY_3D_MODE_2D;
	this->anaglagh_en						= 0;
	this->anaglagh_type						= 0;
	this->wait_anaglagh_display_change 		= 0;

	ret = prepare_l();
	getInputDimensionType();

	return ret;
}

status_t CedarXPlayer::prepare_l() {
	if (mFlags & PREPARED) {
	    return OK;
	}

	mIsAsyncPrepare = false;

	if(mPlayer->control(mPlayer, CDX_CMD_PREPARE, 0, 0) != 0){
		return UNKNOWN_ERROR;
	}

	finishAsyncPrepare_l(0);

	return OK;
}

void CedarXPlayer::abortPrepare(status_t err) {
	CHECK(err != OK);

	if (mIsAsyncPrepare) {
		notifyListener_l(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);
	}

	mPrepareResult = err;
	mFlags &= ~(PREPARING | PREPARE_CANCELLED);
}

//status_t CedarXPlayer::suspend() {
//	LOGD("suspend start");
//
//	if (mFlags & SUSPENDING)
//		return OK;
//
//	SuspensionState *state = &mSuspensionState;
//	getPosition(&state->mPositionUs);
//
//	Mutex::Autolock autoLock(mLock);
//
//	state->mFlags = mFlags & (PLAYING | AUTO_LOOPING | LOOPING | AT_EOS);
//    state->mUri = mUri;
//    state->mUriHeaders = mUriHeaders;
//	mFlags |= SUSPENDING;
//
//	if(isCedarXInitialized){
//		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
//		CDXPlayer_Destroy(mPlayer);
//		mPlayer = NULL;
//		isCedarXInitialized = false;
//	}
//
//	pause_l(true);
//	mVideoRenderer.clear();
//	mVideoRenderer = NULL;
//	if(mAudioPlayer){
//		delete mAudioPlayer;
//		mAudioPlayer = NULL;
//	}
//	mPlayerState = PLAYER_STATE_SUSPEND;
//	LOGD("suspend end");
//
//	return OK;
//}
//
//status_t CedarXPlayer::resume() {
//	LOGD("resume start");
//    Mutex::Autolock autoLock(mLock);
//    SuspensionState *state = &mSuspensionState;
//    status_t err;
//
//    if(mPlayer == NULL){
//    	CDXPlayer_Create((void**)&mPlayer);
//    	mPlayer->control(mPlayer, CDX_CMD_REGISTER_CALLBACK, (unsigned int)&CedarXPlayerCallbackWrapper, (unsigned int)this);
//    	isCedarXInitialized = true;
//    }
//
//    //mPlayer->control(mPlayer, CDX_CMD_SET_STATE, CDX_STATE_UNKOWN, 0);
//
//    if (mIsUri) {
//    	err = setDataSource(state->mUri, &state->mUriHeaders);
//    } else {
//        LOGW("NOT support setdatasouce non-uri currently");
//    }
//
//    mFlags = state->mFlags & (AUTO_LOOPING | LOOPING | AT_EOS);
//
//    mFlags |= RESTORE_CONTROL_PARA;
//
//    if (state->mFlags & PLAYING) {
//        play_l(CDX_CMD_TAG_START_ASYNC);
//    }
//    mFlags &= ~SUSPENDING;
//    //state->mPositionUs = 0;
//    mPlayerState = PLAYER_STATE_RESUME;
//
//    LOGD("resume end");
//
//	return OK;
//}


status_t CedarXPlayer::setScreen(int screen) {
	mScreenID = screen;
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		LOGV("CedarX setScreen to:%d", screen);
		return mVideoRenderer->control(VIDEORENDER_CMD_SETSCREEN, screen);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::set3DMode(int source3dMode, int displayMode)
{
	video3Dinfo_t _3d_info;

	_3d_info.width 	 			= this->mDisplayWidth;
	_3d_info.height	 			= this->mDisplayHeight;
	_3d_info.format	 			= (this->mDisplayFormat == CEDARV_PIXEL_FORMAT_AW_YUV422) ?  HWC_FORMAT_MBYUV422:HWC_FORMAT_MBYUV420; //* or HWC_FORMAT_RGBA_8888.
	_3d_info.is_mode_changed 	= 1;

	//* set source 3d mode.
	if(this->_3d_mode == CEDARV_3D_MODE_DOUBLE_STREAM)
		_3d_info._3d_mode = 1;		//* OVERLAY_3D_OUT_MODE_FP
	else if(this->_3d_mode == CEDARV_3D_MODE_SIDE_BY_SIDE)
		_3d_info._3d_mode = 3;		//* OVERLAY_3D_OUT_MODE_SSH
	else if(this->_3d_mode == CEDARV_3D_MODE_TOP_TO_BOTTOM)
		_3d_info._3d_mode = 0;		//* OVERLAY_3D_OUT_MODE_TB
	else if(this->_3d_mode == CEDARV_3D_MODE_LINE_INTERLEAVE)
		_3d_info._3d_mode = 4;		//* OVERLAY_3D_OUT_MODE_LI
	else if(this->_3d_mode == CEDARV_3D_MODE_COLUME_INTERLEAVE)
		_3d_info._3d_mode = 5;		//* OVERLAY_3D_OUT_MODE_CI_1
	else
		_3d_info._3d_mode = 0xff;	//* OVERLAY_3D_OUT_MODE_NORMAL

	//* set display 3d mode.
	if(this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
	{
		if(this->_3d_mode == CEDARV_3D_MODE_SIDE_BY_SIDE)
			_3d_info.width = mDisplayWidth/2;//frm_inf->width /=2;
		else if(this->_3d_mode == CEDARV_3D_MODE_TOP_TO_BOTTOM)
			_3d_info.height = mDisplayHeight/2;//frm_inf->height /=2;

		if(_3d_info.format != HWC_FORMAT_RGBA_8888)
			_3d_info.format = HWC_FORMAT_RGBA_8888;		//* force pixel format to be RGBA8888.

		_3d_info.display_mode = 2;
	}
	else if(this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_3D)
		_3d_info.display_mode = 1;
	else if(this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_2D)
		_3d_info.display_mode = 3;
	else
		_3d_info.display_mode = 0;

	if(mVideoRenderer != NULL)
	{
		LOGV("set hdmi, src_mode = %d, dst_mode = %d", _3d_info._3d_mode, _3d_info.display_mode);
		return mVideoRenderer->control(VIDEORENDER_CMD_SET3DMODE, (int)&_3d_info);
	}

	return UNKNOWN_ERROR;
}

int CedarXPlayer::getMeidaPlayerState() {
    return mPlayerState;
}

int CedarXPlayer::getSubCount()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBCOUNT, (unsigned int)&tmp, 0);

	LOGV("getSubCount:%d",tmp);

    return tmp;
}

int CedarXPlayer::getSubList(MediaPlayer_SubInfo *infoList, int count)
{
	if(mPlayer == NULL){
		return -1;
	}

	if(mPlayer->control(mPlayer, CDX_CMD_GETSUBLIST, (unsigned int)infoList, count) == 0){
		return count;
	}

	return -1;
}

int CedarXPlayer::getCurSub()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETCURSUB, (unsigned int)&tmp, 0);
    return tmp;
};

status_t CedarXPlayer::switchSub(int index)
{
	if(mPlayer == NULL || mSubtitleParameter.mSubtitleIndex == index){
		return -1;
	}

	mSubtitleParameter.mSubtitleIndex = index;
	return mPlayer->control(mPlayer, CDX_CMD_SWITCHSUB, index, 0);
}

status_t CedarXPlayer::setSubGate(bool showSub)
{
	if(mPlayer == NULL){
		return -1;
	}

	mSubtitleParameter.mSubtitleGate = showSub;
	return mPlayer->control(mPlayer, CDX_CMD_SETSUBGATE, showSub, 0);
}

bool CedarXPlayer::getSubGate()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBGATE, (unsigned int)&tmp, 0);
	return tmp;
};

status_t CedarXPlayer::setSubColor(int color)
{
	if(mPlayer == NULL){
		return -1;
	}

	mSubtitleParameter.mSubtitleColor = color;
	return mPlayer->control(mPlayer, CDX_CMD_SETSUBCOLOR, color, 0);
}

int CedarXPlayer::getSubColor()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBCOLOR, (unsigned int)&tmp, 0);
	return tmp;
}

status_t CedarXPlayer::setSubFrameColor(int color)
{
	if(mPlayer == NULL){
		return -1;
	}

	mSubtitleParameter.mSubtitleFrameColor = color;
    return mPlayer->control(mPlayer, CDX_CMD_SETSUBFRAMECOLOR, color, 0);
}

int CedarXPlayer::getSubFrameColor()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBFRAMECOLOR, (unsigned int)&tmp, 0);
	return tmp;
}

status_t CedarXPlayer::setSubFontSize(int size)
{
	if(mPlayer == NULL){
		return -1;
	}

	mSubtitleParameter.mSubtitleFontSize = size;
	return mPlayer->control(mPlayer, CDX_CMD_SETSUBFONTSIZE, size, 0);
}

int CedarXPlayer::getSubFontSize()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBFONTSIZE, (unsigned int)&tmp, 0);
	return tmp;
}

status_t CedarXPlayer::setSubCharset(const char *charset)
{
	if(mPlayer == NULL){
		return -1;
	}

	//mSubtitleParameter.mSubtitleCharset = percent;
    return mPlayer->control(mPlayer, CDX_CMD_SETSUBCHARSET, (unsigned int)charset, 0);
}

status_t CedarXPlayer::getSubCharset(char *charset)
{
	if(mPlayer == NULL){
		return -1;
	}

    return mPlayer->control(mPlayer, CDX_CMD_GETSUBCHARSET, (unsigned int)charset, 0);
}

status_t CedarXPlayer::setSubPosition(int percent)
{
	if(mPlayer == NULL){
		return -1;
	}

	mSubtitleParameter.mSubtitlePosition = percent;
	return mPlayer->control(mPlayer, CDX_CMD_SETSUBPOSITION, percent, 0);
}

int CedarXPlayer::getSubPosition()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBPOSITION, (unsigned int)&tmp, 0);
	return tmp;
}

status_t CedarXPlayer::setSubDelay(int time)
{
	if(mPlayer == NULL){
		return -1;
	}

	mSubtitleParameter.mSubtitleDelay = time;
	return mPlayer->control(mPlayer, CDX_CMD_SETSUBDELAY, time, 0);
}

int CedarXPlayer::getSubDelay()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETSUBDELAY, (unsigned int)&tmp, 0);
	return tmp;
}

int CedarXPlayer::getTrackCount()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETTRACKCOUNT, (unsigned int)&tmp, 0);
	return tmp;
}

int CedarXPlayer::getTrackList(MediaPlayer_TrackInfo *infoList, int count)
{
	if(mPlayer == NULL){
		return -1;
	}

	if(mPlayer->control(mPlayer, CDX_CMD_GETTRACKLIST, (unsigned int)infoList, count) == 0){
		return count;
	}

	return -1;
}

int CedarXPlayer::getCurTrack()
{
	int tmp;

	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_GETCURTRACK, (unsigned int)&tmp, 0);
	return tmp;
}

status_t CedarXPlayer::switchTrack(int index)
{
	if(mPlayer == NULL || mAudioTrackIndex == index){
		return -1;
	}

	mAudioTrackIndex = index;
	return mPlayer->control(mPlayer, CDX_CMD_SWITCHTRACK, index, 0);
}

status_t CedarXPlayer::setInputDimensionType(int type)
{
	if(mPlayer == NULL)
		return -1;

	this->_3d_mode = (cedarv_3d_mode_e)type;
	if(mPlayer->control(mPlayer, CDX_CMD_SET_PICTURE_3D_MODE, this->_3d_mode, 0) != 0)
		return -1;

	//* the 3d mode you set may be invalid, get the valid 3d mode from mPlayer.
	mPlayer->control(mPlayer, CDX_CMD_GET_PICTURE_3D_MODE, (unsigned int)&this->_3d_mode, 0);

	LOGV("set 3d source mode to %d, current_3d_mode = %d", type, this->_3d_mode);

	return 0;
}

int CedarXPlayer::getInputDimensionType()
{
	cedarv_3d_mode_e tmp;

	if(mPlayer == NULL)
		return -1;

	this->pre_3d_mode = this->_3d_mode;
	mPlayer->control(mPlayer, CDX_CMD_GET_PICTURE_3D_MODE, (unsigned int)&tmp, 0);

	if((unsigned int)tmp != this->_3d_mode)
	{
		this->_3d_mode = tmp;
		LOGV("set _3d_mode to be %d when getting source mode", this->_3d_mode);
	}

	LOGV("this->_3d_mode = %d", this->_3d_mode);

	return (int)this->_3d_mode;
}

status_t CedarXPlayer::setOutputDimensionType(int type)
{
	status_t ret;
	if(mPlayer == NULL)
		return -1;

	ret = mPlayer->control(mPlayer, CDX_CMD_SET_DISPLAY_MODE, type, 0);

	LOGV("set output display mode to be %d, current display mode = %d", type, this->display_3d_mode);
	if(this->display_3d_mode != CEDARX_DISPLAY_3D_MODE_ANAGLAGH && type != CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
	{
		this->display_3d_mode = type;

		set3DMode(this->_3d_mode, this->display_3d_mode);
	}
	else
	{
		//* for switching on or off the anaglagh display, setting for display device(Overlay) will be
		//* done when next picture be render.
		if(type == CEDARX_DISPLAY_3D_MODE_ANAGLAGH && this->display_3d_mode != CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
		{
			//* switch on anaglagh display.
			if(this->anaglagh_en)
				this->wait_anaglagh_display_change = 1;
		}
		else if(type != CEDARX_DISPLAY_3D_MODE_ANAGLAGH && this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
		{
			//* switch off anaglagh display.
			this->display_type_tmp_save = type;
			this->wait_anaglagh_display_change = 1;
		}
		else
		{
			if(this->_3d_mode != this->pre_3d_mode)
			{
				this->display_type_tmp_save = type;
				this->wait_anaglagh_display_change = 1;
			}
		}
	}

	return ret;
}

int CedarXPlayer::getOutputDimensionType()
{
	if(mPlayer == NULL)
		return -1;

	LOGV("current display 3d mode = %d", this->display_3d_mode);
	return (int)this->display_3d_mode;
}

status_t CedarXPlayer::setAnaglaghType(int type)
{
	int ret;
	if(mPlayer == NULL)
		return -1;

	this->anaglagh_type = (cedarv_anaglath_trans_mode_e)type;
	ret = mPlayer->control(mPlayer, CDX_CMD_SET_ANAGLAGH_TYPE, type, 0);
	if(ret == 0)
		this->anaglagh_en = 1;
	else
		this->anaglagh_en = 0;

	return ret;
}

int CedarXPlayer::getAnaglaghType()
{
	if(mPlayer == NULL)
		return -1;

	return (int)this->anaglagh_type;
}

status_t CedarXPlayer::getVideoEncode(char *encode)
{
    return -1;
}

int CedarXPlayer::getVideoFrameRate()
{
    return -1;
}

status_t CedarXPlayer::getAudioEncode(char *encode)
{
    return -1;
}

int CedarXPlayer::getAudioBitRate()
{
    return -1;
}

int CedarXPlayer::getAudioSampleRate()
{
    return -1;
}

status_t CedarXPlayer::enableScaleMode(bool enable, int width, int height)
{
	if(mPlayer == NULL){
		return -1;
	}

	if(enable) {
		mMaxOutputWidth = width;
		mMaxOutputHeight = height;
	}

	return 0;
}

status_t CedarXPlayer::setVppGate(bool enableVpp)
{
	mVppGate = enableVpp;
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		return mVideoRenderer->control(VIDEORENDER_CMD_VPPON, enableVpp);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::setLumaSharp(int value)
{
	mLumaSharp = value;
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		return mVideoRenderer->control(VIDEORENDER_CMD_SETLUMASHARP, value);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::setChromaSharp(int value)
{
	mChromaSharp = value;
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		return mVideoRenderer->control(VIDEORENDER_CMD_SETCHROMASHARP, value);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::setWhiteExtend(int value)
{
	mWhiteExtend = value;
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		return mVideoRenderer->control(VIDEORENDER_CMD_SETWHITEEXTEN, value);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::setBlackExtend(int value)
{
	mBlackExtend = value;
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		return mVideoRenderer->control(VIDEORENDER_CMD_SETBLACKEXTEN, value);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::extensionControl(int command, int para0, int para1)
{
	return OK;
}

uint32_t CedarXPlayer::flags() const {
	if(mCanSeek) {
		return MediaExtractor::CAN_PAUSE | MediaExtractor::CAN_SEEK  | MediaExtractor::CAN_SEEK_FORWARD  | MediaExtractor::CAN_SEEK_BACKWARD;
	}
	else {
		return MediaExtractor::CAN_PAUSE;
	}
}

int CedarXPlayer::nativeSuspend()
{
	if (mFlags & PLAYING) {
		LOGV("nativeSuspend may fail, I'am still playing");
		return -1;
	}

	if(mPlayer != NULL){
		LOGV("I'm force to quit!");
		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
	}

	mFlags |= NATIVE_SUSPENDING;

	return 0;
}

#if 0

static int g_dummy_render_frame_id_last = -1;
static int g_dummy_render_frame_id_curr = -1;

int CedarXPlayer::StagefrightVideoRenderInit(int width, int height, int format, void *frame_info)
{
	g_dummy_render_frame_id_last = -1;
	g_dummy_render_frame_id_curr = -1;
	return 0;
}

void CedarXPlayer::StagefrightVideoRenderData(void *frame_info, int frame_id)
{
	g_dummy_render_frame_id_last = g_dummy_render_frame_id_curr;
	g_dummy_render_frame_id_curr = frame_id;
	return ;
}

int CedarXPlayer::StagefrightVideoRenderGetFrameID()
{
	return g_dummy_render_frame_id_last;
}

void CedarXPlayer::StagefrightVideoRenderExit()
{
}

#else

int CedarXPlayer::StagefrightVideoRenderInit(int width, int height, int format, void *frame_info)
{
	cedarv_picture_t* frm_inf;

    if (mNativeWindow == NULL)
        return -1;

	mDisplayWidth 	= width;
	mDisplayHeight 	= height;
	mFirstFrame 	= 1;
	mDisplayFormat 	= (format == 0x11) ? HWC_FORMAT_MBYUV422 : ((format == 0xd) ? HAL_PIXEL_FORMAT_YV12 : HWC_FORMAT_MBYUV420);
	if(mVideoWidth!=width ||  mVideoHeight!=height)
	{
		mVideoWidth = width;
		mVideoHeight = height;
		notifyListener_l(MEDIA_SET_VIDEO_SIZE, mVideoWidth, mVideoHeight);
	}

	frm_inf = (cedarv_picture_t *)frame_info;

	sp<MetaData> meta = new MetaData;
	meta->setInt32(kKeyScreenID, mScreenID);
    meta->setInt32(kKeyColorFormat, mDisplayFormat);
    meta->setInt32(kKeyWidth, mDisplayWidth);
    meta->setInt32(kKeyHeight, mDisplayHeight);

    mVideoRenderer.clear();

    // Must ensure that mVideoRenderer's destructor is actually executed
    // before creating a new one.
    IPCThreadState::self()->flushCommands();

    if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12)
    {
    	mVideoRenderer = new CedarXDirectHwRenderer(mNativeWindow, meta);

    	set3DMode(this->_3d_mode, this->display_3d_mode);
    }
    else
    {
    	mLocalRenderFrameIDCurr = -1;
    	mVideoRenderer = new CedarXLocalRenderer(mNativeWindow, meta);
    }

    return 0;
}

void CedarXPlayer::StagefrightVideoRenderExit()
{
	if(mVideoRenderer != NULL)
	{
		if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12)
		{
			mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 0);
		}
	}
}

void CedarXPlayer::StagefrightVideoRenderData(void *frame_info, int frame_id)
{
	if(mVideoRenderer != NULL){
		cedarv_picture_t *frm_inf = (cedarv_picture_t *) frame_info;

		if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12) {
			libhwclayerpara_t overlay_para;

			if(this->wait_anaglagh_display_change)
			{
				LOGV("+++++++++++++ display 3d mode == %d", this->display_3d_mode);
				if(this->display_3d_mode != CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
				{
					//* switch on anaglagh display.
					if(this->anaglagh_type == frm_inf->anaglath_transform_mode)
					{
						this->display_3d_mode = CEDARX_DISPLAY_3D_MODE_ANAGLAGH;
						set3DMode(this->_3d_mode, this->display_3d_mode);
						this->wait_anaglagh_display_change = 0;
					}
				}
				else
				{
					//* switch off anaglagh display.
					this->display_3d_mode = this->display_type_tmp_save;
					set3DMode(this->_3d_mode, this->display_3d_mode);
					this->wait_anaglagh_display_change = 0;
				}
			}

			overlay_para.bProgressiveSrc = frm_inf->is_progressive;
			overlay_para.bTopFieldFirst = frm_inf->top_field_first;
			overlay_para.pVideoInfo.frame_rate = frm_inf->frame_rate;

			if(this->_3d_mode == CEDARV_3D_MODE_DOUBLE_STREAM && this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_3D){
				overlay_para.top_y 		= (unsigned int)frm_inf->y;
				overlay_para.top_c 		= (unsigned int)frm_inf->u;
				overlay_para.bottom_y	= (unsigned int)frm_inf->y2;
				overlay_para.bottom_c	= (unsigned int)frm_inf->u2;
			}
			else if(this->display_3d_mode == CEDARX_DISPLAY_3D_MODE_ANAGLAGH){
				overlay_para.top_y 		= (unsigned int)frm_inf->u2;
				overlay_para.top_c 		= (unsigned int)frm_inf->y2;
				overlay_para.bottom_y 	= (unsigned int)frm_inf->v2;
				overlay_para.bottom_c 	= 0;
			}
			else{
				overlay_para.top_y 		= (unsigned int)frm_inf->y;
				overlay_para.top_c 		= (unsigned int)frm_inf->u;
				overlay_para.bottom_y 	= 0;
				overlay_para.bottom_c 	= 0;
			}

			overlay_para.number = frame_id;
			overlay_para.first_frame_flg = mFirstFrame;
			mVideoRenderer->render(&overlay_para, 0);
			if(mFirstFrame) {
				//mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 1);
				mFirstFrame = 0;
			}
		}
		else {
			mVideoRenderer->render(frm_inf->y2, frm_inf->size_y2);
			mLocalRenderFrameIDCurr = frame_id;
		}
	}
}

int CedarXPlayer::StagefrightVideoRenderGetFrameID()
{
	int ret = -1;

	if(mVideoRenderer != NULL){
		if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12) {
			ret = mVideoRenderer->control(VIDEORENDER_CMD_GETCURFRAMEPARA, 0);
			//LOGV("get disp frame id:%d",ret);
		}
		else {
			ret = mLocalRenderFrameIDCurr;
		}
	}

	return ret;
}

#endif

int CedarXPlayer::StagefrightAudioRenderInit(int samplerate, int channels, int format)
{
	//initial audio playback
	if (mAudioPlayer == NULL) {
		if (mAudioSink != NULL) {
			mAudioPlayer = new CedarXAudioPlayer(mAudioSink, this);
			//mAudioPlayer->setSource(mAudioSource);
			LOGV("set audio format: (%d : %d)", samplerate, channels);
			mAudioPlayer->setFormat(samplerate, channels);

			status_t err = mAudioPlayer->start(true /* sourceAlreadyStarted */);

			if (err != OK) {
				delete mAudioPlayer;
				mAudioPlayer = NULL;

				mFlags &= ~(PLAYING);

				return err;
			}

			mWaitAudioPlayback = 1;
		}
	} else {
		mAudioPlayer->resume();
	}

	return 0;
}

void CedarXPlayer::StagefrightAudioRenderExit(int immed)
{
	if(mAudioPlayer){
		delete mAudioPlayer;
		mAudioPlayer = NULL;
	}
}

int CedarXPlayer::StagefrightAudioRenderData(void* data, int len)
{
	return mAudioPlayer->render(data,len);
}

int CedarXPlayer::StagefrightAudioRenderGetSpace(void)
{
	return mAudioPlayer->getSpace();
}

int CedarXPlayer::StagefrightAudioRenderGetDelay(void)
{
	return mAudioPlayer->getLatency();
}

int CedarXPlayer::StagefrightAudioRenderFlushCache(void)
{
	if(mAudioPlayer == NULL)
		return 0;

	return mAudioPlayer->seekTo(0);
}

int CedarXPlayer::CedarXPlayerCallback(int event, void *info)
{
	int ret = 0;
	int *para = (int*)info;

	//LOGV("----------CedarXPlayerCallback event:%d info:%p\n", event, info);

	switch (event) {
	case CDX_EVENT_PLAYBACK_COMPLETE:
		mFlags &= ~PLAYING;
		mFlags |= AT_EOS;
		stop_l(); //for gallery
		break;

	case CDX_EVENT_VIDEORENDERINIT:
	    StagefrightVideoRenderInit(para[0], para[1], para[2], (void *)para[3]);
		break;

	case CDX_EVENT_VIDEORENDERDATA:
		StagefrightVideoRenderData((void*)para[0],para[1]);
		break;

	case CDX_EVENT_VIDEORENDEREXIT:
		StagefrightVideoRenderExit();
		break;

	case CDX_EVENT_VIDEORENDERGETDISPID:
		*((int*)para) = StagefrightVideoRenderGetFrameID();
		break;

	case CDX_EVENT_AUDIORENDERINIT:
		StagefrightAudioRenderInit(para[0], para[1], para[2]);
		break;

	case CDX_EVENT_AUDIORENDEREXIT:
		StagefrightAudioRenderExit(0);
		break;

	case CDX_EVENT_AUDIORENDERDATA:
		ret = StagefrightAudioRenderData((void*)para[0],para[1]);
		break;

	case CDX_EVENT_AUDIORENDERGETSPACE:
		ret = StagefrightAudioRenderGetSpace();
		break;

	case CDX_EVENT_AUDIORENDERGETDELAY:
		ret = StagefrightAudioRenderGetDelay();
		break;

	case CDX_EVENT_AUDIORENDERFLUSHCACHE:
		ret = StagefrightAudioRenderFlushCache();
		break;

	case CDX_MEDIA_INFO_BUFFERING_START:
		LOGV("MEDIA_INFO_BUFFERING_START");
		notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
		break;

	case CDX_MEDIA_INFO_BUFFERING_END:
		LOGV("MEDIA_INFO_BUFFERING_END ...");
		notifyListener_l(MEDIA_BUFFERING_UPDATE, 0);//clear buffer scroll
		usleep(1000);
		notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
		break;

	case CDX_MEDIA_BUFFERING_UPDATE:
	{
		int64_t positionUs;
		int progress = (int)para;

		progress = progress > 100 ? 100 : progress;

		getPosition(&positionUs);
		if(mDurationUs > 0){
			progress = (int)((positionUs + (mDurationUs - positionUs) * progress / 100) * 100 / mDurationUs);
			LOGV("MEDIA_INFO_BUFFERING_UPDATE: %d %lld", (int)progress, positionUs);
			notifyListener_l(MEDIA_BUFFERING_UPDATE, (int)progress);
		}
		break;
	}

	case CDX_MEDIA_WHOLE_BUFFERING_UPDATE:
		notifyListener_l(MEDIA_BUFFERING_UPDATE, (int)para);
		break;

	case CDX_EVENT_PREPARED:
		finishAsyncPrepare_l((int)para);
		break;

	case CDX_EVENT_SEEK_COMPLETE:
		finishSeek_l(0);
		break;

	case CDX_EVENT_NATIVE_SUSPEND:
		LOGV("receive CDX_EVENT_NATIVE_SUSPEND");
		ret = nativeSuspend();
		break;

//	case CDX_MEDIA_INFO_SRC_3D_MODE:
//		{
//			cdx_3d_mode_e tmp_3d_mode;
//			tmp_3d_mode = *((cdx_3d_mode_e *)info);
//			LOGV("source 3d mode get from parser is %d", tmp_3d_mode);
//			notifyListener_l(MEDIA_INFO_SRC_3D_MODE, tmp_3d_mode);
//		}
//		break;

	default:
		break;
	}

	return ret;
}

extern "C" int CedarXPlayerCallbackWrapper(void *cookie, int event, void *info)
{
	return ((android::CedarXPlayer *)cookie)->CedarXPlayerCallback(event, info);
}

} // namespace android

