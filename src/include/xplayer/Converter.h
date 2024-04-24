#pragma once

#include "xplayer/FFmpegUtil.h"

// TODO: pack the vars
class Converter {
public:
  struct Info
  {
    int width = 0;
    int height = 0;
    AVPixelFormat format = AV_PIX_FMT_NONE;

    bool operator==(const Info &info) const {
      return width == info.width && height == info.height && format == info.format;
    }
    bool operator==(Info &&info) const {
      return width == info.width && height == info.height && format == info.format;
    }
    bool operator!=(const Info &info) const {
      return !(*this == info);
    }
    bool operator!=(Info &&info) const {
      return !(*this == info);
    }
  };

public:
  Converter();
  ~Converter();

  static std::shared_ptr<Converter> create(int srcWidth, int srcHeight, AVPixelFormat srcFormat, int dstWidth,
            int dstHeight, AVPixelFormat dstFormat);

  bool init(int srcWidth, int srcHeight, AVPixelFormat srcFormat,
            int dstWidth, int dstHeight, AVPixelFormat dstFormat);
  bool convert(AVFramePtr pInFrame, AVFramePtr &pOutFrame);

private:
  bool isDirty(int dstWidth, int dstHeight, AVPixelFormat dstFormat) const;

private:
  int last_width_;
  int last_height_;
  AVPixelFormat last_format_;

  SwsContext *sws_context_;
};