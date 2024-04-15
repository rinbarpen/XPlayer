#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}  //

using AVPacketPtr = std::shared_ptr<AVPacket>;
using AVFramePtr = std::shared_ptr<AVFrame>;

static AVPacketPtr makeAVPacket()
{
  AVPacketPtr pkt(av_packet_alloc(), [](AVPacket *pkt){
    av_packet_free(&pkt);
  });
  return pkt;
}
static AVFramePtr makeAVFrame()
{
  AVFramePtr frame(av_frame_alloc(), [](AVFrame *frame){
    av_frame_free(&frame);
  });
  return frame;
}
