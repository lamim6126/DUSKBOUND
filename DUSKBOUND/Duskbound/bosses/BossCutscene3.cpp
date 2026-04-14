#include "BossCutscene3.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "BossFightScene3.h"
#include "../scenes/PlayScene.h"
#include "../scenes/GameOverScene.h"
#include <cmath>
#include <cstring>

// BOSS 3 CUTSCENE
// This scene now supports both the pre-boss intro and the
// post-boss victory sequence.

struct B3Slide {
    const char* title;
    const char* line1;
    const char* line2;
    const char* line3;
    int bgR, bgG, bgB;
    int duration;
};

static const B3Slide preSlides[] = {
    { "THE CORRUPTED KING",
      "Once the ruler of all kingdoms...",
      "Now a husk of darkness and rage...",
      "The final trial awaits.",
      60, 45, 5, 300 },

    { "PREPARE FOR BATTLE",
      "He commands both steel and shadow...",
      "The Corrupted King sits upon his throne...",
      "Press SPACE to fight!",
      45, 30, 0, 300 }
};

static const B3Slide postSlides[] = {
    { "THE KING IS SLAIN",
      "The throne room grows still...",
      "The curse shatters with his fall...",
      "The long night finally ends.",
      65, 45, 10, 280 },

    { "DAWN RETURNS",
      "The kingdom is free once more.",
      "Witness the end of the tale.",
      "Press SPACE to see the credits.",
      45, 30, 0, 280 }
};

BossCutscene3::BossCutscene3(Mode mode)
    : slideIndex(0), timer(0), fadeTimer(0), mode(mode) {
}

void BossCutscene3::update() {
    const B3Slide* slides = (mode == PRE_BOSS) ? preSlides : postSlides;
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
        if (mode == PRE_BOSS) Engine::changeScene(new BossFightScene3());
        else Engine::changeScene(new GameOverScene(GameOverScene::ENDING_CREDITS));
    }
}

void BossCutscene3::draw() {
    const B3Slide* slides = (mode == PRE_BOSS) ? preSlides : postSlides;
    const int slideCount = 2;
    if (slideIndex >= slideCount) return;

    const B3Slide& s = slides[slideIndex];

    // [IMAGE SLOT 0 / 1]
    setColor(s.bgR, s.bgG, s.bgB);
    fillRect(0, 0, 800, 450);

    setColor(215, 175, 0);
    fillRect(60, 340, 680, 2);
    fillRect(60, 130, 680, 2);

    int tx = 400 - (int)(strlen(s.title) * 8) / 2;
    drawTextC(tx + 1, 361, s.title, 0, 0, 0);
    drawTextC(tx, 360, s.title, 255,215,0);

    if (s.line1[0]) { int x = 400 - (int)(strlen(s.line1) * 8) / 2; drawTextC(x + 1, 271, s.line1, 0, 0, 0); drawTextC(x, 270, s.line1, 220, 220, 220); }
    if (s.line2[0]) { int x = 400 - (int)(strlen(s.line2) * 8) / 2; drawTextC(x + 1, 241, s.line2, 0, 0, 0); drawTextC(x, 240, s.line2, 200, 200, 200); }
    if (s.line3[0]) { int x = 400 - (int)(strlen(s.line3) * 8) / 2; drawTextC(x + 1, 211, s.line3, 0, 0, 0); drawTextC(x, 210, s.line3, 180, 180, 180); }

    int sx = 400 - (slideCount * 20) / 2;
    for (int i = 0; i < slideCount; i++) {
        if (i == slideIndex) { setColor(255,215,0); fillRect(sx + i * 20 - 4, 155, 9, 9); }
        else { setColor(100, 100, 100); fillRect(sx + i * 20 - 3, 156, 7, 7); }
    }

    const char* hint = (mode == PRE_BOSS) ? "SPACE / ENTER to skip" : "SPACE / ENTER to continue";
    float pulse = 180 + 70 * (float)sin(timer * 0.08);
    int p = (int)pulse;
    int hx = 400 - (int)(strlen(hint) * 8) / 2;
    drawTextC(hx + 1, 101, hint, 0, 0, 0);
    drawTextC(hx, 100, hint, p, p, 0);
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
