#include "xplayer/SDLPlayer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#include "libavutil/avutil.h"
#include "xplayer/Converter.h"
#include "xplayer/FFmpegUtil.h"
#include "xplayer/Log.h"
#include "xplayer/Player.h"
#include "xplayer/common.h"

std::shared_ptr<SDLPlayer> SDLPlayer::create(PlayerConfig config) {
  auto pSDLPlayer = std::make_shared<SDLPlayer>();
  if (!pSDLPlayer->init(config)) {
    return nullptr;
  }
  return pSDLPlayer;
}

SDLPlayer::SDLPlayer() : Player() {
  avformat_network_init();
  avdevice_register_all();
  SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
  converter_ = std::make_shared<Converter>();
  resampler_ = std::make_shared<Resampler>();
}

SDLPlayer::~SDLPlayer() {
  // if (status_ != Player::INITED && status_ != Player::NONE)
  //   this->destroy();
  SDL_Quit();
}

bool SDLPlayer::init(PlayerConfig config) {
  if (!checkConfig()) return false;

  config_ = config;
  status_ = Player::INITED;
  return true;
}

void SDLPlayer::destroy() {
  ASSERT(status_ != Player::NONE);

  this->close();
  // SDL
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);

  status_ = Player::INITED;
}

// call this function after initialization or a file(url) is finished
bool SDLPlayer::openUrl(const std::string &url) {
  status_ = Player::OPENING;

  int r{-1};
  format_context_ = avformat_alloc_context();
  avformat_open_input(&format_context_, url.c_str(), nullptr, nullptr);
  r = avformat_find_stream_info(format_context_, nullptr);
  if (r < 0) {
    LOG_ERROR("[SDLPlayer] Failed to find stream info while opening {}", url);
    this->destroy();
    return false;
  }

  // for (int i = 0; i < format_context_->nb_streams; i++) {
  //   if (config_.enable_audio && audio_stream_index_ < 0 &&
  //       AVMEDIA_TYPE_AUDIO ==
  //           format_context_->streams[i]->codecpar->codec_type) {
  //     audio_stream_index_ = i;
  //   } else if (config_.enable_video && video_stream_index_ < 0 &&
  //              AVMEDIA_TYPE_VIDEO ==
  //                  format_context_->streams[i]->codecpar->codec_type) {
  //     video_stream_index_ = i;
  //   }
  // }
  audio_stream_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO,
                                            -1, -1, nullptr, 0);
  video_stream_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO,
                                            -1, -1, nullptr, 0);

  // Audio is opened?
  if (audio_stream_index_ < 0 && config_.enable_audio) {
    enable_audio_ = false;
  } else {
    enable_audio_ = config_.enable_audio;
  }
  // Video is opened?
  if (video_stream_index_ < 0 && config_.enable_video) {
    enable_video_ = false;
  } else {
    enable_video_ = config_.enable_video;
  }
  if (!enable_audio_ && !enable_video_) {
    LOG_ERROR("[SDLPlayer] No audio or video stream found while opening {}",
              url);
    this->destroy();
    return false;
  }

  if (enable_audio_) {
    auto pAudioParam = format_context_->streams[audio_stream_index_]->codecpar;
    auto pAudioDecoder = avcodec_find_decoder(pAudioParam->codec_id);
    audio_codec_context_ = avcodec_alloc_context3(pAudioDecoder);
    avcodec_parameters_to_context(audio_codec_context_, pAudioParam);
    r = avcodec_open2(audio_codec_context_, pAudioDecoder, nullptr);
    if (r < 0) {
      LOG_ERROR("[SDLPlayer] Failed to open audio codec while opening {}", url);
      this->destroy();
      return false;
    }

    SDL_AudioSpec wanted, obtained;
    SDL_memset(&wanted, 0, sizeof(wanted));
    wanted.freq = config_.audio.sample_rate;
    wanted.format = convertFFmpegSampleFormatToSDLSampleFormat(
        (AVSampleFormat)config_.audio.format);
    wanted.channels = config_.audio.channels;
    wanted.samples = 1024;
    wanted.callback = SDLPlayer::sdlAudioCallback;
    wanted.userdata = this;

    int num = SDL_GetNumAudioDevices(false);
    for (int i = 0; i < num; i++) {
      const char *name = SDL_GetAudioDeviceName(i, false);
      audio_device_id_ =
          SDL_OpenAudioDevice(nullptr, false, &wanted, &obtained, 0);
      if (audio_device_id_ > 0) break;
    }
    if (audio_device_id_ <= 0) {
      LOG_ERROR("[SDLPlayer] Failed to open audio device");
      this->destroy();
      return false;
    }

    audio_buffer_.reset(new AVAudioBuffer{kMaxAudioBufferSize});
    audio_packet_queue_.open();
  }

  url_ = "SDL Player: ";
  url_ += format_context_->url;

  if (enable_video_) {
    auto pVideoParam = format_context_->streams[video_stream_index_]->codecpar;
    auto pVideoDecoder = avcodec_find_decoder(pVideoParam->codec_id);
    video_codec_context_ = avcodec_alloc_context3(pVideoDecoder);
    avcodec_parameters_to_context(video_codec_context_, pVideoParam);
    r = avcodec_open2(video_codec_context_, pVideoDecoder, nullptr);
    if (r < 0) {
      LOG_ERROR("[SDLPlayer] Failed to open video codec while opening {}", url);
      this->destroy();
      return false;
    }

    if (config_.video.width < 0)
      config_.video.width = video_codec_context_->width;
    if (config_.video.height < 0)
      config_.video.height = video_codec_context_->height;
    // auto fit
    // float oldRatio = (float)config_.video.width / config_.video.height;
    // float newRatio =
    //     (float)video_codec_context_->width / video_codec_context_->height;
    // if (oldRatio != newRatio) {
    //   config_.video.height = config_.video.height *
    //                          video_codec_context_->height /
    //                          video_codec_context_->width;
    // }
    // LOG_INFO("{}x{} | {}x{}", video_codec_context_->width,
    //          video_codec_context_->height, config_.video.width,
    //          config_.video.height);

    auto avg_frame_rate =
        format_context_->streams[video_stream_index_]->avg_frame_rate;
    video_codec_context_->framerate = avg_frame_rate;
    if (avg_frame_rate.den && avg_frame_rate.num)
      config_.video.frame_rate = (float)avg_frame_rate.num / avg_frame_rate.den;
    else if (config_.video.frame_rate <= 0.0f) {
      exit(1);
    }

    window_ =
        SDL_CreateWindow(url_.c_str(), config_.video.xleft, config_.video.ytop,
                         config_.video.width, config_.video.height, 0);
    if (!window_) {
      LOG_ERROR("[SDLPlayer] Could not create window! SDL_ERROR: {}",
                SDL_GetError());
      this->destroy();
      return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
      LOG_ERROR("[SDLPlayer] Could not create renderer! SDL_ERROR: {}",
                SDL_GetError());
      this->destroy();
      return false;
    }

    video_packet_queue_.open();
  }

  LOG_INFO("[SDLPlayer] Loading video {}, length: {}", url, getTotalTime());
  // LOG_INFO("[SDLPlayer] Loading config...");
  // config_.dump(std::cout);

  is_finished_ = false;
  is_over_ = false;
  status_ = Player::READY;

  read_thread_.dispatch(&SDLPlayer::onReadFrame, this);
  if (enable_audio_) {
    audio_frame_queue_.open();
    audio_decode_thread_.dispatch(&SDLPlayer::onAudioDecodeFrame, this);
  }
  if (enable_video_) {
    video_frame_queue_.open();
    video_decode_thread_.dispatch(&SDLPlayer::onVideoDecodeFrame, this);
  }

  if (config_.play_after_ready) {
    return play();
  }
  return true;
}
bool SDLPlayer::play() {
  need2pause_ = false;

  // auto pauseDuration = av_gettime() - video_clock_.current();
  // video_clock_.setTs(pauseDuration + video_clock_.current());

  status_ = Player::PLAYING;
  // play_thread_.dispatch(&SDLPlayer::onPlay, this);
  onPlay();
  return true;
}
void SDLPlayer::onPlay() {
  // Control Audio Play
  if (enable_audio_) {
    audio_decode_thread_.open();
    SDL_LockAudioDevice(audio_device_id_);
    SDL_PauseAudioDevice(audio_device_id_, 0);
    SDL_UnlockAudioDevice(audio_device_id_);
  }
  // Control Video Play
  if (enable_video_) {
    video_decode_thread_.open();
    // TODO:
    // video_player_thread_ = std::thread([this]{
    this->onSDLVideoPlay();
    // });
  }
}
bool SDLPlayer::replay() {
  if (status_ != PAUSED) return false;

  need2pause_ = false;
#if 1
  av_read_play(format_context_);
#endif

  status_ = Player::PLAYING;
  return true;
}
bool SDLPlayer::pause() {
  if (status_ != PLAYING) return false;

  need2pause_ = true;
  last_paused_time_ = getCurrentPosition();
#if 1
  av_read_pause(format_context_);
#endif

  status_ = Player::PAUSED;
  return true;
}
void SDLPlayer::close() {
 is_finished_ = true;
  is_over_ = true;

  if (enable_audio_) {
    SDL_LockAudioDevice(audio_device_id_);
    SDL_PauseAudioDevice(audio_device_id_, 1);
    SDL_UnlockAudioDevice(audio_device_id_);
    SDL_CloseAudioDevice(audio_device_id_);
  }
  audio_packet_queue_.close();
  audio_frame_queue_.close();
  video_packet_queue_.close();
  video_frame_queue_.close();

  audio_decode_thread_.join();
  video_decode_thread_.join();
  read_thread_.join();

  audio_packet_queue_.flush();
  audio_frame_queue_.flush();
  video_packet_queue_.flush();
  video_frame_queue_.flush();

  if (video_codec_context_) {
    avcodec_free_context(&video_codec_context_);
    avcodec_close(video_codec_context_);
  }
  if (audio_codec_context_) {
    avcodec_free_context(&audio_codec_context_);
    avcodec_close(audio_codec_context_);
  }
  if (format_context_) {
    avformat_close_input(&format_context_);
    avformat_free_context(format_context_);
  }
  audio_stream_index_ = video_stream_index_ = -1;
  seek_pos_ = 0;
  audio_clock_.reset();
  video_clock_.reset();

  status_ = Player::INITED;
}

