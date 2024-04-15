#pragma once

#include "FFmpegUtil.h"

class VolumeController
{
public:
  // do Nothing
  static void scale(int inChannels, AVFrame *pInframe, float volumeRatio = 1.0f)
  {
    for (int i = 0; i < pInframe->nb_samples; i++) {
      for (int ch = 0; ch < inChannels; ch++) {
        pInframe->data[ch][i] *= volumeRatio;
      }
    }
  }
private:

};
