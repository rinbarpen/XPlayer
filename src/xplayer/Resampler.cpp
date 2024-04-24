#include "xplayer/Resampler.h"

std::shared_ptr<Resampler> Resampler::create(int srcChannels,
                                             AVSampleFormat srcFormat,
                                             int srcSampleRate, int dstChannels,
                                             AVSampleFormat dstFormat,
                                             int dstSampleRate) {
  auto pResampler = std::make_shared<Resampler>();
  if (!pResampler->init(srcChannels, srcFormat, srcSampleRate, dstChannels,
                        dstFormat, dstSampleRate)) {
    return nullptr;
  }
  return pResampler;
}

Resampler::Resampler() {}
Resampler::~Resampler() {
  if (swr_is_initialized(swr_context_)) {
    swr_free(&swr_context_);
    swr_context_ = nullptr;
  }
}

bool Resampler::init(int srcChannels, AVSampleFormat srcFormat,
                     int srcSampleRate, int dstChannels,
                     AVSampleFormat dstFormat, int dstSampleRate) {
  if (!isDirty(srcChannels, srcFormat, srcSampleRate) && !swr_context_) {
    return true;
  }

  if (swr_context_ && swr_is_initialized(swr_context_)) {
    swr_free(&swr_context_);
    swr_context_ = nullptr;
  }

  int r{-1};
  swr_context_ = swr_alloc_set_opts(
      nullptr, av_get_default_channel_layout(dstChannels), dstFormat,
      dstSampleRate, av_get_default_channel_layout(srcChannels), srcFormat,
      srcSampleRate, 0, nullptr);
  if (!swr_context_) return false;

  r = swr_init(swr_context_);
  if (r < 0) {
    swr_free(&swr_context_);
    swr_context_ = nullptr;
    return false;
  }
  last_channels_ = srcChannels;
  last_format_ = srcFormat;
  last_sample_rate_ = srcSampleRate;
  return true;
}

bool Resampler::resample(AVFramePtr pInFrame, AVFramePtr &pOutFrame) {
  // int64_t outCount = (int64_t)pInFrame->nb_samples * pOutFrame->sample_rate /
  //                        pInFrame->sample_rate +
  //                    256;
  // int outSize = av_samples_get_buffer_size(nullptr, pOutFrame->channels,
  //                          outCount,
  //                          (AVSampleFormat)pOutFrame->format, 1);
  // if (outSize < 0) return 0;

  // uint32_t size = 500 * 1000;
  // av_fast_malloc(&pOutFrame->extended_data, &size, outSize);
  // int len = swr_convert(swr_context_, &pOutFrame->extended_data[0], outCount,
  //                 (const uint8_t **)pInFrame->extended_data, pInFrame->nb_samples);

  // if (len <= 0 || len == outCount) {
  //   av_freep(&pOutFrame->extended_data);
  //   return 0;
  // }

  // return len * pOutFrame->channels * av_get_bytes_per_sample((AVSampleFormat)pOutFrame->format);
  return swr_convert_frame(swr_context_, pOutFrame.get(), pInFrame.get()) == 0;
}

bool Resampler::isDirty(int dstChannels, AVSampleFormat dstFormat,
                        int dstSampleRate) const {
  return last_channels_ != dstChannels || last_format_ != dstFormat ||
         last_sample_rate_ != dstSampleRate;
}
