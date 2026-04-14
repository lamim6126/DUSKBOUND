#include "MainMenuScene.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/Sound.h"
#include "PlayScene.h"
#include <windows.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

// Global mouse state (defined in iMain.cpp)
extern int g_mouseX;
extern int g_mouseY;
extern bool g_mouseLDown;

// Menu items
static const int ITEM_COUNT = 5;

// Button layout
static const int BTN_W = 200;
static const int BTN_H = 50;
static const int BTN_START_Y = 281;  // y of bottom-left of first (top) button
static const int BTN_GAP = 58;       // spacing between buttons
static const int BTN_CENTER_X = 410; // horizontal centre of buttons

// Button image paths (PNG): [item][0]=normal, [item][1]=hover
static const char *BTN_IMG[5][2] = {
    {"assets/buttons/btn_start_nm.png",
     "assets/buttons/btn_start_hv.png"}, // Start
    {"assets/buttons/btn_load_nm.png",
     "assets/buttons/btn_load_hv.png"}, // Load Save
    {"assets/buttons/btn_option_nm.png",
     "assets/buttons/btn_option_hv.png"}, // Options
    {"assets/buttons/btn_credit_nm.png",
     "assets/buttons/btn_credit_hv.png"}, // Credits
    {"assets/buttons/btn_exit_nm.png",
     "assets/buttons/btn_exit_hv.png"}, // Exit
};

static const char *MENU_MUSIC = "assets/audio/menu.wav";
static bool gMenuMusicPlaying = false;

// Helpers
// Returns which main-menu item (0-4) is under the mouse, or -1
int MainMenuScene::getHoveredItem() {
  for (int i = 0; i < ITEM_COUNT; i++) {
    int bx = BTN_CENTER_X - BTN_W / 2;
    int by = BTN_START_Y - i * BTN_GAP; // bottom-left y of button
    if (g_mouseX >= bx && g_mouseX <= bx + BTN_W && g_mouseY >= by &&
        g_mouseY <= by + BTN_H)
      return i;
  }
  return -1;
}

// Returns which options-page item (0-2) is under the mouse, or -1
int MainMenuScene::getHoveredOption() {
  static const int OPT_COUNT = 3;
  // Centered at x=400, items spaced 70px apart starting at y=260
  int cx = 400, y = 260, gap = 70;
  for (int i = 0; i < OPT_COUNT; i++) {
    int cy = y - i * gap;
    int left  = cx - 160;
    int right = cx + 160;
    int bot   = cy - 22;
    int top   = cy + 22;
    if (g_mouseX >= left && g_mouseX <= right && g_mouseY >= bot &&
        g_mouseY <= top)
      return i;
  }
  return -1;
}

static void UpdateMenuMusic(bool audioOn) {
  if (audioOn) {
    if (!gMenuMusicPlaying) {
      playSound(MENU_MUSIC, true);
      gMenuMusicPlaying = true;
    }
  } else {
    if (gMenuMusicPlaying) {
      stopSound();
      gMenuMusicPlaying = false;
    }
  }
}

// Refresh the visible save-slot list for the load page.
void MainMenuScene::refreshSaveSlots() {
  listSaveSlots(saveSlots);
  if (saveSlots.empty()) {
    saveSelected = 0;
  } else {
    if (saveSelected < 0)
      saveSelected = 0;
    if (saveSelected >= (int)saveSlots.size())
      saveSelected = (int)saveSlots.size() - 1;
  }
}

