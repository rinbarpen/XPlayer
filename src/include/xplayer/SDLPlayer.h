#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "xplayer/Player.h"
#include "xplayer/AVThread.h"
#include "xplayer//AVQueue.h"
#include "xplayer/Resampler.h"
#include "xplayer/Converter.h"

#include "SDL2/SDL.h"

// TODO: 使用 error_ 记录错误信息，支持音视频流同步播放，解决内存泄露问题
class SDLPlayer : public Player {
public:
  SDLPlayer();
  ~SDLPlayer();

  static std::shared_ptr<SDLPlayer> create(PlayerConfig config);

  bool init(PlayerConfig config) override;
  void destroy() override;
  bool openUrl(const std::string &url) override;

  bool play() override;
  bool replay() override;
  bool pause() override;
  void close() override;

  bool isPlaying() const override { return status_ == Player::PLAYING; }
  bool isPaused() const override { return status_ == Player::PAUSED; }

  // miliseconds
  void seek(int64_t position) override;
  // miliseconds
  int64_t getCurrentPosition() const override;
  // miliseconds
  int64_t getTotalTime() const override;

  std::string lastError() const { return error_; }

  std::string dump() const;

private:
  bool checkConfig();
  bool expect(bool condition, const std::string &error);

  void onSDLVideoPlay();
  void onSDLAudioPlay(Uint8 *stream, int len);

  static SDL_PixelFormatEnum convertFFmpegPixelFormatToSDLPixelFormat(AVPixelFormat format);
  static int convertFFmpegSampleFormatToSDLSampleFormat(AVSampleFormat format);
  static void sdlAudioCallback(void *userdata, Uint8* stream, int len);

private:
  bool enable_video_{false};
  bool enable_audio_{false};
  AVFormatContext *format_context_;
  // video
  int video_stream_index_{-1};
  AVCodecContext *video_codec_context_;
  AVFrameQueue video_frame_queue_;
  // audio
  int audio_stream_index_{-1};
  AVCodecContext *audio_codec_context_;
  SDL_AudioDeviceID audio_device_id_;
  AVFrameQueue audio_frame_queue_;

  std::shared_ptr<Resampler> resampler_;
  std::shared_ptr<Converter> converter_;

  AVDecodeThread video_decode_thread_;
  std::mutex video_mutex_;
  std::condition_variable video_cond_;
  AVDecodeThread audio_decode_thread_;
  std::mutex audio_mutex_;
  std::condition_variable audio_cond_;

  // Sync
  // microseconds
  int64_t video_clock_pos_;
  int64_t audio_clock_pos_;
  int64_t clock_pos_;
  // miliseconds
  int64_t seek_pos_;

  // SDL2
  SDL_Window *window_;
  SDL_Renderer *renderer_;
  SDL_Texture *texture_;
  SDL_Event event_;

  std::string error_;

  static constexpr int kMaxAudioFrame = 1000;
  static constexpr int kMaxVideoFrame = 500;
};
