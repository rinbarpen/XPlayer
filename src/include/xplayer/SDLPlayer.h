#pragma once

#include "xplayer/Player.h"
#include "xplayer/AVAudioBuffer.h"
#include "xplayer/AVThread.h"
#include "xplayer/AVQueue.h"
#include "xplayer/Resampler.h"
#include "xplayer/Converter.h"
#include "xplayer/AVClock.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_audio.h"
#include <memory>

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

  bool isAVStreamBoth() const { return enable_video_ && enable_audio_; }
  bool isVideoStreamOnly() const { return enable_video_ && !enable_audio_; }
  bool isAudioStreamOnly() const { return !enable_video_ && enable_audio_; }

private:
  bool checkConfig();
  bool expect(bool condition, const std::string &error);

  void onPlay();
  void onReadFrame();
  void onSDLAudioPlay(Uint8 *stream, int len);
  void onSDLVideoPlay();
  void onAudioDecodeFrame();
  void onVideoDecodeFrame();

  void videoDelay();

  void onPauseToggle();

  static SDL_PixelFormatEnum convertFFmpegPixelFormatToSDLPixelFormat(AVPixelFormat format);
  static int convertFFmpegSampleFormatToSDLSampleFormat(AVSampleFormat format);
  static void sdlAudioCallback(void *userdata, Uint8* stream, int len);

private:
  std::string url_;
  bool enable_video_{false};
  bool enable_audio_{false};
  AVFormatContext *format_context_;
  AVThread read_thread_{"ReadThread"};
  AVThread audio_decode_thread_{"AudioDecodeThread"};
  AVThread video_decode_thread_{"VideoDecodeThread"};
  AVThread play_thread_{"PlayThread"};
  int seq_;
  // ForwardGeneric seq_;
  Mutex::type read_mutex_;
  std::condition_variable continue_read_cond_;

  // audio
  int audio_stream_index_{-1};
  AVCodecContext *audio_codec_context_;
  AVPacketQueue audio_packet_queue_{kMaxAudioFrame};
  AVFrameQueue audio_frame_queue_{kMaxAudioFrame};
  // video
  int video_stream_index_{-1};
  AVCodecContext *video_codec_context_;
  AVPacketQueue video_packet_queue_{kMaxVideoFrame};
  AVFrameQueue video_frame_queue_{kMaxVideoFrame};

  SDL_AudioDeviceID audio_device_id_;
  std::unique_ptr<AVAudioBuffer> audio_buffer_;
  std::shared_ptr<uint8_t> audio_buf_;
  int audio_buf_index_;
  int audio_buf_size_;

  std::shared_ptr<Resampler> resampler_;
  std::shared_ptr<Converter> converter_;

  bool is_finished_{false};
  bool is_over_{false};
  bool need2pause_{false};
  // Sync
  // miliseconds
  int64_t seek_pos_;
  bool need2seek_{false};
  AVClock clocker_;
  int64_t last_paused_time_{0};  // for cache
  int audio_clock_serial_;
  AVSyncClock audio_clock_;
  AVSyncClock video_clock_;
  AVSyncClock *external_clock_; // always pointer to audio_clock

  // SDL2
  SDL_Window *window_;
  SDL_Renderer *renderer_;

  std::string error_;

  static constexpr size_t kMaxAudioFrame = 600;
  static constexpr size_t kMaxVideoFrame = 300;
  static constexpr size_t kMinAudioFrame = kMaxAudioFrame / 5;
  static constexpr size_t kMinVideoFrame = kMaxVideoFrame / 5;
  static constexpr size_t kMaxAudioBufferSize = 500 * 1000;
};
