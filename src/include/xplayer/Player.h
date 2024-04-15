#pragma once

#include <cstdint>
#include <string>
#include "PlayerConfig.h"
#include "noncopyable.h"

class Player : public noncopyable
{
public:
  enum Status {
    NONE,
    INITED, // after init
    OPENING, // before open
    READY,  // after opened
    PLAYING,
    PAUSED,
    END,
    BROKEN,
  };

  Player() = default;
  virtual ~Player() = default;
  virtual bool init(PlayerConfig config) = 0;
  virtual void destroy() = 0;

  virtual bool openUrl(const std::string &url) = 0;
  virtual bool play() = 0;
  virtual bool replay() = 0;
  virtual bool pause() = 0;
  virtual void close() = 0;

  virtual bool isPlaying() const = 0;
  virtual bool isPaused() const = 0;

  bool isEnableVideo() const { return config_.enable_video; }
  bool isEnableAudio() const { return config_.enable_audio; }

  // video controller
  void setWidth(int width) { config_.video.width = width; }
  int getWidth() const { return config_.video.width; }
  void setHeight(int height) { config_.video.height = height; }
  int getHeight() const { return config_.video.height; }

  // audio controller
  void mute() { config_.audio.is_muted = true; }
  void unmute() { config_.audio.is_muted = false; }
  bool isMuted() const { return config_.audio.is_muted; }
  void setVolume(float volume) { config_.audio.volume = volume; }
  float getVolume() const { return config_.audio.volume; }
  void setSpeed(float speed) { config_.common.speed = speed; }
  float getSpeed() const { return config_.common.speed; }

  // time controller
  // miliseconds
  virtual void seek(int64_t ms_timestamp) = 0;
  virtual int64_t getCurrentPosition() const = 0;
  virtual int64_t getTotalTime() const = 0;

protected:
  PlayerConfig config_{};
  Status status_{NONE};
};
