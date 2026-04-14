#include "CutsceneScene.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../scenes/MainMenuScene.h"


#include <cmath>

// Slide definitions - edit these to change the story!
static const CutsceneSlide slides[] = {
    {"ROYAL HERIRAGE", "Born to rule...",
     "Son of King Odeseous...", "Prince of the seven kingdoms...", 10, 15,
     30, // dark-navy tint
     500},
    {"A KNIGHT'S PATH", "You chose not to seek power...",
     "But to seek strength...",
     "To fight your own battles...", 30, 10, 40, // deep purple tint
     500},
    {"YOUR JOURNEY BEGINS", "Go on a venture...",
     "Fight for glory...", "Achieve greatness...", 10, 30,
     40, // dark-teal tint
     500}};
static const int SLIDE_COUNT = 3;

CutsceneScene::CutsceneScene() {
  slideIndex = 0;
  timer = 0;
  fadeTimer = 0;
  fadingOut = false;
}

void CutsceneScene::update() {
  timer++;
  fadeTimer++;

  // Skip / advance on SPACE or ENTER
  if (consumeKey(32) || consumeKey(13)) {
    slideIndex++;
    timer = 0;
    fadeTimer = 0;
    fadingOut = false;
    if (slideIndex >= SLIDE_COUNT) {
      Engine::changeScene(new MainMenuScene());
      return;
    }
    return;
  }

  // Auto-advance when slide duration expires
  if (timer >= slides[slideIndex].duration) {
    slideIndex++;
    timer = 0;
    fadeTimer = 0;
    fadingOut = false;
    if (slideIndex >= SLIDE_COUNT) {
      Engine::changeScene(new MainMenuScene());
      return;
    }
  }
}

void CutsceneScene::draw() {
  if (slideIndex >= SLIDE_COUNT)
    return;

  const CutsceneSlide &s = slides[slideIndex];

  // --- Background (full screen) ---
  setColor(s.bgR, s.bgG, s.bgB);
  fillRect(0, 0, 800, 450);

  // Subtle gradient overlay (darker bottom strip)
  setColor(0, 0, 0);
  for (int i = 0; i < 60; i++) {
    fillRect(0, i, 800, 1);
  }
  // Subtle gradient overlay (darker top strip)
  for (int i = 390; i < 450; i++) {
    fillRect(0, i, 800, 1);
  }

  // --- Decorative horizontal lines (near screen edges) ---
  setColor(180, 160, 100);
  fillRect(40, 410, 720, 2);
  fillRect(40, 38, 720, 2);

  // --- Title (near top, inside upper decorative line) ---
  int titleX = 400 - (int)(strlen(s.title) * 8) / 2;
  drawTextC(titleX + 1, 421, s.title, 0, 0, 0);
  drawTextC(titleX, 420, s.title, 255, 215, 100);

  // --- Body lines (vertically centered across full height) ---
  if (s.line1 && s.line1[0]) {
    int x1 = 400 - (int)(strlen(s.line1) * 8) / 2;
    drawTextC(x1 + 1, 261, s.line1, 0, 0, 0);
    drawTextC(x1, 260, s.line1, 220, 220, 220);
  }
  if (s.line2 && s.line2[0]) {
    int x2 = 400 - (int)(strlen(s.line2) * 8) / 2;
    drawTextC(x2 + 1, 221, s.line2, 0, 0, 0);
    drawTextC(x2, 220, s.line2, 200, 200, 200);
  }
  if (s.line3 && s.line3[0]) {
    int x3 = 400 - (int)(strlen(s.line3) * 8) / 2;
    drawTextC(x3 + 1, 181, s.line3, 0, 0, 0);
    drawTextC(x3, 180, s.line3, 180, 180, 180);
  }

  // --- Slide progress dots (near bottom) ---
  int dotSpacing = 20;
  int totalDots = SLIDE_COUNT;
  int startX = 400 - (totalDots * dotSpacing) / 2;
  for (int i = 0; i < totalDots; i++) {
    if (i == slideIndex) {
      setColor(255, 215, 100);
      fillRect(startX + i * dotSpacing - 4, 75, 9, 9);
    } else {
      setColor(100, 100, 100);
      fillRect(startX + i * dotSpacing - 3, 76, 7, 7);
    }
  }

  // --- Pulsing "press SPACE" hint (at very bottom) ---
  float pulse = (float)(180 + 70 * sin(timer * 0.08));
  int p = (int)pulse;
  const char* hint = "SPACE / ENTER to skip";
  int hintX = 400 - (int)(strlen(hint) * 8) / 2;
  drawTextC(hintX + 1, 51, hint, 0, 0, 0);
  drawTextC(hintX, 50, hint, p, p, 0);

  // --- Fade-in overlay (first FADE_FRAMES frames of each slide) ---
  int fade = 0;
  if (fadeTimer < FADE_FRAMES) {
    fade = (int)(255.0f * (1.0f - (float)fadeTimer / FADE_FRAMES));
  }
  if (fade > 0) {
    setColor(0, 0, 0);
    // Draw horizontal scanlines with decreasing density
    for (int row = 0; row < 450; row += 2) {
      if ((row / 2) % 2 == 0) {
        int blend = (fade * (row % 30)) / 255;
        if (blend < 2)
          fillRect(0, row, 800, 1);
      }
    }
  }
}
