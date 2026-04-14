#ifndef BOSSFIGHTSCENE2_H
#define BOSSFIGHTSCENE2_H

#include "../core/Scene.h"
#include "../entities/PlayerBossFightController.h"

// BOSS FIGHT SCENE 2 - SANCTUM SORCERER
// Ranged magic boss with teleportation
// Player rendered via PlayerBossFightController
// Boss rendered via local PNG texture cache

class BossFightScene2 : public Scene {
private:
	// PLAYER - Using shared controller
	// Handles all player logic AND PNG rendering
	PlayerBossFightController* player;

	// BOSS STATE
	float bossX, bossY;
	int   bossHP, bossMaxHP;
	int   bossPhase;          // 3 phases
	bool  bossFacingRight;

	// Boss animation
	enum BossState { BOSS_IDLE, BOSS_WALK, BOSS_ATTACK, BOSS_TELEPORT };
	BossState bossState;
	int bossAnimFrame;
	int bossAnimTimer;

	// Projectile system for ranged attacks
	struct Projectile {
		float x, y;
		float vx, vy;
		bool  active;
	};
	Projectile projectiles[7];
	// projectileCount removed - code always scans all 5 slots directly

	// Boss AI
	int bossAttackTimer;
	int bossAttackCooldown;
	int bossTeleportTimer;
	int bossRecoveryTimer;      // brief idle window after casting / teleporting

	// PNG TEXTURE CACHE - Boss + Background
	// Loaded once via loadTextures(), shared
	// across the scene lifetime.
	// Player textures live in PlayerBossFightController.
	static int bgTexture;                    // bg.png

	static int bossWalkL[8];                 // walkleft__00.png  .. walkleft__07.png
	static int bossWalkR[8];                 // walkright__00.png .. walkright__07.png

	static int bossAttackL[7];               // atleft__00.png  .. atleft__06.png
	static int bossAttackR[7];               // atright__00.png .. atright__06.png

	static bool texturesLoaded;              // guard so we load only once

	// UI
	int  victoryTimer;
	bool bossDefeated;

public:
	BossFightScene2();
	~BossFightScene2();

	void update();
	void draw();

private:
	void loadTextures();          // Load all PNG sprites once

	void updatePlayer();
	void updateBoss();
	void updateProjectiles();
	void updateAnimations();
	void checkCollisions();
	void drawUI();

	// Returns the correct boss texture ID for the current state/frame
	int getBossTexture() const;
};

#endif