// Simplest FFmpeg Audio Player.cpp : 定义控制台应用程序的入口点。

/**
* 最简单的基于FFmpeg的音频播放器 2
* Simplest FFmpeg Audio Player 2
*
* 原程序：
* 雷霄骅 Lei Xiaohua
* leixiaohua1020@126.com
* 中国传媒大学/数字电视技术
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* 修改：
* 刘文晨 Liu Wenchen
* 812288728@qq.com
* 电子科技大学/电子信息
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* 本程序实现了音频的解码和播放，是最简单的 FFmpeg 音频解码方面的教程。
* 通过学习本例子可以了解 FFmpeg 的解码流程。
*
* 该版本使用 SDL 2.0 替换了第一个版本中的 SDL 1.0。
* 注意：SDL 2.0 中音频解码的API并无变化。
* 唯一变化的地方在于其回调函数的中的 Audio Buffer 并没有完全初始化，需要手动初始化。
* 本例子中即 SDL_memset(stream, 0, len);
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

// 解决报错：无法解析的外部符号 __imp__fprintf，该符号在函数 _ShowError 中被引用
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C"
{
	// 解决报错：无法解析的外部符号 __imp____iob_func，该符号在函数 _ShowError 中被引用
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

// 1 second of 48kHz 32bit audio，单位是字节
#define MAX_AUDIO_FRAME_SIZE 192000

// Buffer:
// |-----------|-------------|
// chunk-------pos---len-----|
static  Uint8  *audio_chunk;
static  Uint32  audio_len;
static  Uint8  *audio_pos;

/* 音频回调函数
* 开始播放后，会有音频其他子线程来调用回调函数，进行音频数据的补充，经过测试每次补充 4096 个字节
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
	/* 混音播放函数
	* dst: 目标数据，这个是回调函数里面的 stream 指针指向的，直接使用回调的 stream 指针即可
	* src: 音频数据，这个是将需要播放的音频数据混到 stream 里面去，那么这里就是我们需要填充的播放的数据
	* len: 音频数据的长度
	* volume: 音量，范围 0~128 ，SAL_MIX_MAXVOLUME 为 128，设置的是软音量，不是硬件的音响
	*/
	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;
	audio_len -= len;
}


int main(int argc, char* argv[])
{
	// FFmpeg 结构体及变量定义
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

	// 输出文件路径
	FILE *pFile = fopen("output.pcm", "wb");
	// 输入文件路径
	char url[] = "xiaoqingge.mp3";

	// 注册支持的所有的文件格式（容器）及其对应的 CODEC，只需要调用一次
	av_register_all();
	// 对网络库进行全局初始化（加载 socket 库以及网络加密协议相关的库，为后续使用网络相关提供支持）
	// 注意：此函数仅用于解决旧 GnuTLS 或 OpenSSL 库的线程安全问题。
	// 如果 libavformat 链接到这些库的较新版本，或者不使用它们，则无需调用此函数。
	// 否则，需要在使用它们的任何其他线程启动之前调用此函数。
	avformat_network_init();
	// 使用默认参数分配并初始化一个 AVFormatContext 对象
	pFormatCtx = avformat_alloc_context();
	// 打开输入媒体流，分配编解码器上下文、解复用上下文、I/O 上下文
	if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0)
	{
		printf("Can't open input stream.\n");
		return -1;
	}
	// 读取媒体文件的数据包以获取媒体流信息
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("Can't find stream information.\n");
		return -1;
	}

	printf("---------------- File Information ---------------\n");
	// 将 AVFormatContext 结构体中媒体文件的信息进行格式化输出
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
	// pCodecCtx 是指向音频流的编解码器上下文的指针
	pCodecCtx = pFormatCtx->streams[audioStream]->codec;
	// 查找 ID 为 pCodecCtx->codec_id 的已注册的音频流解码器
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	// 初始化指定的编解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Can't open codec.\n");
		return -1;
	}

	// 为 packet 分配空间
	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	// 将 packet 中的可选字段初始化为默认值
	av_init_packet(packet);

	// 输出音频参数
	// 通道类型：双声道
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = pCodecCtx->frame_size;
	// 采样格式：pcm_s16le（整型 16bit）
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// 采样率：44100
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();

	// ------------------------------ SDL ------------------------------
	// ---------------------------- SDL END ----------------------------
	// 初始化音频子系统和计时器子系统
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s.\n", SDL_GetError());
		return -1;
	}
	// 根据音频信息打开音频设备
	// SDL_AudioSpec 是包含音频输出格式的结构体，同时它也包含当音频设备需要更多数据时调用的回调函数
	wanted_spec.freq = out_sample_rate; // 采样率
	wanted_spec.format = AUDIO_S16SYS; // 音频数据格式
	wanted_spec.channels = out_channels; // 通道数
	wanted_spec.silence = 0; // 音频缓冲静音值
	wanted_spec.samples = out_nb_samples; // 基本是 512、1024，设置不合适可能会导致卡顿
	wanted_spec.callback = fill_audio;// 为音频设备提供数据回调（空值使用 SDL 自身预先定义的SDL_QueueAudio() 回调函数）
	wanted_spec.userdata = pCodecCtx;
	// 使用所需参数打开音频设备
	if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
	{
		printf("Can't open audio.\n");
		return -1;
	}
	// ---------------------------- SDL END ----------------------------

	// 根据声道数目获取声道布局
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	// 创建重采样结构体 SwrContext 对象
	au_convert_ctx = swr_alloc();
	// 设置重采样的转换参数
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	// 初始化 SwrContext 对象
	swr_init(au_convert_ctx);

	// 开始播放
	SDL_PauseAudio(0);

	// 读取码流中的音频若干帧或者视频一帧
	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		if (packet->stream_index == audioStream)
		{
			// 解码 packet 中的音频数据，pFrame 存储解码数据
			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
				printf("Error in decoding audio frame.\n");
				return -1;
			}
			if (got_picture > 0)
			{
				// 进行格式转换，返回值为实际转换的采样数
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples);
				// 打印每一帧的信息
				printf("index: %5d\t pts: %lld\t packet size: %d\n", index, packet->pts, packet->size);
				// 向输出文件中写入 PCM 数据
				fwrite(out_buffer, 1, out_buffer_size, pFile);
				index++;
			}
			// Wait until finish
			while (audio_len > 0)
			{
				// 使用 SDL_Delay 进行 1ms 的延迟，用当前缓存区剩余未播放的长度大于 0 结合前面的延迟进行等待
				SDL_Delay(1);
			}
			// Set audio buffer (PCM data)
			audio_chunk = (Uint8 *)out_buffer;
			// Audio buffer length
			audio_len = out_buffer_size;
			audio_pos = audio_chunk;
		}
		// 清空 packet 里面的数据
		av_free_packet(packet);
	}

	// 释放 SwrContext 对象
	swr_free(&au_convert_ctx);
	// 关闭音频设备
	SDL_CloseAudio();
	// 退出 SDL 系统
	SDL_Quit();
	// 关闭文件指针
	fclose(pFile);
	// 释放内存
	av_free(out_buffer);
	// 关闭解码器
	avcodec_close(pCodecCtx);
	// 关闭输入音频文件
	avformat_close_input(&pFormatCtx);

	system("pause");
	return 0;
}

