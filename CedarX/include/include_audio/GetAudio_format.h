/******************************************************************************
* file:GetAudio_format.h 2008-12-24 10:47:12
*
*author lszhang
*
*
*
*******************************************************************************/
#ifndef _GETAUFIO_FORMAT_H_
#define _GETAUFIO_FORMAT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
typedef enum __AW_AUDIO_FORMAT
{
    SW_AUDIO_FORMAT_UNKNOWN = 0,            /* �޷�ʶ�� */
    
    SW_AUDIO_FORMAT_AAC,
    SW_AUDIO_FORMAT_AC3,
    SW_AUDIO_FORMAT_APE,
    SW_AUDIO_FORMAT_DTS,
    SW_AUDIO_FORMAT_FLAC,
    SW_AUDIO_FORMAT_MP3,
    SW_AUDIO_FORMAT_OGG,
    SW_AUDIO_FORMAT_RA,
    SW_AUDIO_FORMAT_WAV,    
    SW_AUDIO_FORMAT_WMA,
    SW_AUDIO_FORMAT_AMR,
	SW_AUDIO_FORMAT_ATRC,
	SW_AUDIO_FORMAT_MID,
        
    SW_AUDIO_FORMAT_ = -1

} __sw_audio_format_e;

typedef enum
{
	IMG_FORMAT_BMP =0,
	IMG_FORMAT_JPG,
	IMG_FORMAT_GIF,
	IMG_FORMAT_PNG,
	IMG_FORMAT_UNSUPPORT = -1
}__w_img_format_t;

typedef enum __A_AUDIO_FONTTYPE
{
	  A_AUDIO_FONTTYPE_ISOIEC8859_1  = 0,       //ISO/IEC 8859-1
	  A_AUDIO_FONTTYPE_UTF_16LE,//
	  A_AUDIO_FONTTYPE_UTF_16BE,
	  A_AUDIO_FONTTYPE_UTF_8,//
	  A_AUDIO_FONTTYPE_ISOIEC8859_2,//
	  A_AUDIO_FONTTYPE_ISOIEC8859_3,//
	  A_AUDIO_FONTTYPE_ISOIEC8859_4,//
	  A_AUDIO_FONTTYPE_ISOIEC8859_5,//
	  A_AUDIO_FONTTYPE_ISOIEC8859_6,
	  A_AUDIO_FONTTYPE_ISOIEC8859_7,
	  A_AUDIO_FONTTYPE_ISOIEC8859_8,
	  A_AUDIO_FONTTYPE_ISOIEC8859_9,
	  A_AUDIO_FONTTYPE_ISOIEC8859_10,
	  A_AUDIO_FONTTYPE_ISOIEC8859_11,
	  A_AUDIO_FONTTYPE_ISOIEC8859_12,
	  A_AUDIO_FONTTYPE_ISOIEC8859_13,
	  A_AUDIO_FONTTYPE_ISOIEC8859_14,
	  A_AUDIO_FONTTYPE_ISOIEC8859_15,
	  A_AUDIO_FONTTYPE_ISOIEC8859_16,
	  A_AUDIO_FONTTYPE_WINDOWS_1250,
	  A_AUDIO_FONTTYPE_WINDOWS_1251,//
	  A_AUDIO_FONTTYPE_WINDOWS_1252,
	  A_AUDIO_FONTTYPE_WINDOWS_1253,
	  A_AUDIO_FONTTYPE_WINDOWS_1254,
	  A_AUDIO_FONTTYPE_WINDOWS_1255,
	  A_AUDIO_FONTTYPE_WINDOWS_1256,
	  A_AUDIO_FONTTYPE_WINDOWS_1257,
	  A_AUDIO_FONTTYPE_WINDOWS_1258,
	  A_AUDIO_FONTTYPE_KOI8_R,
	  A_AUDIO_FONTTYPE_KOI8_U,
	  A_AUDIO_FONTTYPE_GB2312,
	  A_AUDIO_FONTTYPE_GBK,
	  A_AUDIO_FONTTYPE_BIG5,
		
	  
	  A_AUDIO_FONTTYPE_ = -1
}__a_audio_fonttype_e;

