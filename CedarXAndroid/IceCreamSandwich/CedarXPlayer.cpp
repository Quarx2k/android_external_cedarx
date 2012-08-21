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
#include <CDX_Debug.h>
 
#include <dlfcn.h>

#include "CedarXPlayer.h"
#include "CedarXNativeRenderer.h"
#include "CedarXSoftwareRenderer.h"

#include <CDX_ErrorType.h>
#include <CDX_Config.h>
#include <libcedarv.h>
#include <CDX_Fileformat.h>

#include <binder/IPCThreadState.h>
#include <media/stagefright/CedarXAudioPlayer.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaExtractor.h>
#if (CEDARX_ANDROID_VERSION < 7)
#include <media/stagefright/MediaDebug.h>
#else
#include <media/stagefright/foundation/ADebug.h>
#endif
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/foundation/ALooper.h>

#include <AwesomePlayer.h>

#if (CEDARX_ANDROID_VERSION < 7)
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#endif

#include <gui/ISurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>

#include <include_sft/StreamingSource.h>

#include <hardware/hwcomposer.h>
#include <cutils/properties.h>

#define PROP_CHIP_VERSION_KEY  "media.cedarx.chipver"
#define PROP_CONSTRAINT_RES_KEY  "media.cedarx.cons_res"

namespace android {

#define MAX_HARDWARE_LAYER_SUPPORT 1

static int gHardwareLayerRefCounter = 0; //don't touch it

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
    CedarXDirectHwRenderer &operator=(const CedarXDirectHwRenderer &);
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
	mQueueStarted(false), mVideoRendererIsPreview(false), mSftSource(NULL),
	mAudioPlayer(NULL), mFlags(0), mExtractorFlags(0), mCanSeek(0){

	LOGV("Construction");
	mAudioSink = NULL;
	mAwesomePlayer = NULL;

	mExtendMember = (CedarXPlayerExtendMember *)malloc(sizeof(CedarXPlayerExtendMember));
	memset(mExtendMember, 0, sizeof(CedarXPlayerExtendMember));

	reset_l();
	CDXPlayer_Create((void**)&mPlayer);

	mPlayer->control(mPlayer, CDX_CMD_REGISTER_CALLBACK, (unsigned int)&CedarXPlayerCallbackWrapper, (unsigned int)this);
	isCedarXInitialized = true;
	mMaxOutputWidth = 0;
	mMaxOutputHeight = 0;
	mDisableXXXX = 0;
	mScreenID = 0;
	mVideoRenderer = NULL;

    _3d_mode							= 0;
    display_3d_mode						= 0;
    anaglagh_type						= 0;
    anaglagh_en							= 0;
    wait_anaglagh_display_change		= 0;
}

CedarXPlayer::~CedarXPlayer() {
	LOGV("~CedarXPlayer()");
	if(mAwesomePlayer) {
		delete mAwesomePlayer;
		mAwesomePlayer = NULL;
	}

	if (mSftSource != NULL) {
		mSftSource.clear();
		mSftSource = NULL;
	}

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
	//LOGV("Deconstruction %x",mFlags);
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
	mSourceType = SOURCETYPE_URL;
	if (headers) {
	    mUriHeaders = *headers;
	    mPlayer->control(mPlayer, CDX_CMD_SET_URL_HEADERS, (unsigned int)&mUriHeaders, 0);
	} else {
		mPlayer->control(mPlayer, CDX_CMD_SET_URL_HEADERS, (unsigned int)0, 0);
	}
#if 0
	//const char* dbg_url = "http://d327.v.iask.com/f/1/299eda0224f7747440e34e1e439e9a2c75353208.hlv";
	const char* dbg_url = "http://vhoth.dnion.videocdn.qq.com/flv/160/146/S0010wy4NWU.mp4?vkey=C03808A2DEA6F52F81DC0D0D0A0BBA1DFE3A59910FA7E34F9DCBD269E85A3F8B91946094DB16D5FB&level=1";
	//const char* dbg_url = "http://192.168.0.101/9.mp4";
	//const char* dbg_url = "http://tu.video.qiyi.com/tvx/mplay3u8/YzM5ZjIwZGMxMzg3YWIwNTE4ZWQ4MjJkMmE2YjIxZGEvZTU1YmI0NzVhNWYyNDljYmJiMzg4N2ZiYTJhNWZmZDI/NTk1ODAy.m3u8";
	//const char* dbg_url = "rtsp://live.android.maxlab.cn/maxtv-ln.sdp";
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
	mSourceType = SOURCETYPE_FD;
	mPlayer->control(mPlayer, CDX_SET_DATASOURCE_FD, (unsigned int)&ext_fd_desc, 0);
	return OK;
}

status_t CedarXPlayer::setDataSource(const sp<IStreamSource> &source) {
	RefBase *refValue;
	//Mutex::Autolock autoLock(mLock);
	LOGV("CedarXPlayer::setDataSource stream");

	mSftSource = new StreamingSource(source);
	refValue = mSftSource.get();

	mSourceType = SOURCETYPE_SFT_STREAM;
	mPlayer->control(mPlayer, CDX_SET_DATASOURCE_SFT_STREAM, (unsigned int)refValue, 0);

	return OK;
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

	if(mAwesomePlayer) {
		mAwesomePlayer->reset();
	}

	if (mPlayer != NULL) {
		if (mVideoRenderer != NULL)
		{
			if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12)
			{
				mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 0);
			}
		}