void SDLPlayer::seek(int64_t position) {
  if (!need2seek_) {
    int64_t targetPos = position * AV_TIME_BASE;
    if (targetPos >= getTotalTime() || targetPos < 0) {
      LOG_WARN("Invalid seek position, it is further");
      return;
    }
    seek_pos_ = targetPos;
    need2seek_ = true;
  }
}
// No Bugs!
int64_t SDLPlayer::getCurrentPosition() const {
  if (isVideoStreamOnly()) {
    return video_clock_.current() * av_q2d(format_context_->streams[video_stream_index_]->time_base) * 1000;
  }
  // if (isAudioStreamOnly()) {
  return audio_clock_.current()  * av_q2d(format_context_->streams[audio_stream_index_]->time_base) * 1000;
  // }
  // return enable_audio_ ? audio_clock_.current() : video_clock_.current();
}
int64_t SDLPlayer::getTotalTime() const {
  return format_context_->duration / 1000;
}

bool SDLPlayer::checkConfig() {
  bool isNoProblem = true;
  if (config_.enable_video) {
    // check video configurations
    isNoProblem &=
        expect(config_.video.frame_rate != 0,
               "Frame Rate mustn't be 0 on Video") &&
        expect(config_.video.width != 0, "Width mustn't be 0 on Video") &&
        expect(config_.video.height != 0, "Height mustn't be 0 on Video");
  }
  if (config_.enable_audio) {
    // check audio configurations
    isNoProblem &= expect(config_.audio.sample_rate > 0,
                          "Sample rate must be greater than 0 on Audio") &&
                   expect(config_.audio.volume >= 0.0f,
                          "Volume mustn't be a negative number on Audio");
  }

  return isNoProblem;
}
bool SDLPlayer::expect(bool condition, const std::string &error) {
  if (!condition) {
    error_ = error;
  }
  return condition;
}

