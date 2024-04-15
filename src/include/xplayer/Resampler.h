#pragma once

#include "FFmpegUtil.h"

class Resampler {
 public:
  Resampler();
  ~Resampler();

  static std::shared_ptr<Resampler> create(
      int srcChannelLayout, enum AVSampleFormat srcFormat, int srcSampleRate,
      int dstChannelLayout, enum AVSampleFormat dstFormat, int dstSampleRate);

  bool init(int srcChannelLayout, enum AVSampleFormat srcFormat,
            int srcSampleRate, int dstChannelLayout,
            enum AVSampleFormat dstFormat, int dstSampleRate);
  bool resample(AVFrame *pInFrame, AVFrame *&pOutFrame);

 private:
  SwrContext *swr_context_;
};