		mPlayer->control(mPlayer, CDX_CMD_RESET, 0, 0);

		if (isCedarXInitialized) {
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

	{
		Mutex::Autolock autoLock(mLockNativeWindow);
		mVideoRenderer.clear();
		mVideoRenderer = NULL;
		LOGV("mUseHardwareLayer:%d gHardwareLayerRefCounter:%d",mExtendMember->mUseHardwareLayer,gHardwareLayerRefCounter);
		if (mExtendMember->mUseHardwareLayer) {
			gHardwareLayerRefCounter--;
			mExtendMember->mUseHardwareLayer = 0;
		}
	}

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

	mTagPlay = 1;
	mSeeking = false;
	mSeekNotificationSent = false;
	mSeekTimeUs = 0;
	mLastValidPosition = 0;

	mAudioTrackIndex = 0;
	mBitrate = -1;
	mUri.setTo("");
	mUriHeaders.clear();

	if (mSftSource != NULL) {
		mSftSource.clear();
		mSftSource = NULL;
	}

	memset(&mSubtitleParameter, 0, sizeof(struct SubtitleParameter));
	mSubtitleParameter.mSubtitleGate = 0;
}

void CedarXPlayer::notifyListener_l(int msg, int ext1, int ext2) {
	if (mListener != NULL) {
		sp<MediaPlayerBase> listener = mListener.promote();

		if (listener != NULL) {
			if(mSourceType == SOURCETYPE_SFT_STREAM
				&& msg== MEDIA_INFO && (ext1 == MEDIA_INFO_BUFFERING_START || ext1 == MEDIA_INFO_BUFFERING_END
						|| ext1 == MEDIA_BUFFERING_UPDATE)) {
				LOGV("skip notifyListerner");
				return;
			}

			//if(msg != MEDIA_BUFFERING_UPDATE)
			listener->sendEvent(msg, ext1, ext2);
		}
	}
}

status_t CedarXPlayer::play() {
	LOGV("CedarXPlayer::play()");
	SuspensionState *state = &mSuspensionState;

	if(mAwesomePlayer) {
		return mAwesomePlayer->play();
	}

	if(mFlags & NATIVE_SUSPENDING) {
		LOGW("you has been suspend by other's");
		mFlags &= ~NATIVE_SUSPENDING;
		state->mFlags |= PLAYING;
		return resume();
	}

	Mutex::Autolock autoLock(mLock);

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

	if (mSourceType == SOURCETYPE_SFT_STREAM) {
		if (!(mFlags & (PREPARED | PREPARING))) {
			mFlags |= PREPARING;
			mIsAsyncPrepare = true;

			mPlayer->control(mPlayer, CDX_CMD_SET_NETWORK_ENGINE, CEDARX_NETWORK_ENGINE_SFT, 0);
			mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_OUTPUT_SETTING, CEDARX_OUTPUT_SETTING_MODE_PLANNER, 0);
			mPlayer->control(mPlayer, CDX_CMD_PREPARE_ASYNC, 0, 0);
		}
	}
	else if (!(mFlags & PREPARED)) {
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

			if(mSubtitleParameter.mSubtitleIndex){
				mPlayer->control(mPlayer, CDX_CMD_SWITCHSUB, mSubtitleParameter.mSubtitleIndex, 0);
			}
		}

		if(mAudioTrackIndex){
			mPlayer->control(mPlayer, CDX_CMD_SWITCHTRACK, mAudioTrackIndex, 0);
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

	mSeeking = false;
	mTagPlay = 0;
	mPlayerState = PLAYER_STATE_PLAYING;
	mFlags &= ~PAUSING;

	return OK;
}

status_t CedarXPlayer::stop() {
	LOGV("CedarXPlayer::stop");

	if(mAwesomePlayer) {
		return mAwesomePlayer->pause();
	}

	if(mPlayer != NULL){
		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
	}
	stop_l();

	if(display_3d_mode == CEDARX_DISPLAY_3D_MODE_3D)
	{
		set3DMode(CEDARV_3D_MODE_NONE, CEDARX_DISPLAY_3D_MODE_2D);
	}

	_3d_mode 							= CEDARV_3D_MODE_NONE;
	display_3d_mode 					= CEDARX_DISPLAY_3D_MODE_2D;
	anaglagh_en						= 0;
	anaglagh_type						= 0;
	wait_anaglagh_display_change 		= 0;

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

	if(mVideoRenderer != NULL)
	{
		mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 0);
	}