// updateRenameInput
// Lightweight keyboard text entry for the load-save page.
// Letters, digits, space, hyphen and underscore are accepted.
void MainMenuScene::updateRenameInput() {
  if (renameKeyCooldown > 0)
    renameKeyCooldown--;

  if (consumeKey(8)) {
    if (!renameBuffer.empty())
      renameBuffer.erase(renameBuffer.size() - 1, 1);
    renameKeyCooldown = 2;
  }

  if (renameKeyCooldown > 0)
    return;

  for (char c = 'A'; c <= 'Z'; ++c) {
    if (consumeKey(c) || consumeKey(c + 32)) {
      if (renameBuffer.size() < 30)
        renameBuffer.push_back(c);
      renameKeyCooldown = 2;
      return;
    }
  }

  for (char c = '0'; c <= '9'; ++c) {
    if (consumeKey(c)) {
      if (renameBuffer.size() < 30)
        renameBuffer.push_back(c);
      renameKeyCooldown = 2;
      return;
    }
  }

  if (consumeKey(' ')) {
    if (renameBuffer.size() < 30)
      renameBuffer.push_back(' ');
    renameKeyCooldown = 2;
    return;
  }
  if (consumeKey('-')) {
    if (renameBuffer.size() < 30)
      renameBuffer.push_back('-');
    renameKeyCooldown = 2;
    return;
  }
  if (consumeKey('_')) {
    if (renameBuffer.size() < 30)
      renameBuffer.push_back('_');
    renameKeyCooldown = 2;
    return;
  }
}

// Constructor
MainMenuScene::MainMenuScene(int startPage) {
  selected = 0;
  page = startPage;
  msg = "";
  msgTimer = 0;
  moveCooldown = 0;
  optionSelected = 0;
  audioOn = true;
  mouseX = 0;
  mouseY = 0;
  mouseLWasDown = false;
  saveSelected = 0;
  renameMode = false;
  renameKeyCooldown = 0;
  renameBuffer = "";

  // Preload button PNG textures once
  for (int i = 0; i < 5; i++) {
    btnTex[i][0] = loadImage(BTN_IMG[i][0]);
    btnTex[i][1] = loadImage(BTN_IMG[i][1]);
  }
  creditsTexId = loadImage("assets/credits/credis.png");

  if (page == 4) {
    refreshSaveSlots();
    msg = saveSlots.empty() ? "No autosaves found" : "Choose a save to continue";
  }
}