void SDLPlayer::onReadFrame() {
  int r{-1};

  while (!is_over_) {
    if (is_finished_) return;

    // jump to the target frame
    if (need2seek_) {
      int64_t seekTarget = seek_pos_;
      r = av_seek_frame(format_context_, -1, seekTarget, AVSEEK_FLAG_BACKWARD);
      if (r < 0) {
        LOG_FATAL("[SDLPlayer] Failed to seek to {} while seeking", seekTarget);
        return;
      }
      seq_++;

      if (enable_audio_) {
        audio_packet_queue_.flush();
        audio_packet_queue_.step2nextSeq();
      }
      if (enable_video_) {
        video_packet_queue_.flush();
        video_packet_queue_.step2nextSeq();
      }
      need2seek_ = false;

      if (!config_.play_after_ready) {
        // step2nextFrame();
      }
    }

    {
      Mutex::ulock locker(read_mutex_);
      continue_read_cond_.wait_for(
          locker, std::chrono::milliseconds(10), [&]() {
            bool audio_is_full =
                enable_audio_ ? audio_packet_queue_.isFull() : false;
            bool video_is_full =
                enable_video_ ? video_packet_queue_.isFull() : false;
            bool is_paused = isPaused();
            if (audio_is_full || video_is_full || is_paused) {
              return false;
            }
            return true;
          });
    }

    AVPacketPtr pPkt = makeAVPacket();
    r = av_read_frame(format_context_, pPkt.get());
    if (r == AVERROR_EOF) {
      LOG_INFO("[SDLPlayer] End of file");
      is_finished_ = true;
      Mutex::ulock locker(read_mutex_);
      continue_read_cond_.wait_for(
          locker, std::chrono::milliseconds(10), [&]() {
            return false;
          });
      return;
    }
    else if (r < 0) {
      LOG_WARN("[SDLPlayer] Some errors on av_read_frame()");
      continue;
    }

    if (pPkt->stream_index == audio_stream_index_) {
      audio_packet_queue_.push(pPkt);
    }
    else if (pPkt->stream_index == video_stream_index_) {
      video_packet_queue_.push(pPkt);
    }
  }
}