	{
		Mutex::Autolock autoLock(mLockNativeWindow);
		mVideoRenderer.clear();
		mVideoRenderer = NULL;
		LOGV("mUseHardwareLayer:%d gHardwareLayerRefCounter:%d",mExtendMember->mUseHardwareLayer,gHardwareLayerRefCounter);
		if (mExtendMember->mUseHardwareLayer) {
			gHardwareLayerRefCounter--;
			mExtendMember->mUseHardwareLayer = 0;
		}
	}

	mFlags &= ~SUSPENDING;
	LOGV("stop finish 1...");

	return OK;
}

status_t CedarXPlayer::pause() {
	//Mutex::Autolock autoLock(mLock);
	LOGV("pause()");

	if(mAwesomePlayer) {
		return mAwesomePlayer->pause();
	}

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
	if(mAwesomePlayer) {
		return mAwesomePlayer->isPlaying();
	}
	//LOGV("isPlaying cmd mFlags=0x%x",mFlags);
	return (mFlags & PLAYING) || (mFlags & CACHE_UNDERRUN);
}

status_t CedarXPlayer::setNativeWindow_l(const sp<ANativeWindow> &native) {
	int i = 0;
    mNativeWindow = native;

    if (mVideoWidth <= 0) {
        return OK;
    }

    LOGV("attempting to reconfigure to use new surface");

    pause();

    {
    	Mutex::Autolock autoLock(mLockNativeWindow);
    	mVideoRenderer.clear();
    	mVideoRenderer = NULL;
    }

    play();

    return OK;
}

status_t CedarXPlayer::setSurface(const sp<Surface> &surface) {
    Mutex::Autolock autoLock(mLock);

    LOGV("setSurface");
    mSurface = surface;
    return setNativeWindow_l(surface);
}

status_t CedarXPlayer::setSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture) {
    //Mutex::Autolock autoLock(mLock);

    //mSurface.clear();

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
	if (mAwesomePlayer) {
		mAwesomePlayer->setLooping(shouldLoop);
	}

	mFlags = mFlags & ~LOOPING;

	if (shouldLoop) {
		mFlags |= LOOPING;
	}

	return OK;
}

status_t CedarXPlayer::getDuration(int64_t *durationUs) {

	if (mAwesomePlayer) {
	    int64_t tmp;
	    status_t err = mAwesomePlayer->getDuration(&tmp);

	    if (err != OK) {
	        *durationUs = 0;
	        return OK;
	    }

	    *durationUs = (tmp + 500) / 1000;

	    return OK;
	}

	mPlayer->control(mPlayer, CDX_CMD_GET_DURATION, (unsigned int)durationUs, 0);
	*durationUs *= 1000;
	mDurationUs = *durationUs;

	return OK;
}

status_t CedarXPlayer::getPosition(int64_t *positionUs) {

	if (mAwesomePlayer) {
		int64_t tmp;
		status_t err = mAwesomePlayer->getPosition(&tmp);

		if (err != OK) {
			return err;
		}

		*positionUs = (tmp + 500) / 1000;
		LOGV("getPosition:%lld",*positionUs);
		return OK;
	}

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

	if(mSeeking == true) {
		*positionUs = mSeekTimeUs;
	}

	struct timeval now;
	int64_t nowUs;
	gettimeofday(&now, NULL);
	nowUs = now.tv_sec * 1000 + now.tv_usec / 1000; //ms
	if((uint64_t)(nowUs - mExtendMember->mLastGetPositionTimeUs) < 40) {
		*positionUs = mExtendMember->mLastPositionUs;
		return OK;
	}
	mExtendMember->mLastGetPositionTimeUs = nowUs;

	*positionUs = (*positionUs / 1000) * 1000; //to fix android 4.0 cts bug
	mExtendMember->mLastPositionUs = *positionUs;
	//LOGV("getPosition: %lld mSeekTimeUs:%lld nowUs:%lld lastUs:%lld",*positionUs / 1000,mSeekTimeUs,nowUs,mExtendMember->mLastGetPositionTimeUs);

	return OK;
}

status_t CedarXPlayer::seekTo(int64_t timeUs) {

	int64_t currPositionUs;
    if(mPlayer->control(mPlayer, CDX_CMD_SUPPORT_SEEK, 0, 0) == 0)
    {   

        return OK;
    }
	getPosition(&currPositionUs);

	{
		Mutex::Autolock autoLock(mLock);
		mSeekNotificationSent = false;
		LOGV("seek cmd to %llds start", timeUs/1000);

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
			mTagPlay = 1;
			notifyListener_l(MEDIA_SEEK_COMPLETE);
			mSeekNotificationSent = true;
			return OK;
		}
	}

	//mPlayer->control(mPlayer, CDX_CMD_SET_AUDIOCHANNEL_MUTE, 3, 0);
	mPlayer->control(mPlayer, CDX_CMD_SEEK_ASYNC, (int)timeUs, (int)(currPositionUs/1000));

	LOGV("seek cmd to %llds end", timeUs/1000);

	return OK;
}

