#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "xplayer/FFmpegUtil.h"

// Audio Resampling Context
static SwrContext *swr_ctx = nullptr;
static AVFormatContext *format_ctx = nullptr;
// Audio Stream Info
static int audio_stream_idx = -1;
static AVCodecContext *audio_codec_ctx = nullptr;

// SDL Audio Callback
static void audio_callback(void *userdata, Uint8 *stream, int len) {
  AVPacket packet;

  AVFrame audio_frame;
  int total_len = 0;
  while (total_len < len) {
    if (av_read_frame(format_ctx, &packet) < 0) {
      break;
    }
    if (packet.stream_index == audio_stream_idx) {
      int ret = avcodec_send_packet(audio_codec_ctx, &packet);
      if (ret < 0) {
        break;
      }
      ret = avcodec_receive_frame(audio_codec_ctx, &audio_frame);
      if (ret < 0) {
        break;
      }

      uint8_t *out[] = {stream + total_len};
      int out_samples = swr_convert(swr_ctx, out, audio_frame.nb_samples,
                                    (const uint8_t **)audio_frame.data,
                                    audio_frame.nb_samples);
      total_len += out_samples * audio_codec_ctx->channels *
                   av_get_bytes_per_sample(audio_codec_ctx->sample_fmt);
      av_frame_unref(&audio_frame);
    }
    av_packet_unref(&packet);
  }

  memset(stream + total_len, 0, len - total_len);
}

static int testSDLAudio() {
  // Initialize SDL2
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    return -1;
  }

  // Open Video File with FFmpeg
  if (avformat_open_input(&format_ctx,
                          "/home/youmu/Desktop/media/22⧸7 9thシングル『曇り空の向こうは晴れている』music video-MP4-1080p-Mh9E1iZxoHs.mp4", nullptr,
                          nullptr) != 0) {
    fprintf(stderr, "Cannot open input file\n");
    return -1;
  }
  if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
    fprintf(stderr, "Cannot find stream information\n");
    return -1;
  }

  // Find Audio Stream
  for (int i = 0; i < format_ctx->nb_streams; i++) {
    if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream_idx = i;
      break;
    }
  }
  if (audio_stream_idx == -1) {
    fprintf(stderr, "No audio stream found\n");
    return -1;
  }

  // Open Audio Codec
  AVCodec *audio_codec = avcodec_find_decoder(
      format_ctx->streams[audio_stream_idx]->codecpar->codec_id);
  audio_codec_ctx = avcodec_alloc_context3(audio_codec);
  avcodec_parameters_to_context(
      audio_codec_ctx, format_ctx->streams[audio_stream_idx]->codecpar);
  avcodec_open2(audio_codec_ctx, audio_codec, nullptr);

  // Initialize Audio Resampling Context
  swr_ctx = swr_alloc_set_opts(
      nullptr, av_get_default_channel_layout(audio_codec_ctx->channels),
      AV_SAMPLE_FMT_S16, audio_codec_ctx->sample_rate,
      av_get_default_channel_layout(audio_codec_ctx->channels),
      audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate, 0, nullptr);
  swr_init(swr_ctx);

  // SDL Audio Spec
  SDL_AudioSpec audio_spec;
  audio_spec.freq = audio_codec_ctx->sample_rate;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = audio_codec_ctx->channels;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = nullptr;

  // Open SDL Audio Device
  if (SDL_OpenAudio(&audio_spec, nullptr) < 0) {
    fprintf(stderr, "SDL_OpenAudio error: %s\n", SDL_GetError());
    return -1;
  }

  // Start Audio Playback
  SDL_PauseAudio(0);

  // Main Loop
  SDL_Event event;
  while (1) {
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT) {
      break;
    }
  }

  // Cleanup
  SDL_CloseAudio();
  SDL_Quit();
  avcodec_close(audio_codec_ctx);
  avcodec_free_context(&audio_codec_ctx);
  avformat_close_input(&format_ctx);
  swr_free(&swr_ctx);

  return 0;
}