void SDLPlayer::onVideoDecodeFrame() {
  int r{-1};
  while (!is_over_) {
    if (video_packet_queue_.isEmpty() && is_finished_) break;

    AVPacketPtr pPkt;
    if (!video_packet_queue_.pop(pPkt)) {
      continue_read_cond_.notify_one();
      continue;
    }
    // if (video_packet_queue_.seq() != seq_) {
    //   continue;
    // }

    r = avcodec_send_packet(video_codec_context_, pPkt.get());
    if (r < 0) {
      LOG_ERROR("[SDLPlayer] Error sending a packet for decoding");
      break;
    }
    while (true) {
      auto pFrame = makeAVFrame();
      r = avcodec_receive_frame(video_codec_context_, pFrame.get());
      if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
        break;
      else if (r < 0) {
        LOG_ERROR("[SDLPlayer] Video frame is broken while playing");
        break;
      }

      video_frame_queue_.push(pFrame);
    }
  }
}
void SDLPlayer::onAudioDecodeFrame() {
  int r{-1};
  while (!is_over_) {
    if (audio_packet_queue_.isEmpty() && is_finished_) break;

    AVPacketPtr pPkt;
    if (!audio_packet_queue_.pop(pPkt)) {
      continue_read_cond_.notify_one();
      continue;
    }
    // if (audio_packet_queue_.seq() != seq_) {
    //   avcodec_flush_buffers(audio_codec_context_);
    // }

    r = avcodec_send_packet(audio_codec_context_, pPkt.get());
    if (r < 0) {
      LOG_ERROR("[SDLPlayer] Error sending a packet for decoding");
      break;
    }
    while (true) {
      auto pFrame = makeAVFrame();
      r = avcodec_receive_frame(audio_codec_context_, pFrame.get());
      if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
        break;
      else if (r < 0) {
        LOG_ERROR("[SDLPlayer] Audio frame is broken while playing");
        break;
      }

      audio_frame_queue_.push(pFrame);

      if (pFrame->pts != AV_NOPTS_VALUE)
        audio_clock_.setTs(pFrame->pts + pFrame->nb_samples * AV_TIME_BASE /
                                             pFrame->sample_rate);
      else
        audio_clock_.setTs(-1);
    }
  }
}

