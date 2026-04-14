#ifndef BOSSFIGHTSCENE1_H
#define BOSSFIGHTSCENE1_H
#include "../core/Scene.h"
#include "../entities/PlayerBossFightController.h"

// BOSS FIGHT SCENE 1 - CASTLE KNIGHT
// Melee fighter boss with spear.
// Player rendered via PlayerBossFightController
// (PNG textures from assets/images/player_fight/)
// Boss rendered via local PNG texture cache
// (PNG textures from assets/images/boss_fight/)

class BossFightScene1 : public Scene {
private:

	// PLAYER - shared controller handles all
	// movement, combat, animation AND textures
	PlayerBossFightController* player;

	// BOSS STATE
	float bossX, bossY;
	int   bossHP, bossMaxHP;
	int   bossPhase;
	bool  bossFacingRight;

	// Boss animation
	enum BossState { BOSS_IDLE, BOSS_WALK, BOSS_ATTACK };
	BossState bossState;
	int bossAnimFrame;
	int bossAnimTimer;

	// Boss AI
	int bossAttackTimer;     // Countdown to next attack
	int bossAttackCooldown;  // Time between attacks (changes by phase)

	// PNG TEXTURE CACHE - Boss + Background
	// Loaded once via loadTextures().
	// Player textures live in PlayerBossFightController.
	//
	// BOSS_WALK_FRAMES   : frames 00-07 (8 total)
	// BOSS_ATTACK_FRAMES : frames 00-06 (7 total)
	// Sprite display size: 192x192
	static int bgTexture;           // bg.png
	static int bossWalkL[8];        // walkleft__00.png  .. walkleft__07.png
	static int bossWalkR[8];        // walkright__00.png .. walkright__07.png
	static int bossAttackL[7];      // atleft__00.png    .. atleft__06.png
	static int bossAttackR[7];      // atright__00.png   .. atright__06.png
	static bool texturesLoaded;     // Guard: load only once

	// UI
	int  victoryTimer;
	bool bossDefeated;

public:
	BossFightScene1();
	~BossFightScene1();
	void update();
	void draw();

private:
	void loadTextures();       // Load all PNG sprites once
	void updatePlayer();
	void updateBoss();
	void updateAnimations();
	void checkCollisions();
	void drawUI();

	// Returns correct boss texture ID for current state/direction/frame
	int getBossTexture() const;
};

#endif