void CedarXPlayer::finishAsyncPrepare_l(int err){

	LOGV("finishAsyncPrepare_l");

	if(err == CDX_ERROR_UNSUPPORT_USESFT) {
		//current http+mp3 etc goes here
		mAwesomePlayer = new AwesomePlayer;
		mAwesomePlayer->setListener(mListener);
		if(mAudioSink != NULL) {
			mAwesomePlayer->setAudioSink(mAudioSink);
		}

		if(mFlags & LOOPING) {
			mAwesomePlayer->setLooping(!!(mFlags & LOOPING));
		}
		mAwesomePlayer->setDataSource(mUri.string(), &mUriHeaders);
		mAwesomePlayer->prepareAsync();
		return;
	}

	if(err < 0){
		LOGE("CedarXPlayer:prepare error! %d", err);
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
	if (mVideoWidth && mVideoHeight) {
		notifyListener_l(MEDIA_SET_VIDEO_SIZE, mVideoWidth, mVideoHeight);
	}
	else {
		LOGW("unkown video size after prepared");
		//notifyListener_l(MEDIA_SET_VIDEO_SIZE, 640, 480);
	}
	mFlags &= ~(PREPARING|PREPARE_CANCELLED);
	mFlags |= PREPARED;

	//mPlayer->control(mPlayer, CDX_CMD_SET_AUDIOCHANNEL_MUTE, 1, 0);

	if(mIsAsyncPrepare && mSourceType != SOURCETYPE_SFT_STREAM){
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
	int  outputSetting = 0;
	int  disable_media_type = 0;
	char prop_value[4];

	if ((mFlags & PREPARING) || (mPlayer == NULL)) {
		return UNKNOWN_ERROR; // async prepare already pending
	}

	property_get(PROP_CHIP_VERSION_KEY, prop_value, "3");
	mPlayer->control(mPlayer, CDX_CMD_SET_SOFT_CHIP_VERSION, atoi(prop_value), 0);

#if 1
	if (atoi(prop_value) == 5) {
		property_get(PROP_CONSTRAINT_RES_KEY, prop_value, "1");
		if (atoi(prop_value) == 1) {
			mPlayer->control(mPlayer, CDX_CMD_SET_MAX_RESOLUTION, 1288<<16 | 1288, 0);
		}
	}
#endif

	if (mSourceType == SOURCETYPE_SFT_STREAM) {
		notifyListener_l(MEDIA_PREPARED);
		return OK;
	}

	mFlags |= PREPARING;
	mIsAsyncPrepare = true;

	//0: no rotate, 1: 90 degree (clock wise), 2: 180, 3: 270, 4: horizon flip, 5: vertical flig;
	//mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_ROTATION, 2, 0);

	if (mSourceType == SOURCETYPE_URL) {
		const char *uri = mUri.string();
		const char *extension = strrchr(uri,'.');

		//if(!(!strcasecmp(extension, ".m3u8") || !strcasecmp(extension,".m3u") || strcasestr(uri,"m3u8")!=NULL))
		{
			mPlayer->control(mPlayer, CDX_CMD_SET_NETWORK_ENGINE, CEDARX_NETWORK_ENGINE_SFT, 0);
		}

		//if (!strncasecmp("http://", uri, 7) || !strncasecmp("rtsp://", uri, 7))
		{
			//outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;
			//outputSetting |= CEDARX_OUTPUT_SETTING_HARDWARE_CONVERT; //use
		}
	}
	else if (mSourceType == SOURCETYPE_SFT_STREAM) {
		mPlayer->control(mPlayer, CDX_CMD_SET_NETWORK_ENGINE, CEDARX_NETWORK_ENGINE_SFT, 0);
		outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;
	}

	if(mNativeWindow != NULL) {
		Mutex::Autolock autoLock(mLockNativeWindow);
		if (0 == mNativeWindow->perform(mNativeWindow.get(), NATIVE_WINDOW_GETPARAMETER,NATIVE_WINDOW_CMD_GET_SURFACE_TEXTURE_TYPE, 0)) {
			LOGI("use render GPU 0");
			outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;
		}
		else {
			if (gHardwareLayerRefCounter < MAX_HARDWARE_LAYER_SUPPORT) {
				LOGV("use render HW");
				gHardwareLayerRefCounter++;
				mExtendMember->mUseHardwareLayer = 1;
			}
			else {
				LOGI("use render GPU 1");
				outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;
			}
		}
	}
	else {
		outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;
		LOGI("use render GPU 2");
	}

	//outputSetting |= CEDARX_OUTPUT_SETTING_MODE_PLANNER;

	mExtendMember->mOutputSetting = outputSetting;
	mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_OUTPUT_SETTING, outputSetting, 0);

	//mMaxOutputWidth = 720;
	//mMaxOutputHeight = 576;
	if(mMaxOutputWidth && mMaxOutputHeight) {
		LOGV("Max ouput size %dX%d", mMaxOutputWidth, mMaxOutputHeight);
		mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_MAXWIDTH, mMaxOutputWidth, 0);
		mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_MAXHEIGHT, mMaxOutputHeight, 0);
	}

