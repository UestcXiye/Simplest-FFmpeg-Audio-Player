// Simplest FFmpeg Audio Player.cpp : �������̨Ӧ�ó������ڵ㡣

/**
* ��򵥵Ļ���FFmpeg����Ƶ������ 2
* Simplest FFmpeg Audio Player 2
*
* ԭ����
* ������ Lei Xiaohua
* leixiaohua1020@126.com
* �й���ý��ѧ/���ֵ��Ӽ���
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* �޸ģ�
* ���ĳ� Liu Wenchen
* 812288728@qq.com
* ���ӿƼ���ѧ/������Ϣ
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* ������ʵ������Ƶ�Ľ���Ͳ��ţ�����򵥵� FFmpeg ��Ƶ���뷽��Ľ̡̳�
* ͨ��ѧϰ�����ӿ����˽� FFmpeg �Ľ������̡�
*
* �ð汾ʹ�� SDL 2.0 �滻�˵�һ���汾�е� SDL 1.0��
* ע�⣺SDL 2.0 ����Ƶ�����API���ޱ仯��
* Ψһ�仯�ĵط�������ص��������е� Audio Buffer ��û����ȫ��ʼ������Ҫ�ֶ���ʼ����
* �������м� SDL_memset(stream, 0, len);
*
* This software decode and play audio streams.
* Suitable for beginner of FFmpeg.
*
* This version use SDL 2.0 instead of SDL 1.2 in version 1
* Note:The good news for audio is that, with one exception,
* it's entirely backwards compatible with 1.2.
* That one really important exception: The audio callback
* does NOT start with a fully initialized buffer anymore.
* You must fully write to the buffer in all cases. In this
* example it is SDL_memset(stream, 0, len);
*
* Version 2.0
*/


#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

// ��������޷��������ⲿ���� __imp__fprintf���÷����ں��� _ShowError �б�����
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C"
{
	// ��������޷��������ⲿ���� __imp____iob_func���÷����ں��� _ShowError �б�����
	FILE __iob_func[3] = { *stdin, *stdout, *stderr };
}

#define __STDC_CONSTANT_MACROS
#ifdef _WIN32
// Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "SDL2/SDL.h"
};
#else
// Linux
#endif

// 1 second of 48kHz 32bit audio����λ���ֽ�
#define MAX_AUDIO_FRAME_SIZE 192000

// Buffer:
// |-----------|-------------|
// chunk-------pos---len-----|
static  Uint8  *audio_chunk;
static  Uint32  audio_len;
static  Uint8  *audio_pos;

/* ��Ƶ�ص�����
* ��ʼ���ź󣬻�����Ƶ�������߳������ûص�������������Ƶ���ݵĲ��䣬��������ÿ�β��� 4096 ���ֽ�
* The audio function callback takes the following parameters:
* stream: A pointer to the audio buffer to be filled
* len: The length (in bytes) of the audio buffer
*
*/
void  fill_audio(void *udata, Uint8 *stream, int len)
{
	// SDL 2.0
	SDL_memset(stream, 0, len);
	if (audio_len == 0) /* Only play if we have data left */
		return;
	len = (len > audio_len ? audio_len : len); /* Mix as much data as possible */
	/* �������ź���
	* dst: Ŀ�����ݣ�����ǻص���������� stream ָ��ָ��ģ�ֱ��ʹ�ûص��� stream ָ�뼴��
	* src: ��Ƶ���ݣ�����ǽ���Ҫ���ŵ���Ƶ���ݻ쵽 stream ����ȥ����ô�������������Ҫ���Ĳ��ŵ�����
	* len: ��Ƶ���ݵĳ���
	* volume: ��������Χ 0~128 ��SAL_MIX_MAXVOLUME Ϊ 128�����õ���������������Ӳ��������
	*/
	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;
	audio_len -= len;
}


