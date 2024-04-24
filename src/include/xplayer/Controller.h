#pragma once

#include "FFmpegUtil.h"

class VolumeController {
 public:
  static void scale(int inChannels, AVFramePtr pInframe,
                    float volumeRatio, int maxVolume) {
    for (int ch = 0; ch < inChannels; ch++) {
      for (int i = 0; i < pInframe->nb_samples; i++) {
        int newVolume = pInframe->data[ch][i] * volumeRatio;
        if (newVolume > maxVolume) newVolume = maxVolume;
        pInframe->data[ch][i] = newVolume;
      }
    }
  }
};
