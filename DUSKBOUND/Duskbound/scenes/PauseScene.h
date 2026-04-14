#ifndef PAUSESCENE_H
#define PAUSESCENE_H

#include "../core/Scene.h"

class PauseScene : public Scene {
    int selected;
    int moveCooldown;
    int page;          // 0=main, 1=options, 2=credits, 3=controls
    int optionSelected;
    bool audioOn;
    int msgTimer;      // for credits animation

    // Mouse state
    int mouseX, mouseY;
    bool mouseLWasDown;

    // Preloaded button textures [item 0..5][0=normal, 1=hover]
    int btnTex[6][2];

    // Credits group photo texture
    int creditsTexId;

    int getHoveredItem();
    int getHoveredOption();

public:
    PauseScene();
    void update();
    void draw();
};

#endif