int main(int argc, char* argv[])
{
	// FFmpeg �ṹ�弰��������
	AVFormatContext* pFormatCtx;
	int i, audioStream;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket* packet;
	uint8_t* out_buffer;
	AVFrame* pFrame;
	int ret;
	uint32_t len = 0;
	int got_picture;
	int index = 0;
	int64_t in_channel_layout;
	struct SwrContext *au_convert_ctx;

	// SDL
	SDL_AudioSpec wanted_spec;

	// ����ļ�·��
	FILE *pFile = fopen("output.pcm", "wb");
	// �����ļ�·��
	char url[] = "xiaoqingge.mp3";

	// ע��֧�ֵ����е��ļ���ʽ�������������Ӧ�� CODEC��ֻ��Ҫ����һ��
	av_register_all();
	// ����������ȫ�ֳ�ʼ�������� socket ���Լ��������Э����صĿ⣬Ϊ����ʹ����������ṩ֧�֣�
	// ע�⣺�˺��������ڽ���� GnuTLS �� OpenSSL ����̰߳�ȫ���⡣
	// ��� libavformat ���ӵ���Щ��Ľ��°汾�����߲�ʹ�����ǣ���������ô˺�����
	// ������Ҫ��ʹ�����ǵ��κ������߳�����֮ǰ���ô˺�����
	avformat_network_init();
	// ʹ��Ĭ�ϲ������䲢��ʼ��һ�� AVFormatContext ����
	pFormatCtx = avformat_alloc_context();
	// ������ý���������������������ġ��⸴�������ġ�I/O ������
	if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0)
	{
		printf("Can't open input stream.\n");
		return -1;
	}
	// ��ȡý���ļ������ݰ��Ի�ȡý������Ϣ
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("Can't find stream information.\n");
		return -1;
	}

	printf("---------------- File Information ---------------\n");
	// �� AVFormatContext �ṹ����ý���ļ�����Ϣ���и�ʽ�����
	av_dump_format(pFormatCtx, 0, url, false);
	printf("-------------------------------------------------\n");

	audioStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStream = i;
			break;
		}
	}
	if (audioStream == -1)
	{
		printf("Can't find a audio stream.\n");
		return -1;
	}
	// pCodecCtx ��ָ����Ƶ���ı�����������ĵ�ָ��
	pCodecCtx = pFormatCtx->streams[audioStream]->codec;
	// ���� ID Ϊ pCodecCtx->codec_id ����ע�����Ƶ��������
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	// ��ʼ��ָ���ı������
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Can't open codec.\n");
		return -1;
	}

	// Ϊ packet ����ռ�
	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	// �� packet �еĿ�ѡ�ֶγ�ʼ��ΪĬ��ֵ
	av_init_packet(packet);

	// �����Ƶ����
	// ͨ�����ͣ�˫����
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = pCodecCtx->frame_size;
	// ������ʽ��pcm_s16le������ 16bit��
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// �����ʣ�44100
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();

	// ------------------------------ SDL ------------------------------
	// ---------------------------- SDL END ----------------------------
	// ��ʼ����Ƶ��ϵͳ�ͼ�ʱ����ϵͳ
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s.\n", SDL_GetError());
		return -1;
	}
	// ������Ƶ��Ϣ����Ƶ�豸
	// SDL_AudioSpec �ǰ�����Ƶ�����ʽ�Ľṹ�壬ͬʱ��Ҳ��������Ƶ�豸��Ҫ��������ʱ���õĻص�����
	wanted_spec.freq = out_sample_rate; // ������
	wanted_spec.format = AUDIO_S16SYS; // ��Ƶ���ݸ�ʽ
	wanted_spec.channels = out_channels; // ͨ����
	wanted_spec.silence = 0; // ��Ƶ���徲��ֵ
	wanted_spec.samples = out_nb_samples; // ������ 512��1024�����ò����ʿ��ܻᵼ�¿���
	wanted_spec.callback = fill_audio;// Ϊ��Ƶ�豸�ṩ���ݻص�����ֵʹ�� SDL ����Ԥ�ȶ����SDL_QueueAudio() �ص�������
	wanted_spec.userdata = pCodecCtx;
	// ʹ�������������Ƶ�豸
	if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
	{
		printf("Can't open audio.\n");
		return -1;
	}
	// ---------------------------- SDL END ----------------------------

	// ����������Ŀ��ȡ��������
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	// �����ز����ṹ�� SwrContext ����
	au_convert_ctx = swr_alloc();
	// �����ز�����ת������
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	// ��ʼ�� SwrContext ����
	swr_init(au_convert_ctx);

	// ��ʼ����
	SDL_PauseAudio(0);

	// ��ȡ�����е���Ƶ����֡������Ƶһ֡
	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		if (packet->stream_index == audioStream)
		{
			// ���� packet �е���Ƶ���ݣ�pFrame �洢��������
			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
				printf("Error in decoding audio frame.\n");
				return -1;
			}
			if (got_picture > 0)
			{
				// ���и�ʽת��������ֵΪʵ��ת���Ĳ�����
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples);
				// ��ӡÿһ֡����Ϣ
				printf("index: %5d\t pts: %lld\t packet size: %d\n", index, packet->pts, packet->size);
				// ������ļ���д�� PCM ����
				fwrite(out_buffer, 1, out_buffer_size, pFile);
				index++;
			}
			// Wait until finish
			while (audio_len > 0)
			{
				// ʹ�� SDL_Delay ���� 1ms ���ӳ٣��õ�ǰ������ʣ��δ���ŵĳ��ȴ��� 0 ���ǰ����ӳٽ��еȴ�
				SDL_Delay(1);
			}
			// Set audio buffer (PCM data)
			audio_chunk = (Uint8 *)out_buffer;
			// Audio buffer length
			audio_len = out_buffer_size;
			audio_pos = audio_chunk;
		}
		// ��� packet ���������
		av_free_packet(packet);
	}

	// �ͷ� SwrContext ����
	swr_free(&au_convert_ctx);
	// �ر���Ƶ�豸
	SDL_CloseAudio();
	// �˳� SDL ϵͳ
	SDL_Quit();
	// �ر��ļ�ָ��
	fclose(pFile);
	// �ͷ��ڴ�
	av_free(out_buffer);
	// �رս�����
	avcodec_close(pCodecCtx);
	// �ر�������Ƶ�ļ�
	avformat_close_input(&pFormatCtx);

	system("pause");
	return 0;
}

