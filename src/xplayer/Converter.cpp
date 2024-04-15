#include "xplayer/Converter.h"

std::shared_ptr<Converter> Converter::create(int srcWidth, int srcHeight,
                                             AVPixelFormat srcFormat,
                                             int dstWidth, int dstHeight,
                                             AVPixelFormat dstFormat) {
  auto pConverter = std::make_shared<Converter>();
  if (!pConverter->init(srcHeight, srcHeight, srcFormat, dstWidth, dstHeight,
                        dstFormat)) {
    return nullptr;
  }
  return pConverter;
}

Converter::Converter() : sws_context_(nullptr) {}
Converter::~Converter() {
  if (sws_context_) {
    sws_freeContext(sws_context_);
  }
}

bool Converter::init(int srcWidth, int srcHeight, AVPixelFormat srcFormat,
                     int dstWidth, int dstHeight, AVPixelFormat dstFormat) {
  sws_context_ =
      sws_getContext(srcWidth, srcHeight, srcFormat, dstWidth, dstHeight,
                     dstFormat, SWS_BICUBIC, nullptr, nullptr, nullptr);
  if (!sws_context_) return false;
  return true;
}
bool Converter::convert(AVFrame *pInFrame, AVFrame *&pOutFrame) {
  return sws_scale(sws_context_, pInFrame->data, pInFrame->linesize, 0,
                   pInFrame->height, pOutFrame->data, pOutFrame->linesize) > 0;
}
