#include "MenuScene.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../systems/CutsceneScene.h"

void MenuScene::update() {
  // ENTER or SPACE → play cutscene intro, which then leads to main menu
  if (consumeKey(13) || consumeKey(32)) {
    Engine::changeScene(new CutsceneScene());
  }
}

void MenuScene::draw() {
  drawBMP(0, 0, "assets/images/menu_bg.bmp");

  // Pulsing prompt
  drawTextC(301, 251, "Press ENTER to Start", 0, 0, 0);
  drawTextC(300, 250, "Press ENTER to Start", 255, 255, 0);
}