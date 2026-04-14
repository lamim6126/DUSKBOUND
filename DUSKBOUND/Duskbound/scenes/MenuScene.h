#ifndef MENUSCENE_H
#define MENUSCENE_H

#include "../core/Scene.h"

class MenuScene : public Scene {
public:
	MenuScene() : blink(0) {}
	void update();
	void draw();

private:
	int blink;
};

#endif
