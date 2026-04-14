#ifndef BOSSCUTSCENE1_H
#define BOSSCUTSCENE1_H

#include "../core/Scene.h"

class BossCutscene1 : public Scene {
public:
    enum Mode {
        PRE_BOSS = 0,
        POST_BOSS = 1
    };

private:
    int slideIndex;
    int timer;
    int fadeTimer;
    Mode mode;
    static const int FADE_FRAMES = 30;
public:
    explicit BossCutscene1(Mode mode = PRE_BOSS);
    void update();
    void draw();
};

#endif
