#include "xplayer/Resampler.h"

std::shared_ptr<Resampler> Resampler::create(
    int srcChannelLayout, enum AVSampleFormat srcFormat, int srcSampleRate,
    int dstChannelLayout, enum AVSampleFormat dstFormat, int dstSampleRate) {
  auto pResampler = std::make_shared<Resampler>();
  if (!pResampler->init(srcChannelLayout, srcFormat, srcSampleRate,
                        dstChannelLayout, dstFormat, dstSampleRate)) {
    return nullptr;
  }
  return pResampler;
}

Resampler::Resampler() {}
Resampler::~Resampler() {
  if (swr_is_initialized(swr_context_)) {
    swr_free(&swr_context_);
  }
}

bool Resampler::init(int srcChannelLayout, enum AVSampleFormat srcFormat,
                     int srcSampleRate, int dstChannelLayout,
                     enum AVSampleFormat dstFormat, int dstSampleRate) {
  int r{-1};
  swr_context_ = swr_alloc_set_opts(nullptr, dstChannelLayout, dstFormat,
                                    dstSampleRate, srcChannelLayout, srcFormat,
                                    srcSampleRate, 0, nullptr);
  if (swr_context_ == nullptr) {
    return false;
  }

  r = swr_init(swr_context_);
  if (r < 0) {
    swr_free(&swr_context_);
    return false;
  }
  return true;
}

bool Resampler::resample(AVFrame *pInFrame, AVFrame *&pOutFrame) {
  return swr_convert_frame(swr_context_, pOutFrame, pInFrame) == 0;
}
