#include "xplayer/SDLPlayer.h"

#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "xplayer/Log.h"
#include "xplayer/common.h"
#include <cstdint>

std::shared_ptr<SDLPlayer> SDLPlayer::create(PlayerConfig config) {
  auto pSDLPlayer = std::make_shared<SDLPlayer>();
  if (!pSDLPlayer->init(config)) {
    return nullptr;
  }
  return pSDLPlayer;
}

SDLPlayer::SDLPlayer() : Player() {
  // avformat_network_init();
  // avdevice_register_all();
  SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
}

SDLPlayer::~SDLPlayer() {
  this->destroy();
  SDL_Quit();
}

bool SDLPlayer::init(PlayerConfig config) {
  if (!checkConfig()) return false;

  if (config.enable_video) {
    window_ = SDL_CreateWindow("SDL Player", 0, 0, config.video.width,
                               config.video.height, 0);
    if (!window_) {
      LOG_ERROR("[SDLPlayer] Could not create window! SDL_ERROR: {}",
                SDL_GetError());
      return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
      LOG_ERROR("[SDLPlayer] Could not create renderer! SDL_ERROR: {}",
                SDL_GetError());
      return false;
    }
  }

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
    this->destroy();
    LOG_ERROR("[SDLPlayer] Failed to find stream info while opening {}", url);
    return false;
  }

  for (int i = 0; i < format_context_->nb_streams; i++) {
    if (config_.enable_audio && audio_stream_index_ < 0 &&
        AVMEDIA_TYPE_AUDIO ==
            format_context_->streams[i]->codecpar->codec_type) {
      audio_stream_index_ = i;
    } else if (config_.enable_video && video_stream_index_ < 0 &&
               AVMEDIA_TYPE_VIDEO ==
                   format_context_->streams[i]->codecpar->codec_type) {
      video_stream_index_ = i;
    }
  }
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
    this->close();
    LOG_ERROR("[SDLPlayer] No audio or video stream found while opening {}",
              url);
    return false;
  }

  if (enable_audio_) {
    auto pAudioDecoder = avcodec_find_decoder(
        format_context_->streams[audio_stream_index_]->codecpar->codec_id);
    audio_codec_context_ = avcodec_alloc_context3(pAudioDecoder);
    avcodec_parameters_to_context(
        audio_codec_context_,
        format_context_->streams[audio_stream_index_]->codecpar);
    r = avcodec_open2(audio_codec_context_, pAudioDecoder, nullptr);
    if (r < 0) {
      this->destroy();
      LOG_ERROR("[SDLPlayer] Failed to open audio codec while opening {}", url);
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

    audio_device_id_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (audio_device_id_ == 0) {
      LOG_ERROR("[SDLPlayer] Failed to open audio device");
      return false;
    }
    audio_frame_queue_.clear();
    audio_frame_queue_.open();
  }
  if (enable_video_) {
    auto pVideoDecoder = avcodec_find_decoder(
        format_context_->streams[video_stream_index_]->codecpar->codec_id);
    video_codec_context_ = avcodec_alloc_context3(pVideoDecoder);
    avcodec_parameters_to_context(
        video_codec_context_,
        format_context_->streams[video_stream_index_]->codecpar);
    r = avcodec_open2(video_codec_context_, pVideoDecoder, nullptr);
    if (r < 0) {
      this->destroy();
      LOG_ERROR("[SDLPlayer] Failed to open video codec while opening {}", url);
      return false;
    }
    video_frame_queue_.clear();
    video_frame_queue_.open();
  }

  LOG_INFO("[SDLPlayer] Loading video {}, length: {}", url, getTotalTime());
  status_ = Player::READY;
  if (config_.play_after_ready) {
    return play();
  }
  return true;
}
bool SDLPlayer::play() {
  if (enable_audio_) {
    audio_decode_thread_.dispatch([&] {
      int r{-1};
      r = av_seek_frame(format_context_, audio_stream_index_, seek_pos_ * 1000,
                        AVSEEK_FLAG_BACKWARD);
      if (r < 0) {
        LOG_ERROR("[SDLPlayer] Failed to seek to {}ms while playing",
                  seek_pos_);
        return;
      }
      // 保证在重新播放后，没有之前没处理完的帧
      audio_frame_queue_.clear();

      AVPacket *pkt = av_packet_alloc();
      AVFrame *frame = av_frame_alloc();
      while (true) {
        std::unique_lock<std::mutex> locker(audio_mutex_);
        audio_cond_.wait(locker, [&]() {
          return audio_decode_thread_.isRunning() &&
                 audio_frame_queue_.size() <= kMaxAudioFrame;
        });
        if (audio_decode_thread_.isFinished()) return;

        r = av_read_frame(format_context_, pkt);
        if (pkt->stream_index != audio_stream_index_) {
          av_packet_unref(pkt);
          continue;
        }
        if (r == AVERROR_EOF) {
          LOG_INFO("[SDLPlayer] The audio stream is finished");
          audio_decode_thread_.setFinished(true);
          break;
        } else if (r < 0) {
          LOG_ERROR("[SDLPlayer] Failed to read frame while playing");
          break;
        }
        r = avcodec_send_packet(audio_codec_context_, pkt);
        if (r < 0) {
          LOG_ERROR("[SDLPlayer] Error sending a packet for decoding");
          break;
        }

        while (true) {
          r = avcodec_receive_frame(audio_codec_context_, frame);
          if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
            break;
          else if (r < 0) {
            LOG_ERROR("[SDLPlayer] Audio frame is broken while playing");
            break;
          }

          if (frame->sample_rate == 0) {
            LOG_WARN("[SDLPlayer] Audio frame sample rate is 0");
            continue;
          }

          // we get: frame
          // the things can be done in the zone: process the audio frame
          AVFrame *outFrame = av_frame_alloc();
          resampler_ = Resampler::create(
              frame->channel_layout, (AVSampleFormat)frame->format,
              frame->sample_rate,
              av_get_default_channel_layout(config_.audio.channels),
              config_.audio.format == AV_SAMPLE_FMT_NONE
                  ? (AVSampleFormat)frame->format
                  : config_.audio.format,
              config_.audio.sample_rate);
          bool success = resampler_->resample(frame, outFrame);
          if (!success) {
            av_frame_unref(outFrame);
            outFrame = av_frame_clone(frame);
          }
          audio_frame_queue_.push(outFrame);
          audio_cond_.notify_one();

          av_frame_unref(frame);
        }
      }
      av_packet_free(&pkt);
      av_frame_free(&frame);
    });
  }
  if (enable_video_) {
    video_decode_thread_.dispatch([&] {
      int r{-1};
      r = av_seek_frame(format_context_, audio_stream_index_, seek_pos_ * 1000,
                        AVSEEK_FLAG_BACKWARD);
      if (r < 0) {
        LOG_ERROR("[SDLPlayer] Failed to seek to {}ms while playing",
                  seek_pos_);
        return;
      }
      // 保证在重新播放后，没有之前没处理完的帧
      video_frame_queue_.clear();

      AVPacket *pkt = av_packet_alloc();
      AVFrame *frame = av_frame_alloc();
      while (true) {
        std::unique_lock<std::mutex> locker(video_mutex_);
        video_cond_.wait(locker, [&]() {
          return video_decode_thread_.isRunning() &&
                 video_frame_queue_.size() <= kMaxVideoFrame;
        });
        if (video_decode_thread_.isFinished()) return;

        r = av_read_frame(format_context_, pkt);
        if (pkt->stream_index != video_stream_index_) {
          av_packet_unref(pkt);
          continue;
        }
        if (r == AVERROR_EOF) {
          LOG_INFO("[SDLPlayer] The video stream is finished");
          video_decode_thread_.setFinished(true);
          break;
        } else if (r < 0) {
          LOG_ERROR("[SDLPlayer] Failed to read frame while playing");
          break;
        }
        r = avcodec_send_packet(video_codec_context_, pkt);
        if (r < 0) {
          LOG_ERROR("[SDLPlayer] Error sending a packet for decoding");
          break;
        }

        while (true) {
          r = avcodec_receive_frame(video_codec_context_, frame);
          if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
            break;
          else if (r < 0) {
            LOG_ERROR("[SDLPlayer] Video frame is broken while playing");
            break;
          }

          // we get: frame
          // the things can be done in the zone: process the video frame
          AVFrame *outFrame = av_frame_alloc();
          converter_ = Converter::create(
              frame->width, frame->height, (AVPixelFormat)frame->format,
              config_.video.width, config_.video.height,
              config_.video.format == AV_PIX_FMT_NONE
                  ? (AVPixelFormat)frame->format
                  : config_.video.format);
          av_image_alloc(outFrame->data, outFrame->linesize, config_.video.width, config_.video.height,
              config_.video.format == AV_PIX_FMT_NONE
                  ? (AVPixelFormat)frame->format
                  : config_.video.format, 1);
          bool success = converter_->convert(frame, outFrame);
          if (!success || true) {
            av_frame_free(&outFrame);
            outFrame = av_frame_clone(frame);
          }

          video_frame_queue_.push(outFrame);
          video_cond_.notify_one();
          av_frame_unref(frame);
        }
      }
      av_frame_free(&frame);
      av_packet_free(&pkt);
    });
  }
  status_ = Player::PLAYING;
  return replay();
}
bool SDLPlayer::replay() {
  while (status_ == Player::PLAYING) {
    // Control Video Play
    if (enable_video_) {
      video_decode_thread_.open();
      video_cond_.notify_one();
      // TODO:
      this->onSDLVideoPlay();
    }
    // Control Audio Play
    if (enable_audio_) {
      audio_decode_thread_.open();
      audio_cond_.notify_one();
      SDL_LockAudioDevice(audio_device_id_);
      SDL_PauseAudioDevice(audio_device_id_, 0);
      SDL_UnlockAudioDevice(audio_device_id_);
    }
  }
  return true;
}
bool SDLPlayer::pause() {
  seek(getCurrentPosition());
  // TODO:
  if (audio_device_id_ > 0) {
    SDL_LockAudioDevice(audio_device_id_);
    SDL_PauseAudioDevice(audio_device_id_, 1);
    SDL_UnlockAudioDevice(audio_device_id_);
  }

  video_decode_thread_.close();
  audio_decode_thread_.close();
  status_ = Player::PAUSED;
  return true;
}
void SDLPlayer::close() {
  if (audio_device_id_ > 0) {
    SDL_LockAudioDevice(audio_device_id_);
    SDL_PauseAudioDevice(audio_device_id_, 1);
    SDL_UnlockAudioDevice(audio_device_id_);
    SDL_CloseAudioDevice(audio_device_id_);
  }

  audio_decode_thread_.setFinished(true);
  video_decode_thread_.setFinished(true);
  audio_decode_thread_.close();
  video_decode_thread_.close();
  audio_frame_queue_.clear();
  video_frame_queue_.clear();
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
  seek_pos_ = clock_pos_ = audio_clock_pos_ = video_clock_pos_ = 0;

  status_ = Player::END;
}

