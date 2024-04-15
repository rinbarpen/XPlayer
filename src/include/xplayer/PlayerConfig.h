#pragma once

#include "FFmpegUtil.h"

struct PlayerConfig {
  struct video {
    int width = 1280;
    int height = 720;
    AVPixelFormat format = AV_PIX_FMT_NONE;  // automatically
    int frame_rate = 30;
  } video;
  struct audio {
    int channels = 2;
    int sample_rate = 1024;
    AVSampleFormat format = AV_SAMPLE_FMT_NONE;  // automatically
    int bitrate = 0;
    float volume = 1.0f;
    bool is_muted = false;
  } audio;
  struct common {
    float speed = 1.0f;
  } common;
  bool enable_audio = true;
  bool enable_video = true;
  bool play_after_ready = true;
};