#if 0
	disable_media_type |= CDX_MEDIA_TYPE_DISABLE_MPG | CDX_MEDIA_TYPE_DISABLE_TS | CDX_MEDIA_TYPE_DISABLE_ASF;
	disable_media_type |= CDX_CODEC_TYPE_DISABLE_MPEG2 | CDX_CODEC_TYPE_DISABLE_VC1;
	disable_media_type |= CDX_CODEC_TYPE_DISABLE_WMA;
	mPlayer->control(mPlayer, CDX_CMD_DISABLE_MEDIA_TYPE, disable_media_type, 0);
#endif

	//mPlayer->control(mPlayer, CDX_SET_THIRDPART_STREAM, CEDARX_THIRDPART_STREAM_USER0, 0);
	//mPlayer->control(mPlayer, CDX_SET_THIRDPART_STREAM, CEDARX_THIRDPART_STREAM_USER0, CDX_MEDIA_FILE_FMT_AVI);

	return (mPlayer->control(mPlayer, CDX_CMD_PREPARE_ASYNC, 0, 0) == 0 ? OK : UNKNOWN_ERROR);
}

status_t CedarXPlayer::prepare() {
	status_t ret;

	Mutex::Autolock autoLock(mLock);
	LOGV("prepare");
	_3d_mode 							= CEDARV_3D_MODE_NONE;
	display_3d_mode 					= CEDARX_DISPLAY_3D_MODE_2D;
	anaglagh_en						= 0;
	anaglagh_type						= 0;
	wait_anaglagh_display_change 		= 0;

	ret = prepare_l();
	getInputDimensionType();

	return ret;
}

