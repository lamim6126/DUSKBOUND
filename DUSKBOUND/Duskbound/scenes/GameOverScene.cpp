#include "GameOverScene.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/SaveSystem.h"
#include "MainMenuScene.h"
#include "PlayScene.h"

bool GameOverScene::assetsLoaded = false;
int GameOverScene::defeatTexture = -1;
int GameOverScene::endingTexture = -1;

// Load optional background images for the defeat and ending screens.
void GameOverScene::loadAssets() {
    if (assetsLoaded) return;

    defeatTexture = loadImage("assets/images/gameover/gameover.png");
    endingTexture = loadImage("assets/images/gameover/ending.png");
    assetsLoaded = true;
}

GameOverScene::GameOverScene(Mode mode) : mode(mode) {
    loadAssets();
}

void GameOverScene::update() {
    // Final credits page -> return to the main menu after the player confirms.
    if (mode == ENDING_CREDITS) {
        if (consumeKey('\r') || consumeKey('\n') || consumeKey(32)) {
            getGameState().endingSeen = 1;
            saveActiveGame();
            Engine::changeScene(new MainMenuScene());
        }
        return;
    }

    // Defeat screen -> open the load-save page directly.
    if (consumeKey('\r') || consumeKey('\n') || consumeKey(32) ||
        consumeKey('l') || consumeKey('L')) {
        Engine::changeScene(new MainMenuScene(4));
        return;
    }

    // Optional shortcut back to the main menu root.
    if (consumeKey('b') || consumeKey('B')) {
        Engine::changeScene(new MainMenuScene());
    }
}

void GameOverScene::draw() {
    int bg = (mode == ENDING_CREDITS) ? endingTexture : defeatTexture;
    if (bg >= 0) {
        drawImage(0, 0, 800, 450, bg);
    } else {
        if (mode == ENDING_CREDITS) setColor(18, 12, 28);
        else setColor(20, 0, 0);
        fillRect(0, 0, 800, 450);
    }

    if (mode == ENDING_CREDITS) {
        drawTextC(326, 381, "THE END", 0, 0, 0);
        drawTextC(325, 380, "THE END", 255, 220, 120);

        drawTextC(221, 321, "Duskbound - Final Journey Complete", 0, 0, 0);
        drawTextC(220, 320, "Duskbound - Final Journey Complete", 220, 220, 220);

        drawTextC(286, 271, "Credits", 0, 0, 0);
        drawTextC(285, 270, "Credits", 200, 180, 120);

        drawTextC(241, 231, "Developed by Raiyan", 0, 0, 0);
        drawTextC(240, 230, "Developed by Raiyan", 230, 230, 230);
        drawTextC(241, 206, "Developed by Shoscha", 0, 0, 0);
        drawTextC(240, 205, "Developed by Shoscha", 230, 230, 230);
        drawTextC(241, 181, "Developed by Lamim", 0, 0, 0);
        drawTextC(240, 180, "Developed by Lamim", 230, 230, 230);

        drawTextC(161, 131, "The kingdom is safe.", 0, 0, 0);
        drawTextC(160, 130, "The kingdom is safe.", 200, 200, 200);

		char finalScore[64];
		sprintf_s(finalScore, sizeof(finalScore), "Final Score: %d", getGameState().totalScore);
		drawTextC(321, 151, finalScore, 0, 0, 0);
		drawTextC(320, 150, finalScore, 100, 255, 100);

        drawTextC(191, 71, "Press ENTER or SPACE to return to the Main Menu", 0, 0, 0);
        drawTextC(190, 70, "Press ENTER or SPACE to return to the Main Menu", 255, 230, 120);
    } else {
        drawTextC(301, 251, "GAME OVER", 0, 0, 0);
        drawTextC(300, 250, "GAME OVER", 255, 60, 60);

        drawTextC(171, 206, "Press ENTER / SPACE / L to open Load Save", 0, 0, 0);
        drawTextC(170, 205, "Press ENTER / SPACE / L to open Load Save", 220, 220, 220);

        drawTextC(231, 166, "Press B to return to the Main Menu", 0, 0, 0);
        drawTextC(230, 165, "Press B to return to the Main Menu", 255, 255, 255);
    }
}
