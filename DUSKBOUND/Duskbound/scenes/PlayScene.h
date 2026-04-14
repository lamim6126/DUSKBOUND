#ifndef PLAYSCENE_H
#define PLAYSCENE_H

#include "../core/Scene.h"
#include "../core/Engine.h"
#include "../core/Utils.h"
#include "../entities/NPC.h"

class PlayScene : public Scene {
public:
    PlayScene();   // Constructor - scene objects are created lazily
    void update();
    void draw();
    static void reset();  // Clears static world state for a full restart
};

#endif