void SDLPlayer::onSDLVideoPlay() {
  SDL_Event event;
  int r{-1};
  while (!is_over_) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        this->destroy();
        SDL_Quit();
        exit(0);
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_SPACE:
          this->onPauseToggle();
          break;
        }
        break;
      }
    }

    if (isPaused()) continue;

    if (video_frame_queue_.isEmpty() && is_finished_) break;

    AVFramePtr pFrame;
    if (!video_frame_queue_.pop(pFrame)) {
      continue;
    }

    // we get: frame
    // the things can be done in the zone: process the video frame
    auto pOutFrame = makeAVFrame();
    auto targetFormat = (config_.video.format == AV_PIX_FMT_NONE)
                            ? (AVPixelFormat)pFrame->format
                            : config_.video.format;
    converter_->init(pFrame->width, pFrame->height,
                     (AVPixelFormat)pFrame->format, config_.video.width,
                     config_.video.height, targetFormat);
    r = av_image_alloc(pOutFrame->data, pOutFrame->linesize,
                       config_.video.width, config_.video.height, targetFormat,
                       1);
    if (r < 0) {
      videoDelay();
      continue;
    }

    static int c = 0;
    bool success = converter_->convert(pFrame, pOutFrame);
    c += success;
    if (!success) {
      av_freep(&pOutFrame->data[0]);
      videoDelay();
      continue;
    }
    LOG_DEBUG("Convert fault count: {}", c);
    LOG_DEBUG("VideoPacketQueueSize: {}", video_packet_queue_.size());
    LOG_DEBUG("VideoFrameQueueSize: {}", video_frame_queue_.size());
    // TODO:

    // auto currTime = av_rescale_q(pFrame->pts, video_codec_context_->time_base,
                                //  AV_TIME_BASE_Q);
    auto currTs = pFrame->pts;
    video_clock_.setTs(currTs);
    int64_t secs = getCurrentPosition() / 1000;
    LOG_DEBUG("CurrentTimestamp: {} | {}m:{:02}s", secs,
              secs / 60, secs % 60);

    SDL_PixelFormatEnum format =
        convertFFmpegPixelFormatToSDLPixelFormat(config_.video.format);
    SDL_Texture *pTexture =
        SDL_CreateTexture(renderer_, format, SDL_TEXTUREACCESS_STREAMING,
                          config_.video.width, config_.video.height);
    if (pTexture == nullptr) {
      LOG_ERROR("[SDLPlayer] Failed to create texture while playing");
      av_freep(&pOutFrame->data[0]);
      videoDelay();
      break;
    }

    if (format == SDL_PIXELFORMAT_YV12 || format == SDL_PIXELFORMAT_IYUV)
      SDL_UpdateYUVTexture(pTexture, nullptr, pOutFrame->data[0],
                           pOutFrame->linesize[0], pOutFrame->data[1],
                           pOutFrame->linesize[1], pOutFrame->data[2],
                           pOutFrame->linesize[2]);
    else
      SDL_UpdateTexture(pTexture, nullptr, pOutFrame->data[0],
                        pOutFrame->linesize[0]);

    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, pTexture, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    SDL_DestroyTexture(pTexture);

    // every frame is converted.
    // the data which stores image is allocated in the heap, so we need
    // to free it here
    av_freep(&pOutFrame->data[0]);
    videoDelay();
  }

  destroy();
}
void SDLPlayer::onSDLAudioPlay(Uint8 *stream, int len) {
  if (isPaused()) return;

  int len1;
  // TODO: implement me!
  while (len > 0) {
    bool silent = false;
    if (audio_buffer_->remain() <= 0) {
      AVFramePtr pFrame;
      bool success = audio_frame_queue_.pop(pFrame);
      silent = !success;
#if 1
      audio_buffer_->fill(pFrame->extended_data[0], pFrame->linesize[0]);
#else
      if (success) {
        auto pOutFrame = makeAVFrame();
        auto targetFormat = config_.audio.format == AV_SAMPLE_FMT_NONE
                                ? (AVSampleFormat)pFrame->format
                                : config_.audio.format;

        pOutFrame->format = targetFormat;
        pOutFrame->sample_rate = config_.audio.sample_rate;
        pOutFrame->channels = config_.audio.channels;
        pOutFrame->channel_layout =
            av_get_default_channel_layout(config_.audio.channels);

        success = resampler_->init(pFrame->channels, (AVSampleFormat)pFrame->format,
                         pFrame->sample_rate, config_.audio.channels,
                         targetFormat, config_.audio.sample_rate);
        LOG_DEBUG("init resampler {}", success);
        audio_buffer_->clear();
        success = resampler_->resample(pFrame, pOutFrame);
        LOG_DEBUG("resample resampler {}", success);
        if (success) {
          audio_buffer_->fill(pOutFrame->extended_data[0], pOutFrame->linesize[0]);
          av_freep(&pOutFrame->extended_data[0]);
        } else {
          silent = true;
        }
      }
#endif
    }

    LOG_DEBUG("AudioPacketQueueSize: {}", audio_packet_queue_.size());
    LOG_DEBUG("AudioFrameQueueSize: {}", audio_frame_queue_.size());

    // VolumeController::scale(config_.audio.channels, pFrame,
    //                         config_.audio.volume, SDL_MIX_MAXVOLUME);

    len1 = audio_buffer_->remain();
    if (len1 > len) len1 = len;
    silent = false;
    if (!config_.audio.is_muted && !silent &&
        config_.audio.volume * 100 >= SDL_MIX_MAXVOLUME) {
      memcpy(stream, audio_buffer_->peek(), len1);
    } else {
      // silent
      memset(stream, 0, len1);
    }
    if (!config_.audio.is_muted) {
      SDL_MixAudioFormat(stream, audio_buffer_->peek(), AUDIO_S16SYS, len1,
                          config_.audio.volume * 100);
    }

    len -= len1;
    stream += len1;
    audio_buffer_->skip(len1);
  }

  // Sync
  // if (audio_clock_.current() >= 0) {
  // audio_clock_.setTs(
  //     audio_clock_.current() -
  //     (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) /
  //         is->audio_tgt.bytes_per_sec);
  // }
}

