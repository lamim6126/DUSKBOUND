#ifndef CUTSCENESCENE_H
#define CUTSCENESCENE_H

#include "../core/Scene.h"
#include <string>

struct CutsceneSlide {
  const char *title;
  const char *line1;
  const char *line2;
  const char *line3;
  int bgR, bgG, bgB; // background tint
  int duration;      // frames this slide lasts (at ~60fps)
};

class CutsceneScene : public Scene {
  int slideIndex;
  int timer;
  int fadeTimer; // 0..30 fade in, slides play, last 30 frames fade out
  bool fadingOut;

  static const int FADE_FRAMES = 30;

public:
  CutsceneScene();
  void update();
  void draw();
};

#endif