// Update
void MainMenuScene::update() {

  UpdateMenuMusic(audioOn);

  // Snapshot mouse for this frame
  mouseX = g_mouseX;
  mouseY = g_mouseY;
  bool lDown = g_mouseLDown;
  bool clicked = (!lDown && mouseLWasDown); // rising edge = click
  mouseLWasDown = lDown;

  // ---- OPTIONS PAGE ----
  if (page == 1) {
    if (moveCooldown > 0)
      moveCooldown--;

    // Mouse hover
    int hov = getHoveredOption();
    if (hov >= 0)
      optionSelected = hov;

    // Keyboard nav
    if (moveCooldown == 0) {
      if (keys['w'] || keys['W']) {
        optionSelected--;
        if (optionSelected < 0)
          optionSelected = 2;
        moveCooldown = 10;
      } else if (keys['s'] || keys['S']) {
        optionSelected++;
        if (optionSelected > 2)
          optionSelected = 0;
        moveCooldown = 10;
      }
    }

    // Confirm: keyboard ENTER/SPACE or mouse click
    bool confirm = (moveCooldown == 0 && (consumeKey(13) || consumeKey(32))) ||
                   (clicked && getHoveredOption() == optionSelected);
    if (confirm) {
      if (optionSelected == 0) {
        page = 3;
        moveCooldown = 10;
        return;
      }
      if (optionSelected == 1) {
        audioOn = !audioOn;
        stopSound();
        gMenuMusicPlaying = false;
        if (audioOn) {
          playSound(MENU_MUSIC, true);
          gMenuMusicPlaying = true;
        }
        moveCooldown = 10;
        return;
      }
      if (optionSelected == 2) {
        page = 0;
        moveCooldown = 10;
        return;
      }
    }

    if (consumeKey('b') || consumeKey('B')) {
      page = 0;
      moveCooldown = 10;
      return;
    }
    return;
  }

  // ---- CREDITS PAGE ----
  if (page == 2) {
    if (moveCooldown > 0)
      moveCooldown--;
    msgTimer++;
    if (moveCooldown == 0 && (consumeKey('b') || consumeKey('B'))) {
      page = 0;
      msgTimer = 0;
      moveCooldown = 10;
    }
    return;
  }

  // ---- CONTROLS PAGE ----
  if (page == 3) {
    // B returns to the options page first, matching the on-screen hint
    // and the pause-menu control flow.
    if (consumeKey('b') || consumeKey('B')) {
      page = 1;
      moveCooldown = 10;
      return;
    }

    // Escape still leaves the controls page immediately.
    if (consumeKey(27)) {
      page = 0;
      moveCooldown = 10;
      return;
    }
    return;
  }

  // ---- LOAD SAVE PAGE ----
  if (page == 4) {
    if (moveCooldown > 0)
      moveCooldown--;

    // Rename mode uses the same page but temporarily captures text input.
    if (renameMode) {
      updateRenameInput();

      if (consumeKey('\r') || consumeKey('\n')) {
        if (!saveSlots.empty() && !renameBuffer.empty()) {
          if (renameSaveDisplayName(saveSlots[saveSelected].filePath,
                                    renameBuffer.c_str())) {
            msg = "Save renamed successfully";
            refreshSaveSlots();
          } else {
            msg = "Failed to rename save";
          }
        }
        renameMode = false;
        return;
      }

      if (consumeKey(27) || consumeKey('b') || consumeKey('B')) {
        renameMode = false;
        msg = "Rename cancelled";
        return;
      }

      return;
    }

    // Mouse hover: highlight whichever slot the cursor is over.
    if (!saveSlots.empty()) {
      for (int i = 0; i < (int)saveSlots.size() && i < 6; i++) {
        int boxY = 345 - i * 50;
        if (g_mouseX >= 80 && g_mouseX <= 720 &&
            g_mouseY >= boxY - 10 && g_mouseY <= boxY + 20) {
          saveSelected = i;
          break;
        }
      }
    }

    // Keyboard navigation (W/S).
    if (moveCooldown == 0 && !saveSlots.empty()) {
      if (keys['w'] || keys['W']) {
        saveSelected--;
        if (saveSelected < 0)
          saveSelected = (int)saveSlots.size() - 1;
        moveCooldown = 10;
      } else if (keys['s'] || keys['S']) {
        saveSelected++;
        if (saveSelected >= (int)saveSlots.size())
          saveSelected = 0;
        moveCooldown = 10;
      }
    }

    if (consumeKey('b') || consumeKey('B')) {
      page = 0;
      return;
    }

    // Start renaming the highlighted slot.
    if ((consumeKey('r') || consumeKey('R')) && !saveSlots.empty()) {
      renameMode = true;
      renameBuffer = saveSlots[saveSelected].displayName;
      msg = "Type a new name, then press ENTER";
      return;
    }

    // Delete the highlighted slot immediately.
    if ((consumeKey('x') || consumeKey('X')) && !saveSlots.empty()) {
      if (deleteSaveSlot(saveSlots[saveSelected].filePath)) {
        msg = "Save deleted";
        refreshSaveSlots();
      } else {
        msg = "Failed to delete save";
      }
      return;
    }

    // Load via keyboard (ENTER / SPACE).
    if ((consumeKey(13) || consumeKey(32)) && !saveSlots.empty()) {
      if (loadSaveSlot(saveSlots[saveSelected].filePath)) {
        stopSound();
        gMenuMusicPlaying = false;
        Engine::changeScene(new PlayScene());
        return;
      }
      msg = "Failed to load selected save";
    }

    // Load via mouse click on a slot.
    if (clicked && !saveSlots.empty()) {
      for (int i = 0; i < (int)saveSlots.size() && i < 6; i++) {
        int boxY = 345 - i * 50;
        if (g_mouseX >= 80 && g_mouseX <= 720 &&
            g_mouseY >= boxY - 10 && g_mouseY <= boxY + 20) {
          saveSelected = i;
          if (loadSaveSlot(saveSlots[i].filePath)) {
            stopSound();
            gMenuMusicPlaying = false;
            Engine::changeScene(new PlayScene());
            return;
          }
          msg = "Failed to load selected save";
          break;
        }
      }
    }

    return;
  }


  // ---- MAIN MENU ----
  if (moveCooldown > 0)
    moveCooldown--;

  // Mouse hover updates selection
  int hov = getHoveredItem();
  if (hov >= 0)
    selected = hov;

  // Keyboard navigation
  if (moveCooldown == 0) {
    if (keys['w'] || keys['W']) {
      selected--;
      if (selected < 0)
        selected = ITEM_COUNT - 1;
      moveCooldown = 10;
    } else if (keys['s'] || keys['S']) {
      selected++;
      if (selected >= ITEM_COUNT)
        selected = 0;
      moveCooldown = 10;
    }
  }

  // Confirm: keyboard or mouse click on hovered item
  bool confirm = (consumeKey(13) || consumeKey(32)) ||
                 (clicked && getHoveredItem() == selected && hov >= 0);
  if (confirm) {
    switch (selected) {
    case 0:
      // Start always creates a fresh save slot.
      beginNewSaveSlot();
      stopSound();
      gMenuMusicPlaying = false;
      Engine::changeScene(new PlayScene());
      break;
    case 1:
      refreshSaveSlots();
      page = 4;
      msg = saveSlots.empty() ? "No autosaves found" : "Choose a save to continue";
      break;
    case 2:
      page = 1;
      moveCooldown = 10;
      break;
    case 3:
      page = 2;
      msgTimer = 0;
      moveCooldown = 10;
      break;
    case 4:
      stopSound();
      gMenuMusicPlaying = false;
      exit(0);
      break;
    }
  }
}

