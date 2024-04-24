#include "xplayer/FFmpegUtil.h"
#include "xplayer/SDLPlayer.h"
#include "xplayer/Log.h"

#include <csignal>

void sig_handler(int sig) {
  exit(0);
}

// FIXME: Too fast for 1.0 speed
int main()
{
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGPIPE, sig_handler);
  signal(SIGHUP, sig_handler);
  signal(SIGSEGV, sig_handler);

  PlayerConfig config;
  config.video.width = -1;
  config.video.height = -1;
  config.video.format = AV_PIX_FMT_YUV420P;
  // config.video.format = AV_PIX_FMT_RGB24;
  config.video.frame_rate = -1.0f;
  config.audio.format = AV_SAMPLE_FMT_FLTP;
  config.audio.channels = 2;
  config.audio.sample_rate = 44100;
  config.audio.is_muted = false;
  config.audio.volume = 1.0f;
  config.common.speed = 1.0f;
  config.play_after_ready = false;
  config.enable_video = true;
  config.enable_audio = false;

  std::list<std::string> mediaPlaylist = {
    // "/home/youmu/Desktop/media/bad_apple.mp4",
    // "/home/youmu/Desktop/media/22⧸7 9thシングル『曇り空の向こうは晴れている』music video-MP4-1080p-Mh9E1iZxoHs.mp4",
    "/home/youmu/Desktop/media/bad_apple_clip1.mp4",
    "/home/youmu/Desktop/media/227_9th_clip0.mp4",
    // "/home/youmu/Desktop/media/【東方】Bad Apple!! ＰＶ【影絵】-VP9-360p-FtutLA63Cp8.mp4",
  };

  auto player = SDLPlayer::create(config);
  while (true) {
    if (mediaPlaylist.empty()) {
      LOG_INFO("Waiting for next media...");
      while (mediaPlaylist.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
      }
    }

    auto url = mediaPlaylist.front();
    mediaPlaylist.pop_front();

    player->init(config);
    player->openUrl(url);
    // player->seek(100000);
    player->play();
    while (player->isPlaying()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }

  LOG_INFO("This media playlist is over");

  return 0;
}