SDL_PixelFormatEnum SDLPlayer::convertFFmpegPixelFormatToSDLPixelFormat(
    AVPixelFormat format) {
  switch (format) {
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV420P:
      return SDL_PIXELFORMAT_YV12;
    case AV_PIX_FMT_YUV422P:
      return SDL_PIXELFORMAT_YUY2;
    case AV_PIX_FMT_YUV444P:
      return SDL_PIXELFORMAT_IYUV;
    case AV_PIX_FMT_RGB24:
      return SDL_PIXELFORMAT_RGB24;
    case AV_PIX_FMT_BGR24:
      return SDL_PIXELFORMAT_BGR24;
    case AV_PIX_FMT_RGBA:
      return SDL_PIXELFORMAT_RGBA32;
    case AV_PIX_FMT_BGRA:
      return SDL_PIXELFORMAT_BGRA32;
    case AV_PIX_FMT_ARGB:
      return SDL_PIXELFORMAT_ARGB32;
    case AV_PIX_FMT_ABGR:
      return SDL_PIXELFORMAT_ABGR32;
    default:
      return SDL_PIXELFORMAT_UNKNOWN;
  }
}
int SDLPlayer::convertFFmpegSampleFormatToSDLSampleFormat(
    AVSampleFormat format) {
  switch (format) {
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
      return AUDIO_F32;
    case AV_SAMPLE_FMT_S16:
      return AUDIO_S16;
    case AV_SAMPLE_FMT_S32:
      return AUDIO_S32;
    default:
      return 0;
  }
}

void SDLPlayer::sdlAudioCallback(void *userdata, Uint8 *stream, int len) {
  SDLPlayer *player = static_cast<SDLPlayer *>(userdata);
  player->onSDLAudioPlay(stream, len);
}

std::string SDLPlayer::dump() const {
  if (video_stream_index_ >= 0)
    av_dump_format(format_context_, video_stream_index_, url_.c_str(), 0);
  if (audio_stream_index_ >= 0)
    av_dump_format(format_context_, audio_stream_index_, url_.c_str(), 0);
  std::stringstream ss;
  config_.dump(ss);
  return ss.str();
}

void SDLPlayer::onPauseToggle()
{
  if (isPaused()) {
    replay();
  }
  else {
    pause();
  }
}

void SDLPlayer::videoDelay()
{
  // microseconds
  int64_t delayUS =
    AV_TIME_BASE / config_.video.frame_rate / config_.common.speed;
  if (isAVStreamBoth()) {
    int syncThreshold = FFMAX(0.04f, FFMIN(delayUS, 0.1f)) * AV_TIME_BASE;
    auto diff = video_clock_.current() - audio_clock_.current();
    if (diff < 10 * AV_TIME_BASE) {  // 10 secs
      if (diff <= -syncThreshold) {
        // video is slow
        delayUS = FFMAX(0, delayUS + diff);
      }
      else if (diff >= syncThreshold) {
        auto up = std::ceil(AV_TIME_BASE / (config_.video.frame_rate * config_.common.speed));
        if (delayUS > up)
          // video is too fast
          delayUS = delayUS + diff;
        else
          // video is too slow
          delayUS = 2 * delayUS;
      }
    }
  }
  else {
    int64_t elapseUS = clocker_.elapse();
    delayUS = FFMAX(0, delayUS - elapseUS);
  }

  LOG_DEBUG("Delay: {}us", delayUS);
  std::this_thread::sleep_for(std::chrono::microseconds{delayUS});
}