// Draw
void MainMenuScene::draw() {
  drawBMP(0, 0, "assets/images/menu_bg.bmp");

  // ---- OPTIONS PAGE ----
  if (page == 1) {
    // Centered "OPTIONS" title at top
    const char* title = "OPTIONS";
    // Times Roman 24 avg ~13px/char; "OPTIONS" = 7 chars => ~91px wide
    int titleX = 400 - (int)(strlen(title) * 13) / 2;
    drawTextBig(titleX + 1, 391, title, 0, 0, 0);
    drawTextBig(titleX,     390, title, 255, 220, 100);

    // Decorative divider under title
    setColor(180, 150, 60);
    fillRect(200, 375, 400, 2);

    const char *options[3] = {"Controls", audioOn ? "Audio: ON" : "Audio: OFF",
                              "Back"};

    // Three menu items centered at x=400, spaced 70px apart from y=260
    int cx = 400, y = 260, gap = 70;
    for (int i = 0; i < 3; i++) {
      int yy = y - i * gap;
      bool hov = (i == optionSelected);
      int len   = (int)strlen(options[i]);
      int textW = len * 13;             // ~13px per char in Times Roman 24
      int textX = cx - textW / 2;

      // Highlight / background box (wider, centered)
      if (hov) {
        setColor(100, 75, 15);
        fillRect(cx - 162, yy - 20, 324, 46);
        setColor(70, 50, 5);
        fillRect(cx - 160, yy - 18, 320, 42);
      } else {
        setColor(15, 12, 30);
        fillRect(cx - 160, yy - 18, 320, 42);
      }

      // Arrow indicator for selected item
      if (hov) {
        int arrX = cx - 155;
        drawTextBig(arrX + 1, yy - 8, ">", 0, 0, 0);
        drawTextBig(arrX,     yy - 8, ">", 255, 220, 0);
        drawTextBig(textX + 1, yy - 8, options[i], 0, 0, 0);
        drawTextBig(textX,     yy - 8, options[i], 255, 220, 0);
      } else {
        drawTextBig(textX + 1, yy - 8, options[i], 0, 0, 0);
        drawTextBig(textX,     yy - 8, options[i], 200, 200, 200);
      }
    }

    // Decorative divider above hint
    setColor(180, 150, 60);
    fillRect(200, 78, 400, 2);

    drawTextC(221, 61, "W/S = move   ENTER = select   B = back", 0, 0, 0);
    drawTextC(220, 60, "W/S = move   ENTER = select   B = back", 200, 200, 200);
    return;
  }

  // ---- CREDITS PAGE ----
  if (page == 2) {
	  float time = msgTimer * 0.05f;

	  // ---- Scroll background (parchment, same style as quest scroll) ----
	  const int scrollX = 140;
	  const int scrollY = 40;
	  const int scrollW = 520;
	  const int scrollH = 370;

	  // Outer border (dark brown)
	  setColor(96, 72, 34);
	  fillRect(scrollX - 4, scrollY - 4, scrollW + 8, scrollH + 8);

	  // Parchment face
	  setColor(218, 196, 148);
	  fillRect(scrollX, scrollY, scrollW, scrollH);

	  // Inner border accent
	  setColor(140, 108, 52);
	  drawRectOutline(scrollX, scrollY, scrollW, scrollH);

	  // Thin inner rule 8px inside the border
	  setColor(160, 128, 60);
	  drawRectOutline(scrollX + 8, scrollY + 8, scrollW - 16, scrollH - 16);

	  // Divider under title
	  setColor(140, 108, 52);
	  fillRect(scrollX + 20, scrollY + scrollH - 30, scrollW - 40, 1);

	  // ---- Section header ----
	  const char* secHeader = "Developed by";
	  int shX = 400 - (int)(strlen(secHeader) * 8) / 2;
	  drawTextC(shX + 1, scrollY + scrollH - 56, secHeader, 80, 50, 15);
	  drawTextC(shX, scrollY + scrollH - 57, secHeader, 80, 50, 15);

	  // ---- Divider above first entry ----
	  setColor(160, 130, 70);
	  fillRect(scrollX + 40, scrollY + scrollH - 72, scrollW - 80, 1);

	  // ---- Entry helper layout ----
	  // Three entries, evenly spaced.
	  // Each entry: Name (gold) + ID (muted dark brown) + role (dark)
	  struct CreditEntry {
		  const char* name;
		  const char* id;
		  const char* role;
	  };

	  static const CreditEntry entries[3] = {
		  { "RESHAD AHMED LAMIM", "ID: 00724205101076", "Main Menu, Menu, Cutscenes, Pause Scene, Sound" },
		  { "MUNTASIR RABBI SHOSCHA", "ID: 00724205101079", "World Design, Map Implementation, NPCs, Powerups" },
		  { "MD. RAIYAN HUSSAIN CHOUDHURY",
		  "ID: 00724205101067", "Boss and Character Design, Boss and Character Implementation, Save System, World Collision" },
	  };

	  // Row Y positions (from bottom of scroll, descending)
	  const int rowY[3] = {
		  scrollY + scrollH - 108,
		  scrollY + scrollH - 186,
		  scrollY + scrollH - 264,
	  };

	  for (int i = 0; i < 3; i++) {
		  // Entry background stripe (subtle warm tint)
		  setColor(200, 176, 120);
		  fillRect(scrollX + 20, rowY[i] - 44, scrollW - 40, 50);
		  setColor(160, 128, 60);
		  drawRectOutline(scrollX + 20, rowY[i] - 44, scrollW - 40, 50);

		  // Number badge (left side)
		  setColor(96, 72, 34);
		  fillRect(scrollX + 28, rowY[i] - 36, 22, 22);
		  char badge[4];
		  sprintf_s(badge, sizeof(badge), "%d", i + 1);
		  drawTextC(scrollX + 32, rowY[i] - 22, badge, 218, 196, 148);

		  // Name (dark brown, bold via drawTextBig)
		  int nameX = scrollX + 60;
		  drawTextBig(nameX + 1, rowY[i] - 18, entries[i].name, 60, 30, 5);
		  drawTextBig(nameX, rowY[i] - 19, entries[i].name, 60, 30, 5);

		  // ID (muted)
		  drawTextC(nameX + 1, rowY[i] - 37, entries[i].id, 80, 60, 30);
		  drawTextC(nameX, rowY[i] - 38, entries[i].id, 80, 60, 30);
	  }

	  // ---- Decorative bottom rule ----
	  setColor(140, 108, 52);
	  fillRect(scrollX + 20, scrollY + 22, scrollW - 40, 1);

	  // ---- Course / project line ----
	  const char* proj = "CSE 1200  - Software Development-I Lab -  AUST";
	  int projX = 400 - (int)(strlen(proj) * 8) / 2;
	  drawTextC(projX + 1, scrollY + 11, proj, 80, 55, 20);
	  drawTextC(projX, scrollY + 12, proj, 80, 55, 20);

	  // ---- Pulsing "Press B to go back" hint (below scroll) ----
	  int pulse_back = (int)(200 + 55 * sin(time * 3));
	  const char* hint = "Press B to go back";
	  int hintX = 400 - (int)(strlen(hint) * 8) / 2;
	  drawTextC(hintX + 1, 21, hint, 0, 0, 0);
	  drawTextC(hintX, 20, hint, pulse_back, pulse_back, 0);

	  return;
  }

  // ---- CONTROLS PAGE ----
  if (page == 3) {
    // Title (centered, Times Roman 24pt gold)
    const char* ctitle = "CONTROLS";
    int ctX = 400 - (int)(strlen(ctitle) * 13) / 2;
    drawTextBig(ctX + 1, 421, ctitle, 0, 0, 0);
    drawTextBig(ctX,     420, ctitle, 255, 220, 100);

    // Gold divider under title
    setColor(180, 150, 60);
    fillRect(80, 405, 640, 2);

    // Control entries: key | description | card-center-X | card-center-Y
    // Layout: 2 per row at x=200 (left) and x=600 (right), 3 rows + 1 centered
    const char* ckeys[]  = { "W",   "A",        "S",       "D",         "I",            "ESC",       "B"    };
    const char* cdescs[] = { "Move Up","Move Left","Move Down","Move Right","Quest Scroll","Pause Game","Back" };
    int ccxs[] = { 200, 600, 200, 600, 200, 600, 400 };
    int ccys[] = { 330, 330, 248, 248, 166, 166,  84 };
    int ccount = 7;

    for (int i = 0; i < ccount; i++) {
      int cx = ccxs[i];
      int cy = ccys[i];
      const char* key  = ckeys[i];
      const char* desc = cdescs[i];

      // Outer gold border frame
      setColor(130, 100, 20);
      fillRect(cx - 141, cy - 27, 282, 56);
      // Dark card background
      setColor(14, 11, 28);
      fillRect(cx - 140, cy - 26, 280, 54);
      // Amber key box background
      setColor(55, 40, 8);
      fillRect(cx - 133, cy - 20, 56, 42);
      // Key box inner gold border accent (top + left highlight)
      setColor(160, 120, 30);
      fillRect(cx - 133, cy - 20, 56, 2);   // top edge
      fillRect(cx - 133, cy - 20, 2, 42);   // left edge

      // Key text – gold bold, centered inside key box (56px wide)
      int klen  = (int)strlen(key);
      int kw    = klen * 13;
      int kx    = cx - 133 + (56 - kw) / 2;
      drawTextBig(kx + 1, cy - 13, key, 0, 0, 0);
      drawTextBig(kx,     cy - 13, key, 255, 205, 40);

      // Description text, starts 10px right of key box
      int dx = cx - 133 + 56 + 10;           // cx - 67
      drawTextBig(dx + 1, cy - 13, desc, 0, 0, 0);
      drawTextBig(dx,     cy - 13, desc, 185, 185, 185);
    }

    // Bottom gold divider
    setColor(180, 150, 60);
    fillRect(80, 54, 640, 2);

    drawTextC(261, 31, "B = back to options   ESC = main menu", 0, 0, 0);
    drawTextC(260, 30, "B = back to options   ESC = main menu", 200, 200, 200);

    return;
  }


  // ---- LOAD SAVE PAGE ----
  if (page == 4) {
    drawTextC(321, 461, "LOAD SAVE", 0, 0, 0);
    drawTextC(320, 460, "LOAD SAVE", 255, 255, 255);

    if (renameMode) {
      drawTextC(81, 421, "Type name   ENTER = confirm   BACKSPACE = erase   B/ESC = cancel", 0, 0, 0);
      drawTextC(80, 420, "Type name   ENTER = confirm   BACKSPACE = erase   B/ESC = cancel", 240, 240, 200);
    } else {
      drawTextC(81, 421, "W/S = choose   ENTER = load   R = rename   X = delete   B = back", 0, 0, 0);
      drawTextC(80, 420, "W/S = choose   ENTER = load   R = rename   X = delete   B = back", 240, 240, 200);
    }

    if (!msg.empty()) {
      drawTextC(81, 391, msg.c_str(), 0, 0, 0);
      drawTextC(80, 390, msg.c_str(), 255, 240, 120);
    }

    if (renameMode) {
      setColor(20, 15, 5);
      fillRect(80, 320, 640, 44);
      setColor(80, 60, 10);
      drawRectOutline(80, 320, 640, 44);

      drawTextC(121, 348, "Rename selected save:", 0, 0, 0);
      drawTextC(120, 347, "Rename selected save:", 255, 220, 120);
      drawTextC(121, 331, renameBuffer.c_str(), 0, 0, 0);
      drawTextC(120, 330, renameBuffer.c_str(), 230, 230, 230);
      return;
    }

    if (saveSlots.empty()) {
      drawTextC(271, 261, "No autosaves found", 0, 0, 0);
      drawTextC(270, 260, "No autosaves found", 255, 255, 0);
    } else {
      for (int i = 0; i < (int)saveSlots.size() && i < 6; i++) {
        int boxY = 345 - i * 50;
        bool sel = (i == saveSelected);

        if (sel) {
          setColor(80, 60, 10);
          fillRect(78, boxY - 12, 644, 34);
          setColor(60, 45, 0);
          fillRect(80, boxY - 10, 640, 30);
        } else {
          setColor(10, 10, 25);
          fillRect(80, boxY - 10, 640, 30);
        }

        drawTextC(121, boxY + 13, saveSlots[i].displayName, 0, 0, 0);
        drawTextC(120, boxY + 12, saveSlots[i].displayName,
                  sel ? 255 : 220, sel ? 220 : 220, sel ? 120 : 220);

        drawTextC(121, boxY - 5, saveSlots[i].lastSaved, 0, 0, 0);
        drawTextC(120, boxY - 6, saveSlots[i].lastSaved, 180, 180, 180);

        if (hasActiveSaveSlot() &&
            std::strcmp(getActiveSavePath(), saveSlots[i].filePath) == 0) {
          drawTextC(571, boxY + 13, "ACTIVE", 0, 0, 0);
          drawTextC(570, boxY + 12, "ACTIVE", 120, 255, 120);
        }
      }
    }
    return;
  }

  // ---- MAIN MENU ----
  // Draw PNG image buttons (preloaded textures, no DIB error)
  for (int i = 0; i < ITEM_COUNT; i++) {
    int bx = BTN_CENTER_X - BTN_W / 2;
    int by = BTN_START_Y - i * BTN_GAP;
    bool hov = (i == selected);
    int texId = btnTex[i][hov ? 1 : 0];
    if (texId >= 0)
      drawChunk(bx, by, BTN_W, BTN_H, texId);
  }

  drawTextC(221, 21, "W/S = move   ENTER/SPACE = select   or click", 0, 0, 0);
  drawTextC(220, 20, "W/S = move   ENTER/SPACE = select   or click", 200, 200,
            200);
}