void SDLPlayer::seek(int64_t position) { seek_pos_ = position; }
int64_t SDLPlayer::getCurrentPosition() const {
  return enable_audio_ ? audio_clock_pos_ : video_clock_pos_;
}
int64_t SDLPlayer::getTotalTime() const {
  return format_context_->duration / 1000;
}

bool SDLPlayer::checkConfig() {
  bool isNoProblem = true;
  if (config_.enable_video) {
    // check video configurations
    isNoProblem &= expect(config_.video.frame_rate > 0,
                          "Frame Rate must be greater than 0 on Video") &&
                   expect(config_.video.width > 0,
                          "Width must be greater than 0 on Video") &&
                   expect(config_.video.height > 0,
                          "Height must be greater than 0 on Video");
  }
  if (config_.enable_audio) {
    // check audio configurations
    isNoProblem &= expect(config_.audio.sample_rate > 0,
                          "Sample rate must be greater than 0 on Audio") &&
                   expect(config_.audio.volume > 0.0f,
                          "Volume must be greater than 0 on Audio");
  }

  return isNoProblem;
}

bool SDLPlayer::expect(bool condition, const std::string &error) {
  if (!condition) {
    error_ = error;
  }
  return condition;
}

void SDLPlayer::onSDLVideoPlay() {
  while (true) {
    AVFrame *pFrame;
    if (!video_frame_queue_.pop(pFrame)) {
      continue;
    }

    SDL_PixelFormatEnum format =
        convertFFmpegPixelFormatToSDLPixelFormat((AVPixelFormat)pFrame->format);
    SDL_Texture *pTexture =
        SDL_CreateTexture(renderer_, format, SDL_TEXTUREACCESS_STREAMING,
                          config_.video.width, config_.video.height);

    if (pTexture == nullptr) {
      LOG_ERROR("[SDLPlayer] Failed to create texture while playing");
      return;
    }

    SDL_UpdateTexture(pTexture, nullptr, pFrame->data[0], pFrame->linesize[0]);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, pTexture, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    SDL_DestroyTexture(pTexture);
    av_frame_free(&pFrame);
  }
}
void SDLPlayer::onSDLAudioPlay(Uint8 *stream, int len) {
  // BUG: segment fail when calling pop
  // AVFrame *pFrame;
  // LOG_DEBUG("{}", audio_frame_queue_.isOpening());
  // if (audio_frame_queue_.pop(pFrame)) {
  //   LOG_DEBUG("Audio frame size: {}", pFrame->pkt_size);
  // }
  // TODO: Complete me!
  LOG_DEBUG("{}#{}", (intptr_t)stream, len);
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
  return "";
}

