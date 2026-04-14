#ifndef GAMEOVERSCENE_H
#define GAMEOVERSCENE_H

#include "../core/Scene.h"

class GameOverScene : public Scene {
public:
    enum Mode {
        DEFEAT_SCREEN = 0,
        ENDING_CREDITS = 1
    };

    explicit GameOverScene(Mode mode = DEFEAT_SCREEN);
    void update();
    void draw();

private:
    Mode mode;
    static bool assetsLoaded;
    static int defeatTexture;
    static int endingTexture;
    static void loadAssets();
};

#endif
