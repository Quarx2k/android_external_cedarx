/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

#ifndef CDX_Common_H_
#define CDX_Common_H_

#include <CDX_Types.h>
#include <CDX_PlayerAPI.h>
#include <CDX_Subtitle.h>
#include <CDX_Fileformat.h>

typedef enum CEDARX_SOURCETYPE{
	CEDARX_SOURCE_FD,
	CEDARX_SOURCE_FILEPATH,
	CEDARX_SOURCE_M3U8,
	CEDARX_SOURCE_DRAMBUFFER,
	CEDARX_SOURCE_SFT_STREAM, //for ics
	CEDARX_SOURCE_NORMAL_STREAM,
	CEDARX_SOURCE_WRITER_CALLBACK, //for recoder writer
}CEDARX_SOURCETYPE;

typedef enum MEDIA_3DMODE_TYPE{
	VIDEORENDER_3DMODE_NONE = 0, //don't touch it
	VIDEORENDER_3DMODE_LR_INTERLEAVE,
	VIDEORENDER_3DMODE_TD_INTERLEAVE
}MEDIA_3DMODE_TYPE;

typedef struct CedarXDataSourceDesc{
	CEDARX_THIRDPART_STREAMTYPE thirdpart_stream_type;
	CEDARX_STREAMTYPE stream_type;
	CEDARX_SOURCETYPE source_type;
	CEDARX_MEDIA_TYPE media_type;
	MEDIA_3DMODE_TYPE media_subtype_3d;

	void *stream_info; //used for m3u/ts

	char *buffer_data;
	int  buffer_data_length;
	reqdata_from_dram req_cb;

	char *source_url; //SetDataSource url
	CedarXExternFdDesc ext_fd_desc;

	void *url_headers;
	void *sft_stream_handle;
	void *sft_cached_source2;
	void *sft_http_source;
	CDX_S64 sft_stream_length;

	int  demux_type;
	int  httplive_use_mplayer;

	int  mp_stream_cache_size; //unit KByte used for mplayer cache size setting
	char* bd_source_url;
    int   playBDFile;
}CedarXDataSourceDesc;

typedef enum CDX_AUDIO_CODEC_TYPE {
	CDX_AUDIO_NONE = 0,
	CDX_AUDIO_UNKNOWN,
	CDX_AUDIO_MP1,
	CDX_AUDIO_MP2,
	CDX_AUDIO_MP3,
	CDX_AUDIO_MPEG_AAC_LC,
	CDX_AUDIO_AC3,
	CDX_AUDIO_DTS,
	CDX_AUDIO_LPCM_V,
	CDX_AUDIO_LPCM_A,
	CDX_AUDIO_ADPCM,
	CDX_AUDIO_PCM,
	CDX_AUDIO_WMA_STANDARD,
	CDX_AUDIO_FLAC,
	CDX_AUDIO_APE,
	CDX_AUDIO_OGG,
	CDX_AUDIO_RAAC,
	CDX_AUDIO_COOK,
	CDX_AUDIO_SIPR,
	CDX_AUDIO_ATRC,
	CDX_AUDIO_AMR,
	CDX_AUDIO_RA,

	CDX_AUDIO_MLP = CDX_AUDIO_UNKNOWN,
	CDX_AUDIO_PPCM = CDX_AUDIO_UNKNOWN,
	CDX_AUDIO_WMA_LOSS = CDX_AUDIO_UNKNOWN,
	CDX_AUDIO_WMA_PRO = CDX_AUDIO_UNKNOWN,
	CDX_AUDIO_MP3_PRO = CDX_AUDIO_UNKNOWN,
}CDX_AUDIO_CODEC_TYPE;

typedef enum CDX_DECODE_MODE {
	CDX_DECODER_MODE_NORMAL = 0,
	CDX_DECODER_MODE_RAWMUSIC,
}CDX_DECODE_MODE;

typedef struct CedarXScreenInfo {
	int screen_width;
	int screen_height;
}CedarXScreenInfo;

#endif
/* File EOF */