typedef struct __ID3_IMAGE_INFO
{
    int				length;         //��ݳ���
    int				FileLocation;   //�ļ�ƫ��λ��
    __w_img_format_t	img_format;     //ͼƬ��ʽ
    int				pic_type;       //picture type;
    int				img_high;       //Ԥ����ͼƬ�߶�
    int				img_width;      //Ԥ����ͼƬ���
    int				otherdata;      //Ԥ��

}__id3_image_info_t;


typedef struct __AUDIO_FILE_INFO
{
    unsigned int					ulSampleRate;   // ������ sample rate
    unsigned int					ulBitRate;      // ������ bit rate��λ��BPS
    unsigned int					ulChannels;     // ����� channel
    unsigned int					ulDuration;     // ����ʱ�� duration ��λ��ms
    unsigned int					ulBitsSample;   // �����λ�� sample 8/16/24/32
    unsigned int					ulCharEncode;   // 0:GB2312.1:UNICODE

    int					ulAudio_name_sz;        // ��Ƶ��ʽ˵��
    signed char					*ulAudio_name;          // mp3 /RealAudio Cook.sipo. / aac-lc....
    __a_audio_fonttype_e	ulAudio_nameCharEncode; // 

    int					ulGenre_sz;             // ����
    signed char					*ulGenre;               // pop soft...
    __a_audio_fonttype_e	ulGenreCharEncode;

    int					ultitle_sz;             // ������
    signed char					*ultitle;
   __a_audio_fonttype_e		ultitleCharEncode;

    int					ulauthor_sz;            // �ݳ���
    signed char					*ulauthor;
    __a_audio_fonttype_e	ulauthorCharEncode;

    __sw_audio_format_e		ulFormat;
    int					ulAlbum_sz;             // ר��
    signed char					*ulAlbum;
    __a_audio_fonttype_e	ulAlbumCharEncode;

    int					ulYear_sz;              // ��Ʒ���
    signed char					*ulYear;
    __a_audio_fonttype_e	ulYearCharEncode;

    int					ulAPic_sz;             // attached picture
    __id3_image_info_t		*ulAPic;
    __a_audio_fonttype_e	ulAPicCharEncode;

    int					ulUslt_sz;             // ��ͬ���ĸ��/�ı� ����
    signed char*					ulUslt;                // int ulFileLocation��
    __a_audio_fonttype_e	ulUsltCharEncode;

    int					ulSylt_sz;             // ͬ���ĸ��/�ı�
    signed char*					ulSylt;                // int ulFileLocation��
    __a_audio_fonttype_e	ulSyltCharEncode;

    int                   ulbufflag;				//0:���ļ����з����������buf�������ݽ��з���
	int                   ulbuf_length;			//buf length;
	char*					ulbuf;					//����buf��������.
	int					data[9];

    signed char					*ulBSINFO;              // temporary buffer of read data
    int					InforBufLeftLength;
    signed char					*InforBuf;              // ���ڴ洢 ulAudio_name  ultitle ulauthor����Ϣ��buffer

}audio_file_info_t;


int GetAudioFormat(const char *pFilename,int *A_Audio_Format);//return 1 :succed 0 :fail
int GetAudioInfo(const char *pFilename, audio_file_info_t *AIF);  //return 1 :succed 0 :fail
int GetAudioDataInfo(const char *pFilename, audio_file_info_t *AIF,signed char* buf,int datalen);  //return 1 :succed 0 :fail
int GetAudioFileInfo(FILE *Bitstream, audio_file_info_t *AIF);  //return 1 :succed 0 :fail

#ifdef __cplusplus
}
#endif

#endif //_GETAUFIO_FORMAT_H_

