#include "BossCutscene1.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/SaveSystem.h"
#include "BossFightScene1.h"
#include "../scenes/PlayScene.h"
#include <cmath>
#include <cstring>

// BOSS 1 CUTSCENE
// This scene now supports both the pre-boss intro and the
// post-boss victory sequence.

struct B1Slide {
    const char* title;
    const char* line1;
    const char* line2;
    const char* line3;
    int bgR, bgG, bgB;
    int duration;
};

static const B1Slide preSlides[] = {
    { "THE CASTLE KNIGHT",
      "Guardian of the outer walls...",
      "A warrior of iron will...",
      "He does not yield.",
      80, 10, 10, 300 },

    { "PREPARE FOR BATTLE",
      "Steel your heart...",
      "The Knight awaits inside...",
      "Press SPACE to fight!",
      60, 5, 5, 300 }
};

static const B1Slide postSlides[] = {
    { "THE KNIGHT FALLS",
      "The castle gate is broken...",
      "Its iron guardian lies silent...",
      "The path ahead now opens.",
      70, 18, 18, 260 },

    { "VICTORY",
      "The outer wall is reclaimed.",
      "Return to the overworld.",
      "New roads await your blade.",
      50, 10, 10, 260 }
};

BossCutscene1::BossCutscene1(Mode mode)
    : slideIndex(0), timer(0), fadeTimer(0), mode(mode) {
}

void BossCutscene1::update() {
    const B1Slide* slides = (mode == PRE_BOSS) ? preSlides : postSlides;
    const int slideCount = 2;

    timer++;
    fadeTimer++;

    if (consumeKey(32) || consumeKey(13)) {
        slideIndex++;
        timer = 0;
        fadeTimer = 0;
    }
    else if (timer >= slides[slideIndex].duration) {
        slideIndex++;
        timer = 0;
        fadeTimer = 0;
    }

    if (slideIndex >= slideCount) {
        if (mode == PRE_BOSS) {
            Engine::changeScene(new BossFightScene1());
        } else {
            // Show the next destination as soon as the player returns to the world.
            setWorldBanner("SANCTUM LOCATION REVEALED", 180);
            Engine::changeScene(new PlayScene());
        }
    }
}

void BossCutscene1::draw() {
    const B1Slide* slides = (mode == PRE_BOSS) ? preSlides : postSlides;
    const int slideCount = 2;
    if (slideIndex >= slideCount) return;

    const B1Slide& s = slides[slideIndex];

    // [IMAGE SLOT 0 / 1]
    setColor(s.bgR, s.bgG, s.bgB);
    fillRect(0, 0, 800, 450);

    setColor(200, 100, 60);
    fillRect(60, 340, 680, 2);
    fillRect(60, 130, 680, 2);

    int tx = 400 - (int)(strlen(s.title) * 8) / 2;
    drawTextC(tx + 1, 361, s.title, 0, 0, 0);
    drawTextC(tx, 360, s.title, 255,120,60);

    if (s.line1[0]) { int x = 400 - (int)(strlen(s.line1) * 8) / 2; drawTextC(x + 1, 271, s.line1, 0, 0, 0); drawTextC(x, 270, s.line1, 220, 220, 220); }
    if (s.line2[0]) { int x = 400 - (int)(strlen(s.line2) * 8) / 2; drawTextC(x + 1, 241, s.line2, 0, 0, 0); drawTextC(x, 240, s.line2, 200, 200, 200); }
    if (s.line3[0]) { int x = 400 - (int)(strlen(s.line3) * 8) / 2; drawTextC(x + 1, 211, s.line3, 0, 0, 0); drawTextC(x, 210, s.line3, 180, 180, 180); }

    int sx = 400 - (slideCount * 20) / 2;
    for (int i = 0; i < slideCount; i++) {
        if (i == slideIndex) { setColor(255,120,60); fillRect(sx + i * 20 - 4, 155, 9, 9); }
        else { setColor(100, 100, 100); fillRect(sx + i * 20 - 3, 156, 7, 7); }
    }

    const char* hint = (mode == PRE_BOSS) ? "SPACE / ENTER to skip" : "SPACE / ENTER to continue";
    float pulse = 180 + 70 * (float)sin(timer * 0.08);
    int p = (int)pulse;
    int hx = 400 - (int)(strlen(hint) * 8) / 2;
    drawTextC(hx + 1, 101, hint, 0, 0, 0);
    drawTextC(hx, 100, hint, p, p / 2, 0);
    if (fadeTimer < FADE_FRAMES) {
        int fade = (int)(255.f * (1.f - (float)fadeTimer / FADE_FRAMES));
        if (fade > 0) {
            setColor(0, 0, 0);
            for (int r = 0; r < 450; r += 2) {
                if (((r / 2) % 2 == 0) && ((fade * (r % 30)) / 255 < 2)) fillRect(0, r, 800, 1);
            }
        }
    }
}
