#pragma once

#include "xplayer/FFmpegUtil.h"

class Converter {
public:
  Converter();
  ~Converter();

  static std::shared_ptr<Converter> create(int srcWidth, int srcHeight, AVPixelFormat srcFormat, int dstWidth,
            int dstHeight, AVPixelFormat dstFormat);

  bool init(int srcWidth, int srcHeight, AVPixelFormat srcFormat, int dstWidth,
            int dstHeight, AVPixelFormat dstFormat);
  bool convert(AVFrame *pInFrame, AVFrame *&pOutFrame);

private:
  SwsContext *sws_context_;
};