#ifndef MAINMENUSCENE_H
#define MAINMENUSCENE_H

#include "../core/Scene.h"
#include "../core/SaveSystem.h"
#include <string>
#include <vector>

class MainMenuScene : public Scene {
  int selected;
  int moveCooldown;
  int page;
  int msgTimer;
  std::string msg;

  int optionSelected;
  bool audioOn;

  // Mouse state
  int mouseX, mouseY;
  bool mouseLWasDown; // for edge-detect (click on release)

  // Preloaded button textures [item 0..4][0=normal, 1=hover]
  int btnTex[5][2];

  // Credits group photo texture
  int creditsTexId;

  // Load-save page state
  std::vector<SaveSlotInfo> saveSlots;
  int saveSelected;
  bool renameMode;
  int renameKeyCooldown;
  std::string renameBuffer;

  int getHoveredItem();
  int getHoveredOption();
  void refreshSaveSlots();
  void updateRenameInput();

public:
  explicit MainMenuScene(int startPage = 0);
  void update();
  void draw();
};

#endif
