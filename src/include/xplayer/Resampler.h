#pragma once

#include "FFmpegUtil.h"

// TODO: pack the vars
class Resampler {
public:
  struct Info
  {
    int channels = 0;
    AVSampleFormat format = AV_SAMPLE_FMT_NONE;
    int sample_rate = 0;

    bool operator==(const Info &info) const {
      return channels == info.channels && format == info.format && sample_rate == info.sample_rate;
    }
    bool operator==(Info &&info) const {
      return channels == info.channels && format == info.format && sample_rate == info.sample_rate;
    }
    bool operator!=(const Info &info) const {
      return !(*this == info);
    }
    bool operator!=(Info &&info) const {
      return !(*this == info);
    }
  };

public:
  Resampler();
  ~Resampler();

  static std::shared_ptr<Resampler> create(
    int srcChannels, AVSampleFormat srcFormat, int srcSampleRate,
    int dstChannels, AVSampleFormat dstFormat, int dstSampleRate);

  bool init(int srcChannels, AVSampleFormat srcFormat, int srcSampleRate,
            int dstChannels, AVSampleFormat dstFormat, int dstSampleRate);

  bool resample(AVFramePtr pInFrame, AVFramePtr &pOutFrame);

 private:
  bool isDirty(int dstChannels, AVSampleFormat dstFormat,
               int dstSampleRate) const;

 private:
  SwrContext *swr_context_;
  int last_channels_;
  AVSampleFormat last_format_;
  int last_sample_rate_;
};
