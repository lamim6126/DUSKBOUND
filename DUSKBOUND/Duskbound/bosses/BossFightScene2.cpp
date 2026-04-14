#include "BossFightScene2.h"
#include "BossCutscene2.h"
#include "../scenes/GameOverScene.h"
#include "../core/SaveSystem.h"
#include "../core/GameState.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/Sound.h"
#include "../scenes/PlayScene.h"
#include "../core/GameConfig.h"
#include "../systems/Powerups.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Boss fight background music
static const char* BOSS_FIGHT_MUSIC = "assets/audio/boss.wav";

// BOSS ANIMATION CONSTANTS
// BOSS_WALK_FRAMES   : frames 00-07 (8 total)
// BOSS_ATTACK_FRAMES : frames 00-06 (7 total)
const int BOSS_ANIM_SPEED = 5;
const int BOSS_WALK_FRAMES = 8;
const int BOSS_ATTACK_FRAMES = 7;

// Boss 2 uses a large framed sprite with empty
// margins, so aim/fire decisions should use a
// visual body centre instead of the raw sprite X.
static float boss2BodyCentreX(float bossX) {
	return bossX + 128.0f;   // boss is drawn at 256px wide
}

static float boss2PlayerCentreX(const PlayerBossFightController* player) {
	return player->x + 32.0f;   // 64px player sprite -> centre
}

// STATIC TEXTURE STORAGE - defined here once
// All -1 until loadTextures() runs.
int  BossFightScene2::bgTexture = -1;

