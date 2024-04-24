#pragma once

#include <string>
#include <iostream>
#include "FFmpegUtil.h"

struct PlayerConfig {
  struct video {
    int width = 1280; // < 0 for automatically
    int height = 720; // < 0 for automatically
    int xleft = 0;
    int ytop = 0;
    AVPixelFormat format = AV_PIX_FMT_NONE;  // automatically
    float frame_rate = -1.0f;  // < 0 for automatically
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

  void dump(std::ostream& os) const {
    // Video
    if (enable_video) {
      os << "Video: \n";
      os << "\tWidth: " << video.width << "\n";
      os << "\tHeight: " << video.height << "\n";
      os << "\tX: " << video.xleft << "\n";
      os << "\tY: " << video.ytop << "\n";
      os << "\tFormat: " << video.format << "\n";
      os << "\tFrameRate: " << video.frame_rate << "\n";
    }
    // Audio
    if (enable_audio) {
      os << "Audio: \n";
      os << "\tChannels: " << audio.channels << "\n";
      os << "\tSample rate: " << audio.sample_rate << "\n";
      os << "\tFormat: " << audio.format << "\n";
      os << "\tBitrate: " << audio.bitrate << "\n";
      os << "\tVolume: " << audio.volume << "\n";
      os << "\tMuted: " << std::boolalpha << audio.is_muted << "\n";
    }
    // Common
    os << "Speed: " << common.speed << "\n";
  }
};
