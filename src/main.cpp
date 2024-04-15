#include "include/xplayer/SDLPlayer.h"
#include "libavutil/samplefmt.h"

int main()
{
  PlayerConfig config;
  config.video.width = 1280;
  config.video.height = 720;
  config.video.format = AV_PIX_FMT_RGB24;
  config.audio.format = AV_SAMPLE_FMT_FLTP;
  config.audio.channels = 2;
  config.audio.sample_rate = 44100;
  config.audio.is_muted = false;
  config.audio.volume = 1.0f;
  config.play_after_ready = false;
  config.enable_video = true;
  config.enable_audio = false;

  auto player = SDLPlayer::create(config);
  // player->openUrl("/home/youmu/Desktop/media/22⧸7 9thシングル『曇り空の向こうは晴れている』music video-MP4-1080p-Mh9E1iZxoHs.mp4");
  player->openUrl("/home/youmu/Desktop/media/【東方】Bad Apple!! ＰＶ【影絵】-VP9-360p-FtutLA63Cp8.mp4");
  player->play();

  while (true) std::this_thread::sleep_for(std::chrono::milliseconds(100));

  return 0;
}