status_t CedarXPlayer::prepare_l() {
	int ret;
	if (mFlags & PREPARED) {
	    return OK;
	}

	mIsAsyncPrepare = false;
	ret = mPlayer->control(mPlayer, CDX_CMD_PREPARE, 0, 0);
	if(ret != 0){
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

status_t CedarXPlayer::suspend() {
	LOGD("suspend start");

	if (mFlags & SUSPENDING)
		return OK;

	SuspensionState *state = &mSuspensionState;
	getPosition(&state->mPositionUs);

	//Mutex::Autolock autoLock(mLock);

	state->mFlags = mFlags & (PLAYING | AUTO_LOOPING | LOOPING | AT_EOS);
    state->mUri = mUri;
    state->mUriHeaders = mUriHeaders;
	mFlags |= SUSPENDING;

	if(isCedarXInitialized){
		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
		CDXPlayer_Destroy(mPlayer);
		mPlayer = NULL;
		isCedarXInitialized = false;
	}

	pause_l(true);

	{
		Mutex::Autolock autoLock(mLockNativeWindow);
		mVideoRenderer.clear();
		mVideoRenderer = NULL;
	}

	if(mAudioPlayer){
		delete mAudioPlayer;
		mAudioPlayer = NULL;
	}
	mPlayerState = PLAYER_STATE_SUSPEND;
	LOGD("suspend end");

	return OK;
}

status_t CedarXPlayer::resume() {
	LOGD("resume start");
    //Mutex::Autolock autoLock(mLock);
    SuspensionState *state = &mSuspensionState;
    status_t err;
    if (mSourceType != SOURCETYPE_URL){
        LOGW("NOT support setdatasouce non-uri currently");
        return UNKNOWN_ERROR;
    }

    if(mPlayer == NULL){
    	CDXPlayer_Create((void**)&mPlayer);
    	mPlayer->control(mPlayer, CDX_CMD_REGISTER_CALLBACK, (unsigned int)&CedarXPlayerCallbackWrapper, (unsigned int)this);
    	isCedarXInitialized = true;
    }

    //mPlayer->control(mPlayer, CDX_CMD_SET_STATE, CDX_STATE_UNKOWN, 0);

    err = setDataSource(state->mUri, &state->mUriHeaders);
	mPlayer->control(mPlayer, CDX_CMD_SET_NETWORK_ENGINE, CEDARX_NETWORK_ENGINE_SFT, 0);
	mPlayer->control(mPlayer, CDX_CMD_SET_VIDEO_OUTPUT_SETTING, mExtendMember->mOutputSetting, 0);

    mFlags = state->mFlags & (AUTO_LOOPING | LOOPING | AT_EOS);

    mFlags |= RESTORE_CONTROL_PARA;

    if (state->mFlags & PLAYING) {
        play_l(CDX_CMD_TAG_START_ASYNC);
    }
    mFlags &= ~SUSPENDING;
    //state->mPositionUs = 0;
    mPlayerState = PLAYER_STATE_RESUME;

    LOGD("resume end");

	return OK;
}

status_t CedarXPlayer::setScreen(int screen) {
	mScreenID = screen;
	LOGV("CedarX will setScreen to:%d", screen);
	if(mVideoRenderer != NULL && !(mFlags & SUSPENDING)){
		LOGV("CedarX setScreen to:%d", screen);
		return OK; //no need to setscreen from android4.0 v1.5 version
		//return mVideoRenderer->control(VIDEORENDER_CMD_SETSCREEN, screen);
	}
	return UNKNOWN_ERROR;
}

status_t CedarXPlayer::set3DMode(int source3dMode, int displayMode)
{
	video3Dinfo_t _3d_info;

	_3d_info.width 	 			= mDisplayWidth;
	_3d_info.height	 			= mDisplayHeight;
	_3d_info.format	 			= mDisplayFormat;

	//* set source 3d mode.
	if(source3dMode == CEDARV_3D_MODE_DOUBLE_STREAM)
		_3d_info.src_mode = HWC_3D_SRC_MODE_FP;
	else if(source3dMode == CEDARV_3D_MODE_SIDE_BY_SIDE)
		_3d_info.src_mode = HWC_3D_SRC_MODE_SSH;
	else if(source3dMode == CEDARV_3D_MODE_TOP_TO_BOTTOM)
		_3d_info.src_mode = HWC_3D_SRC_MODE_TB;
	else if(source3dMode == CEDARV_3D_MODE_LINE_INTERLEAVE)
		_3d_info.src_mode = HWC_3D_SRC_MODE_LI;
	else
		_3d_info.src_mode = HWC_3D_SRC_MODE_NORMAL;

	//* set display 3d mode.
	if(displayMode == CEDARX_DISPLAY_3D_MODE_ANAGLAGH) {
		if(source3dMode == CEDARV_3D_MODE_SIDE_BY_SIDE) {
			_3d_info.width = mDisplayWidth/2;//frm_inf->width /=2;
			_3d_info.width = (_3d_info.width + 0xF) & 0xFFFFFFF0;
		}
		else if(source3dMode == CEDARV_3D_MODE_TOP_TO_BOTTOM) {
			_3d_info.height = mDisplayHeight/2;//frm_inf->height /=2;
			_3d_info.height = (_3d_info.height + 0xF) & 0xFFFFFFF0;
		}

		if(_3d_info.format != HWC_FORMAT_RGBA_8888)
			_3d_info.format = HWC_FORMAT_RGBA_8888;		//* force pixel format to be RGBA8888.

		_3d_info.display_mode = HWC_3D_OUT_MODE_ANAGLAGH;
	}
	else if(displayMode == CEDARX_DISPLAY_3D_MODE_3D)
		_3d_info.display_mode = HWC_3D_OUT_MODE_HDMI_3D_1080P24_FP;
	else if(displayMode == CEDARX_DISPLAY_3D_MODE_2D)
		_3d_info.display_mode = HWC_3D_OUT_MODE_ORIGINAL;
	else
		_3d_info.display_mode = HWC_3D_OUT_MODE_2D;
	if(mVideoRenderer != NULL) {
		LOGV("set hdmi, src_mode = %d, dst_mode = %d", _3d_info.src_mode, _3d_info.display_mode);
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

	_3d_mode = (cedarv_3d_mode_e)type;
	if(mPlayer->control(mPlayer, CDX_CMD_SET_PICTURE_3D_MODE, _3d_mode, 0) != 0)
		return -1;

	//* the 3d mode you set may be invalid, get the valid 3d mode from mPlayer.
	mPlayer->control(mPlayer, CDX_CMD_GET_PICTURE_3D_MODE, (unsigned int)&_3d_mode, 0);

	LOGV("set 3d source mode to %d, current_3d_mode = %d", type, _3d_mode);

	return 0;
}

int CedarXPlayer::getInputDimensionType()
{
	cedarv_3d_mode_e tmp;

	if(mPlayer == NULL)
		return -1;

	pre_3d_mode = _3d_mode;
	mPlayer->control(mPlayer, CDX_CMD_GET_SOURCE_3D_MODE, (unsigned int)&tmp, 0);

	if((unsigned int)tmp != _3d_mode)
	{
		_3d_mode = tmp;
		LOGV("set _3d_mode to be %d when getting source mode", _3d_mode);
	}

	LOGV("_3d_mode = %d", _3d_mode);

	return (int)_3d_mode;
}

status_t CedarXPlayer::setOutputDimensionType(int type)
{
	status_t ret;
	if(mPlayer == NULL)
		return -1;

	ret = mPlayer->control(mPlayer, CDX_CMD_SET_DISPLAY_MODE, type, 0);

	LOGV("set output display mode to be %d, current display mode = %d", type, display_3d_mode);
	if(display_3d_mode != CEDARX_DISPLAY_3D_MODE_ANAGLAGH && type != CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
	{
		display_3d_mode = type;

		set3DMode(_3d_mode, display_3d_mode);
	}
	else
	{
		//* for switching on or off the anaglagh display, setting for display device(Overlay) will be
		//* done when next picture be render.
		if(type == CEDARX_DISPLAY_3D_MODE_ANAGLAGH && display_3d_mode != CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
		{
			//* switch on anaglagh display.
			if(anaglagh_en)
				wait_anaglagh_display_change = 1;
		}
		else if(type != CEDARX_DISPLAY_3D_MODE_ANAGLAGH && display_3d_mode == CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
		{
			//* switch off anaglagh display.
			display_type_tmp_save = type;
			wait_anaglagh_display_change = 1;
		}
		else
		{
			if(_3d_mode != pre_3d_mode)
			{
				display_type_tmp_save = type;
				wait_anaglagh_display_change = 1;
			}
		}
	}

	return ret;
}

int CedarXPlayer::getOutputDimensionType()
{
	if(mPlayer == NULL)
		return -1;

	LOGV("current display 3d mode = %d", display_3d_mode);
	return (int)display_3d_mode;
}

status_t CedarXPlayer::setAnaglaghType(int type)
{
	int ret;
	if(mPlayer == NULL)
		return -1;

	anaglagh_type = (cedarv_anaglath_trans_mode_e)type;
	ret = mPlayer->control(mPlayer, CDX_CMD_SET_ANAGLAGH_TYPE, type, 0);
	if(ret == 0)
		anaglagh_en = 1;
	else
		anaglagh_en = 0;

	return ret;
}

int CedarXPlayer::getAnaglaghType()
{
	if(mPlayer == NULL)
		return -1;

	return (int)anaglagh_type;
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

status_t CedarXPlayer::setChannelMuteMode(int muteMode)
{
	if(mPlayer == NULL){
		return -1;
	}

	mPlayer->control(mPlayer, CDX_CMD_SET_AUDIOCHANNEL_MUTE, muteMode, 0);
	return OK;
}

int CedarXPlayer::getChannelMuteMode()
{
	if(mPlayer == NULL){
		return -1;
	}

	int mute;
	mPlayer->control(mPlayer, CDX_CMD_GET_AUDIOCHANNEL_MUTE, (unsigned int)&mute, 0);
	return mute;
}

status_t CedarXPlayer::extensionControl(int command, int para0, int para1)
{
	return OK;
}

status_t CedarXPlayer::generalInterface(int cmd, int int1, int int2, int int3, void *p)
{
	if (mPlayer == NULL) {
		return -1;
	}

	switch (cmd) {

	case MEDIAPLAYER_CMD_SET_BD_FOLDER_PLAY_MODE:
		mPlayer->control(mPlayer, CDX_CMD_PLAY_BD_FILE, int1, 0);
		break;

	case MEDIAPLAYER_CMD_GET_BD_FOLDER_PLAY_MODE:
		mPlayer->control(mPlayer, CDX_CMD_IS_PLAY_BD_FILE, (unsigned int)p, 0);
		break;

	case MEDIAPLAYER_CMD_SET_STREAMING_TYPE:
		mPlayer->control(mPlayer, CDX_CMD_SET_STREAMING_TYPE, int1, 0);
		break;

	case MEDIAPLAYER_CMD_QUERY_HWLAYER_RENDER:
		*((int*)p) = mExtendMember->mUseHardwareLayer;
		break;

	default:
		break;
	}

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

//	if(mPlayer != NULL){
//		LOGV("I'm force to quit!");
//		mPlayer->control(mPlayer, CDX_CMD_STOP_ASYNC, 0, 0);
//	}

	suspend();

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
	android_native_rect_t crop;

	mDisplayWidth 	= width;
	mDisplayHeight 	= height;
	mFirstFrame 	= 1;
	mDisplayFormat 	= (format == 0x11) ? (int32_t)HWC_FORMAT_MBYUV422
			: ((format == 0xd) ? (int32_t)HAL_PIXEL_FORMAT_YV12 : (int32_t)HWC_FORMAT_MBYUV420);

	LOGD("video render size:%dx%d", width, height);

	if (mNativeWindow == NULL)
	    return -1;

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

    LOGV("StagefrightVideoRenderInit mScreenID:%d",mScreenID);

    mVideoRenderer.clear();

    // Must ensure that mVideoRenderer's destructor is actually executed
    // before creating a new one.
    IPCThreadState::self()->flushCommands();

    if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12)
    {
    	mVideoRenderer = new CedarXDirectHwRenderer(mNativeWindow, meta);

    	set3DMode(_3d_mode, display_3d_mode);
    }
    else
    {
    	mLocalRenderFrameIDCurr = -1;
    	mVideoRenderer = new CedarXLocalRenderer(mNativeWindow, meta);
    }
	
	if(frm_inf->rotate_angle != 0)
    {
		crop.top = frm_inf->top_offset;
		crop.left = frm_inf->left_offset;
		crop.right = frm_inf->width + frm_inf->left_offset;
		crop.bottom = frm_inf->height + frm_inf->top_offset;

		mVideoRenderer->control(VIDEORENDER_CMD_SET_CROP, (int)&crop);
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
	Mutex::Autolock autoLock(mLockNativeWindow);

	if(mVideoRenderer != NULL){
		cedarv_picture_t *frm_inf = (cedarv_picture_t *) frame_info;

		if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12) {
			libhwclayerpara_t overlay_para;

			memset(&overlay_para, 0, sizeof(libhwclayerpara_t));

			if(wait_anaglagh_display_change)
			{
				LOGV("+++++++++++++ display 3d mode == %d", display_3d_mode);
				if(display_3d_mode != CEDARX_DISPLAY_3D_MODE_ANAGLAGH)
				{
					//* switch on anaglagh display.
					if(anaglagh_type == (uint32_t)frm_inf->anaglath_transform_mode)
					{
						display_3d_mode = CEDARX_DISPLAY_3D_MODE_ANAGLAGH;
						set3DMode(_3d_mode, display_3d_mode);
						wait_anaglagh_display_change = 0;
					}
				}
				else
				{
					//* switch off anaglagh display.
					display_3d_mode = display_type_tmp_save;
					set3DMode(_3d_mode, display_3d_mode);
					wait_anaglagh_display_change = 0;
				}
			}

			overlay_para.bProgressiveSrc = frm_inf->is_progressive;
			overlay_para.bTopFieldFirst = frm_inf->top_field_first;
			overlay_para.pVideoInfo.frame_rate = frm_inf->frame_rate;
#if 1 //deinterlace support
			overlay_para.flag_addr              = frm_inf->flag_addr;
			overlay_para.flag_stride            = frm_inf->flag_stride;
			overlay_para.maf_valid              = frm_inf->maf_valid;
			overlay_para.pre_frame_valid        = frm_inf->pre_frame_valid;
#endif
			if(_3d_mode == CEDARV_3D_MODE_DOUBLE_STREAM && display_3d_mode == CEDARX_DISPLAY_3D_MODE_3D){
				overlay_para.top_y 		= (unsigned int)frm_inf->y;
				overlay_para.top_c 		= (unsigned int)frm_inf->u;
				overlay_para.bottom_y	= (unsigned int)frm_inf->y2;
				overlay_para.bottom_c	= (unsigned int)frm_inf->u2;
			}
			else if(display_3d_mode == CEDARX_DISPLAY_3D_MODE_ANAGLAGH){
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
			//LOGV("render frame id:%d",frame_id);
			if(mFirstFrame) {
				mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 1);
				mFirstFrame = 0;
			}
		}
		else {
			mVideoRenderer->render(frm_inf->y2, frm_inf->size_y2);
			mLocalRenderFrameIDCurr = frame_id;
		}
	}
	else {
		if (mDisplayFormat == HAL_PIXEL_FORMAT_YV12) {
			mLocalRenderFrameIDCurr = frame_id;
		}

		if (mNativeWindow != NULL) {
			sp<MetaData> meta = new MetaData;
			meta->setInt32(kKeyScreenID, mScreenID);
		    meta->setInt32(kKeyColorFormat, mDisplayFormat);
		    meta->setInt32(kKeyWidth, mDisplayWidth);
		    meta->setInt32(kKeyHeight, mDisplayHeight);

		    LOGV("reinit mVideoRenderer");
		    // Must ensure that mVideoRenderer's destructor is actually executed
		    // before creating a new one.
		    IPCThreadState::self()->flushCommands();

		    if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12)
		    {
		    	mVideoRenderer = new CedarXDirectHwRenderer(mNativeWindow, meta);

		    	set3DMode(_3d_mode, display_3d_mode);
		    }
		    else
		    {
		    	mVideoRenderer = new CedarXLocalRenderer(mNativeWindow, meta);
		    }
		}
	}
}

int CedarXPlayer::StagefrightVideoRenderGetFrameID()
{
	int ret = -1;

	if(mDisplayFormat != HAL_PIXEL_FORMAT_YV12) {
		if(mVideoRenderer != NULL) {
			ret = mVideoRenderer->control(VIDEORENDER_CMD_GETCURFRAMEPARA, 0);
		}
		//LOGV("get disp frame id:%d",ret);
	}
	else {
		ret = mLocalRenderFrameIDCurr;
	}

	//LOGV("get disp frame id:%d",ret);

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

	case CDX_EVENT_FATAL_ERROR:
        notifyListener_l(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, (int)para);
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