int  BossFightScene2::bossWalkL[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene2::bossWalkR[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

int  BossFightScene2::bossAttackL[7] = { -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene2::bossAttackR[7] = { -1, -1, -1, -1, -1, -1, -1 };

bool BossFightScene2::texturesLoaded = false;

// LOAD TEXTURES
// Loads all PNG sprites once.
// Player textures are handled by
// PlayerBossFightController::loadPlayerTextures()
// which is called automatically in the constructor.
void BossFightScene2::loadTextures() {
	if (texturesLoaded) return;   // already loaded, skip

	printf("=== LOADING BOSS 2 PNG TEXTURES ===\n");

	// ---- Background ----
	bgTexture = loadImage("assets/images/boss_fight2/bg.png");
	printf("  bg.png : id=%d\n", bgTexture);

	// ---- Boss walk sprites ----
	// walkleft__00.png  .. walkleft__07.png
	// walkright__00.png .. walkright__07.png
	for (int i = 0; i < BOSS_WALK_FRAMES; i++) {
		char pathL[256], pathR[256];
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/boss_fight2/walkleft__%02d.png", i);
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/boss_fight2/walkright__%02d.png", i);
		bossWalkL[i] = loadImage(pathL);
		bossWalkR[i] = loadImage(pathR);
	}
	printf("  Boss walk frames loaded (%d each side)\n", BOSS_WALK_FRAMES);

	// ---- Boss attack sprites ----
	// atleft__00.png  .. atleft__06.png
	// atright__00.png .. atright__06.png
	for (int i = 0; i < BOSS_ATTACK_FRAMES; i++) {
		char pathL[256], pathR[256];
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/boss_fight2/atleft__%02d.png", i);
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/boss_fight2/atright__%02d.png", i);
		bossAttackL[i] = loadImage(pathL);
		bossAttackR[i] = loadImage(pathR);
	}
	printf("  Boss attack frames loaded (%d each side)\n", BOSS_ATTACK_FRAMES);

	texturesLoaded = true;
	printf("=== BOSS 2 TEXTURES READY ===\n");
}

// CONSTRUCTOR
BossFightScene2::BossFightScene2() {
	// Clear all keys
	memset(keys, 0, sizeof(keys));

	// Initialize shared player controller
	// This also calls loadPlayerTextures() internally if not done yet
	player = new PlayerBossFightController();

	// Load boss PNG textures (no-op if already loaded)
	loadTextures();

	// Boss setup - use GameConfig constants
	bossX = 550.0f;
	bossY = FLOOR_Y-46.0f;
	bossHP = BOSS2_MAX_HP;
	bossMaxHP = BOSS2_MAX_HP;
	bossPhase = 1;
	bossFacingRight = false;
	bossState = BOSS_IDLE;
	bossAnimFrame = 0;
	bossAnimTimer = 0;

	// Boss AI
	// Phase 1 should feel measured, with obvious openings before the
	// later phases tighten the pressure.
	bossAttackCooldown = 120;
	bossAttackTimer = bossAttackCooldown;
	bossTeleportTimer = 360;   // Slower opening teleport rhythm
	bossRecoveryTimer = 0;

	// Initialize projectiles - scan all 5 slots directly, no counter needed
	for (int i = 0; i < 7; i++) {
		projectiles[i].active = false;
	}

	// Victory state
	victoryTimer = 0;
	bossDefeated = false;

	// Start boss fight background music on the MCI channel (loops without interrupting SFX)
	playMusic(BOSS_FIGHT_MUSIC);
}

// DESTRUCTOR
BossFightScene2::~BossFightScene2() {
	delete player;
}

// UPDATE PLAYER
// Delegates all logic to the shared controller.

void BossFightScene2::updatePlayer() {
	// Blocking check (highest priority)
	player->updateBlocking();

	// CRITICAL: Update animation and cooldowns even when blocking!
	player->updateAnimation();
	player->updateCooldowns();

	// If blocking, skip movement/attack but keep animating
	if (player->isBlocking) {
		return;
	}

	// Jump physics
	player->updateJump();

	// Movement
	float dx = player->updateMovement();
	player->x += dx;

	// Attack - use the boss body centre instead of the sprite-left.
	if (player->updateAttack()) {
		float playerCentreX = boss2PlayerCentreX(player);
		float bossCentreX = boss2BodyCentreX(bossX);
		float dist = std::fabs(playerCentreX - bossCentreX);

		if (dist < ATTACK_RANGE && !bossDefeated) {
			// Check if back attack (hit from behind)
			bool isBackAttack = (playerCentreX < bossCentreX && bossFacingRight) ||
				(playerCentreX > bossCentreX && !bossFacingRight);

			int damage = player->getAttackDamage(isBackAttack);
			bossHP -= damage;
			if (bossHP < 0) bossHP = 0;
		}
	}

	// Reset to idle if not doing anything
	player->resetToIdle();
}

// UPDATE BOSS
// 3-phase AI with teleport and ranged attacks

void BossFightScene2::updateBoss() {
	if (bossDefeated) return;

	// Check defeat
	if (bossHP <= 0) {
		bossDefeated = true;
		victoryTimer = 0;

		int hpScore = player->hp * 3;
		getGameState().totalScore += hpScore;

		// Persist the boss-clear state immediately.
		GameState& data = getGameState();
		data.bossDefeated[1] = 1;
		setWorldBanner("SANCTUM SORCERER DEFEATED", 180);
		saveActiveGame();
		return;
	}

	float playerCentreX = boss2PlayerCentreX(player);
	float bossCentreX = boss2BodyCentreX(bossX);
	float dxToPlayer = playerCentreX - bossCentreX;
	float absDist = std::fabs(dxToPlayer);

	// ---- Phase transitions ----
	// Keep the three phases distinct: phase 1 is patient, phase 2 is quicker,
	// and phase 3 is the most aggressive without collapsing into nonstop spam.
	if (bossHP < bossMaxHP * 0.66f && bossPhase == 1) {
		bossPhase = 2;
		bossAttackCooldown = 96;
		if (bossTeleportTimer > 285) bossTeleportTimer = 285;
	}
	if (bossHP < bossMaxHP * 0.33f && bossPhase == 2) {
		bossPhase = 3;
		bossAttackCooldown = 74;
		if (bossTeleportTimer > 220) bossTeleportTimer = 220;
	}

	// Use phase-specific windows so the pacing stays readable.
	int nearTeleportWindow = 95;
	int postTeleportRecovery = 36;
	int postCastRecovery = 28;
	int resetTeleportTimer = 360;

	if (bossPhase == 2) {
		nearTeleportWindow = 80;
		postTeleportRecovery = 28;
		postCastRecovery = 22;
		resetTeleportTimer = 285;
	} else if (bossPhase == 3) {
		nearTeleportWindow = 65;
		postTeleportRecovery = 20;
		postCastRecovery = 16;
		resetTeleportTimer = 220;
	}

	// Always face the player's body centre.
	bossFacingRight = (playerCentreX > bossCentreX);

	// Tick the short recovery window after a cast / teleport.
	if (bossRecoveryTimer > 0) {
		bossRecoveryTimer--;
	}

	// If the player stays too close to the wand side, ask for a reposition soon
	// but do not snap-teleport almost instantly.
	if (absDist < 110.0f && bossTeleportTimer > nearTeleportWindow) {
		bossTeleportTimer = nearTeleportWindow;
	}

	// ---- Teleport ----
	// Only teleport when the short recovery window is over.
	if (bossRecoveryTimer == 0) {
		bossTeleportTimer--;
		if (bossTeleportTimer <= 0) {
			bossState = BOSS_TELEPORT;
			bossAnimFrame = 0;
			bossAnimTimer = 0;

			if (playerCentreX < 400.0f) {
				bossX = 500.0f + (rand() % 150);
			}
			else {
				bossX = 100.0f + (rand() % 150);
			}

			// After a teleport, stand briefly before doing anything else.
			bossRecoveryTimer = postTeleportRecovery;
			bossTeleportTimer = resetTeleportTimer;
			if (bossAttackTimer < 40) bossAttackTimer = 40;

			bossCentreX = boss2BodyCentreX(bossX);
			dxToPlayer = playerCentreX - bossCentreX;
			absDist = std::fabs(dxToPlayer);
			bossFacingRight = (playerCentreX > bossCentreX);
		}
	}

	// ---- Ranged attack ----
	// The sorcerer should only fire when the player is in a sensible lane.
	// If the player is too close, wait for the reposition window instead.
	if (bossRecoveryTimer == 0) {
		bossAttackTimer--;

		if (bossAttackTimer <= 0) {
			if (absDist < 130.0f) {
				bossAttackTimer = 26;
			}
			else if (absDist > 560.0f) {
				bossAttackTimer = 20;
			}
			else {
				bossState = BOSS_ATTACK;
				bossAnimFrame = 0;
				bossAnimTimer = 0;
				bossAttackTimer = bossAttackCooldown;
				bossRecoveryTimer = postCastRecovery;

				// Stand for a moment after casting before the next teleport.
				if (bossTeleportTimer < nearTeleportWindow) bossTeleportTimer = nearTeleportWindow;

				// Spawn from a wand point relative to the boss centre instead of
				// the raw sprite edge, so left/right shots line up with the player better.
				for (int i = 0; i < 7; i++) {
					if (!projectiles[i].active) {
						projectiles[i].x = bossCentreX + (bossFacingRight ? 56.0f : -56.0f);
						projectiles[i].y = bossY + 92.0f;

						projectiles[i].vx = BOSS2_PROJECTILE_SPEED * (bossFacingRight ? 1.0f : -1.0f);
						projectiles[i].vy = 0.0f;
						projectiles[i].active = true;
						break;
					}
				}
			}
		}
	}

	// Return to idle after action frames finish.
	if (bossState == BOSS_ATTACK && bossAnimFrame >= BOSS_ATTACK_FRAMES - 1) {
		bossState = BOSS_IDLE;
	}
	if (bossState == BOSS_TELEPORT) {
		bossState = BOSS_IDLE;
	}
}

// UPDATE PROJECTILES
// Move fireballs; check player collision.
// Player can dodge by jumping over them.

void BossFightScene2::updateProjectiles() {
	for (int i = 0; i < 7; i++) {
		if (!projectiles[i].active) continue;

		// Move projectile
		projectiles[i].x += projectiles[i].vx;
		projectiles[i].y += projectiles[i].vy;

		// Collision with player - use the player centre instead of sprite-left.
		float playerCentreX = boss2PlayerCentreX(player);
		float playerCentreY = player->y + player->jumpY + 32.0f;
		float dx = projectiles[i].x - playerCentreX;
		float dy = projectiles[i].y - playerCentreY;
		float distSq = dx * dx + dy * dy;

		if (distSq < 40.0f * 40.0f) {   // 40^2 = 1600
			projectiles[i].active = false;
			player->takeDamage(BOSS2_DAMAGE);   // damage from GameConfig
		}

		// Deactivate if off-screen
		if (projectiles[i].x < -50 || projectiles[i].x > 850 ||
			projectiles[i].y < -50 || projectiles[i].y > 500) {
			projectiles[i].active = false;
		}
	}
}

// UPDATE ANIMATIONS
// Advances boss sprite frame counter.
// Player animation is handled by the controller.
void BossFightScene2::updateAnimations() {
	if (bossDefeated) return;

	bossAnimTimer++;
	if (bossAnimTimer >= BOSS_ANIM_SPEED) {
		bossAnimTimer = 0;
		bossAnimFrame++;

		switch (bossState) {
		case BOSS_IDLE:
			bossAnimFrame = 0;   // Idle = hold walk frame 0
			break;

		case BOSS_WALK:
			if (bossAnimFrame >= BOSS_WALK_FRAMES) bossAnimFrame = 0;
			break;

		case BOSS_ATTACK:
			// Hold last attack frame until state resets
			if (bossAnimFrame >= BOSS_ATTACK_FRAMES) {
				bossAnimFrame = BOSS_ATTACK_FRAMES - 1;
			}
			break;

		case BOSS_TELEPORT:
			bossAnimFrame = 0;
			break;
		}
	}
}

// CHECK COLLISIONS
// Victory / defeat scene transitions
void BossFightScene2::checkCollisions() {
	// Victory: countdown then return to overworld
	// Defeat: open the game-over / load-save flow.
	if (player->hp <= 0) {
		stopMusic();
		Engine::changeScene(new GameOverScene());
	}
}

// GET BOSS TEXTURE
// Returns the correct PNG texture ID for the
// current boss state, direction, and anim frame.
// Falls back to walk frame 0 if not found (-1).
int BossFightScene2::getBossTexture() const {
	switch (bossState) {

	case BOSS_IDLE:
		// Use walk frame 0 as the idle pose
		return bossFacingRight
			? bossWalkR[0]
			: bossWalkL[0];

	case BOSS_WALK:
		return bossFacingRight
			? bossWalkR[bossAnimFrame % BOSS_WALK_FRAMES]
			: bossWalkL[bossAnimFrame % BOSS_WALK_FRAMES];

	case BOSS_ATTACK:
		return bossFacingRight
			? bossAttackR[bossAnimFrame % BOSS_ATTACK_FRAMES]
			: bossAttackL[bossAnimFrame % BOSS_ATTACK_FRAMES];

	case BOSS_TELEPORT:
		// Show walk frame 0 during the instant teleport
		return bossFacingRight
			? bossWalkR[0]
			: bossWalkL[0];
	}

	return bossWalkR[0];   // safe fallback
}

// UPDATE - Main loop
void BossFightScene2::update() {

	// Victory countdown - wait, then show the post-boss cutscene.
	if (bossDefeated) {
		victoryTimer++;
		if (victoryTimer > 180) {
			stopMusic();
			Engine::changeScene(new BossCutscene2(BossCutscene2::POST_BOSS));
		}
		return;
	}

	updatePlayer();
	updateBoss();
	updateProjectiles();
	updateAnimations();
	checkCollisions();

	// Manual exit to overworld
	if (consumeKey('b') || consumeKey('B')) {
		stopMusic();
		Engine::changeScene(new PlayScene());
	}
}

// DRAW UI
// Health bars, phase indicator, controls hint
void BossFightScene2::drawUI() {

	// ---- Player HP bar ----
	drawTextC(11, 426, "Player HP:", 0, 0, 0);
	drawTextC(10, 425, "Player HP:", 255, 255, 255);

	setColor(100, 0, 0);
	fillRect(100, 425, 200, 20);

	int playerBarWidth = (int)(200.0f * (player->hp / (float)player->maxHP));
	setColor(0, 200, 0);
	fillRect(100, 425, playerBarWidth, 20);

	// ---- Boss name + HP bar ----
	// Boss 2 has the longest label, so keep the name above the bar to avoid
	// overlapping the health fill.
	drawTextC(501, 441, "Sanctum Sorcerer", 0, 0, 0);
	drawTextC(500, 440, "Sanctum Sorcerer", 255, 255, 255);

	setColor(100, 0, 0);
	fillRect(580, 421, 200, 18);

	int bossBarWidth = (int)(200.0f * (bossHP / (float)bossMaxHP));
	setColor(200, 0, 0);
	fillRect(580, 421, bossBarWidth, 18);

	// ---- Phase indicator ----
	char phaseText[32];
	sprintf_s(phaseText, sizeof(phaseText), "PHASE %d", bossPhase);
	drawTextC(581, 403, phaseText, 0, 0, 0);
	drawTextC(580, 402, phaseText, 255, 100, 100);

	// ---- Victory message ----
	if (bossDefeated) {
		drawTextC(251, 226, "THE SANCTUM CLEARED!", 0, 0, 0);
		drawTextC(250, 225, "THE SANCTUM CLEARED!", 255, 215, 0);
		char scoreText[64];
		int hpBonus = player->hp * 3;
		sprintf_s(scoreText, sizeof(scoreText), "HP Bonus: +%d  Total: %d", hpBonus, getGameState().totalScore);
		drawTextC(251, 201, scoreText, 0, 0, 0);
		drawTextC(250, 200, scoreText, 100, 255, 100);
	}

	// ---- Controls hint ----
	drawTextC(11, 11, "A/D=Move  SPACE=Attack  X=Block  W=Jump  B=Leave", 0, 0, 0);
	drawTextC(10, 10, "A/D=Move  SPACE=Attack  X=Block  W=Jump  B=Leave", 200, 200, 200);
}

// DRAW - Render everything
void BossFightScene2::draw() {

	// ---- Background PNG (or fallback solid colour) ----
	if (bgTexture >= 0) {
		drawImage(0, 0, 800, 450, bgTexture);
	}
	else {
		// Fallback: deep blue-purple gradient
		setColor(10, 5, 30);
		fillRect(0, 0, 800, 450);
	}

	// ---- Player - drawn via PlayerBossFightController texture cache ----
	// Adjust the 64x64 size to match your actual sprite sheet frame size.
	int playerTex = player->getPlayerTexture();
	if (playerTex >= 0) {
		drawImage((int)player->x,
			(int)(player->y + player->jumpY),
			64, 64,
			playerTex);
	}
	else {
		// Fallback: simple blue rectangle if textures failed to load
		setColor(50, 100, 200);
		fillRect((int)player->x,
			(int)(player->y + player->jumpY),
			32, 64);
	}

	// ---- Boss - drawn via local texture cache ----
	// Adjust 256x256 to match your actual boss sprite size.
	if (!bossDefeated) {
		int bossTex = getBossTexture();
		if (bossTex >= 0) {
			drawImage((int)bossX, (int)bossY, 256, 256, bossTex);
		}
		else {
			// Fallback: purple rectangle if textures failed to load
			setColor(120, 0, 180);
			fillRect((int)bossX, (int)bossY, 128, 128);
		}
	}

	// ---- Fireballs - orange squares ----
	// Replace fillRect with drawImage here if you add a fireball PNG
	for (int i = 0; i < 7; i++) {
		if (!projectiles[i].active) continue;
		setColor(255, 100, 0);
		fillRect((int)projectiles[i].x - 8, (int)projectiles[i].y - 8, 16, 16);
	}

	// ---- HUD overlay ----
	drawUI();
}