#include "BossFightScene1.h"
#include "BossCutscene1.h"
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

// Boss fight background music
static const char* BOSS_FIGHT_MUSIC = "assets/audio/boss.wav";

// BOSS ANIMATION CONSTANTS
// BOSS_WALK_FRAMES   : frames 00-07 (8 total)
// BOSS_ATTACK_FRAMES : frames 00-06 (7 total)
const int BOSS_ANIM_SPEED = 5;
const int BOSS_WALK_FRAMES = 8;
const int BOSS_ATTACK_FRAMES = 7;

// BOSS SPRITE & HITBOX CONSTANTS (192x192 sprite)
static const int   BOSS_SPRITE_W = 192;
static const int   BOSS_SPRITE_H = 192;
static const int   BOSS_BODY_BOT_Y = 44;   // sprite-Y of character feet (192-44)

static const int   BOSS_HIT_W = 40;
static const int   BOSS_HIT_H = 90;
static const int   BOSS_HIT_OX_L = 99;    // hitbox left offset, facing left
static const int   BOSS_HIT_OX_R = 53;    // hitbox left offset, facing right
static const int   BOSS_HIT_OY = 51;    // hitbox top offset (both directions)

// Small helper functions for boss 1 body math.
static float boss1BodyCentreX(float bossX, bool facingRight) {
	return bossX
		+ (facingRight ? BOSS_HIT_OX_R : BOSS_HIT_OX_L)
		+ BOSS_HIT_W * 0.5f;
}

static float boss1PlayerCentreX(const PlayerBossFightController* player) {
	return player->x + 32.0f;   // 64px player sprite -> centre
}

// STATIC TEXTURE STORAGE - defined here once.
// All -1 until loadTextures() runs.
int  BossFightScene1::bgTexture = -1;

int  BossFightScene1::bossWalkL[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene1::bossWalkR[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

int  BossFightScene1::bossAttackL[7] = { -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene1::bossAttackR[7] = { -1, -1, -1, -1, -1, -1, -1 };

bool BossFightScene1::texturesLoaded = false;

// LOAD TEXTURES
// Loads all boss + bg PNGs once.
// Player textures are handled automatically by
// PlayerBossFightController::loadPlayerTextures()
// which runs in the controller's constructor.
void BossFightScene1::loadTextures() {
	if (texturesLoaded) return;   // already loaded, skip

	printf("=== LOADING BOSS 1 PNG TEXTURES ===\n");

	// ---- Background ----
	bgTexture = loadImage("assets/images/boss_fight/bg.png");
	printf("  bg.png : id=%d\n", bgTexture);

	// ---- Boss walk sprites ----
	for (int i = 0; i < BOSS_WALK_FRAMES; i++) {
		char pathL[256], pathR[256];
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/boss_fight/walkleft__%02d.png", i);
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/boss_fight/walkright__%02d.png", i);
		bossWalkL[i] = loadImage(pathL);
		bossWalkR[i] = loadImage(pathR);
	}
	printf("  Boss walk frames loaded (%d each side)\n", BOSS_WALK_FRAMES);

	// ---- Boss attack sprites ----
	for (int i = 0; i < BOSS_ATTACK_FRAMES; i++) {
		char pathL[256], pathR[256];
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/boss_fight/atleft__%02d.png", i);
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/boss_fight/atright__%02d.png", i);
		bossAttackL[i] = loadImage(pathL);
		bossAttackR[i] = loadImage(pathR);
	}
	printf("  Boss attack frames loaded (%d each side)\n", BOSS_ATTACK_FRAMES);

	texturesLoaded = true;
	printf("=== BOSS 1 TEXTURES READY ===\n");
}

// CONSTRUCTOR
BossFightScene1::BossFightScene1() {
	// Clear all keys
	memset(keys, 0, sizeof(keys));

	// Initialize shared player controller
	// Also calls loadPlayerTextures() internally if not done yet
	player = new PlayerBossFightController();

	// Load boss PNG textures (no-op if already loaded)
	loadTextures();

	// Boss setup - use GameConfig constants
	// bossY: feet of character sit at sprite-Y=148, so offset up by 148
	bossX = 550.0f;
	bossY = FLOOR_Y - (float)BOSS_BODY_BOT_Y;
	bossHP = BOSS1_MAX_HP;
	bossMaxHP = BOSS1_MAX_HP;
	bossPhase = 1;
	bossFacingRight = false;
	bossState = BOSS_IDLE;
	bossAnimFrame = 0;
	bossAnimTimer = 0;

	// Boss AI - use GameConfig constants
	bossAttackTimer = BOSS1_ATTACK_COOLDOWN_PHASE1;
	bossAttackCooldown = BOSS1_ATTACK_COOLDOWN_PHASE1;

	// Victory state
	victoryTimer = 0;
	bossDefeated = false;

	// Start boss fight background music on the MCI channel (loops without interrupting SFX)
	playMusic(BOSS_FIGHT_MUSIC);
}

// DESTRUCTOR
BossFightScene1::~BossFightScene1() {
	delete player;
}

// UPDATE PLAYER
// Delegates all logic to the shared controller

void BossFightScene1::updatePlayer() {
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

	// Attack - check against the boss body centre, not the raw sprite-left.
	if (player->updateAttack()) {
		float playerCentreX = boss1PlayerCentreX(player);
		float bossCentreX = boss1BodyCentreX(bossX, bossFacingRight);
		float dist = std::fabs(playerCentreX - bossCentreX);

		if (dist < ATTACK_RANGE && !bossDefeated) {
			// Back attack means the player hit the boss from behind its body.
			bool isBackAttack = (playerCentreX < bossCentreX && bossFacingRight) ||
				(playerCentreX > bossCentreX && !bossFacingRight);

			int damage = player->getAttackDamage(isBackAttack);
			bossHP -= damage;
			if (bossHP <= 0) {
				bossHP = 0;
				bossDefeated = true;
				victoryTimer = 0;

				int hpScore = player->hp * 3;  // 3 points per HP remaining
				getGameState().totalScore += hpScore;
				setWorldBanner("CASTLE KNIGHT DEFEATED", 180);

				// Persist the boss-clear state immediately.
				GameState& data = getGameState();
				data.bossDefeated[0] = 1;
				saveActiveGame();
			}
		}
	}

	// Reset to idle if not doing anything
	player->resetToIdle();
}

// UPDATE BOSS
// Melee AI: tracks player and attacks in range.
// Attack zone is the full spear sweep:
void BossFightScene1::updateBoss() {
	if (bossDefeated) return;

	// The knight should be able to use the whole visible arena width.
	// Clamp only to the screen edges for the full 192px sprite.
	if (bossX < 0.0f) bossX = 0.0f;
	if (bossX > 608.0f) bossX = 608.0f;

	float playerCentreX = boss1PlayerCentreX(player);
	float bossCentreX = boss1BodyCentreX(bossX, bossFacingRight);
	float dxToPlayer = playerCentreX - bossCentreX;
	float absDist = std::fabs(dxToPlayer);

	// Reset to idle if not currently attacking.
	if (bossState != BOSS_ATTACK) {
		bossState = BOSS_IDLE;

		// Always face the player's actual body centre.
		bossFacingRight = (playerCentreX > bossCentreX);

		// Simple melee AI:
		if (absDist > 115.0f) {
			bossX += (dxToPlayer > 0.0f) ? 1.5f : -1.5f;
			bossState = BOSS_WALK;
		}

		if (bossX < 0.0f) bossX = 0.0f;
		if (bossX > 608.0f) bossX = 608.0f;

		bossCentreX = boss1BodyCentreX(bossX, bossFacingRight);
		dxToPlayer = playerCentreX - bossCentreX;
		absDist = std::fabs(dxToPlayer);
	}

	// Attack timer countdown
	if (bossAttackTimer > 0) {
		bossAttackTimer--;
	}

	// Trigger attack when the player is inside melee reach.
	if (bossState != BOSS_ATTACK &&
		bossAttackTimer == 0 &&
		absDist <= 125.0f) {

		bossState = BOSS_ATTACK;
		bossAnimFrame = 0;
		bossAnimTimer = 0;
		bossAttackTimer = bossAttackCooldown;

		float bodyCentreX = boss1BodyCentreX(bossX, bossFacingRight);
		float spearTipX = bossFacingRight
			? bossX + BOSS_SPRITE_W
			: bossX;

		bool inSpearZone = bossFacingRight
			? (playerCentreX >= bodyCentreX - 8.0f && playerCentreX <= spearTipX)
			: (playerCentreX <= bodyCentreX + 8.0f && playerCentreX >= spearTipX);

		if (inSpearZone && !player->isJumping) {
			player->takeDamage(BOSS1_DAMAGE);   // damage from GameConfig
		}
	}

	// Phase 2: faster attacks at 50% HP
	if (bossHP < bossMaxHP * 0.5f && bossPhase == 1) {
		bossPhase = 2;
		bossAttackCooldown = BOSS1_ATTACK_COOLDOWN_PHASE2;
	}
}


// UPDATE ANIMATIONS
// Advances boss sprite frame counter
void BossFightScene1::updateAnimations() {
	bossAnimTimer++;
	if (bossAnimTimer >= BOSS_ANIM_SPEED) {
		bossAnimTimer = 0;
		bossAnimFrame++;

		switch (bossState) {
		case BOSS_IDLE:
			bossAnimFrame = 0;
			break;

		case BOSS_WALK:
			if (bossAnimFrame >= BOSS_WALK_FRAMES) bossAnimFrame = 0;
			break;

		case BOSS_ATTACK:
			if (bossAnimFrame >= BOSS_ATTACK_FRAMES) {
				bossAnimFrame = 0;
				bossState = BOSS_IDLE;
			}
			break;
		}
	}
}

// CHECK COLLISIONS
// Victory / defeat scene transitions
void BossFightScene1::checkCollisions() {
	// Defeat: open the game-over / load-save flow.
	if (player->hp <= 0) {
		stopMusic();
		Engine::changeScene(new GameOverScene());
	}
}

// GET BOSS TEXTURE
// Returns the correct PNG texture ID for the
// current boss state, direction, and anim frame.
int BossFightScene1::getBossTexture() const {
	switch (bossState) {

	case BOSS_IDLE:
		// Use walk frame 0 as the idle standing pose
		return bossFacingRight ? bossWalkR[0] : bossWalkL[0];

	case BOSS_WALK:
		return bossFacingRight
			? bossWalkR[bossAnimFrame % BOSS_WALK_FRAMES]
			: bossWalkL[bossAnimFrame % BOSS_WALK_FRAMES];

	case BOSS_ATTACK:
		return bossFacingRight
			? bossAttackR[bossAnimFrame % BOSS_ATTACK_FRAMES]
			: bossAttackL[bossAnimFrame % BOSS_ATTACK_FRAMES];
	}

	return bossWalkR[0];   // safe fallback
}

// UPDATE - Main loop
void BossFightScene1::update() {

	// Victory countdown then transition into the post-boss cutscene.
	if (bossDefeated) {
		victoryTimer++;
		if (victoryTimer > 180) {
			stopMusic();
			Engine::changeScene(new BossCutscene1(BossCutscene1::POST_BOSS));
		}
		return;
	}

	updatePlayer();
	updateBoss();
	updateAnimations();
	checkCollisions();

	// Manual exit to overworld
	if (consumeKey('b') || consumeKey('B')) {
		stopMusic();
		Engine::changeScene(new PlayScene());
	}
}

// DRAW UI
// Health bars and controls hint
void BossFightScene1::drawUI() {

	// ---- Player HP bar ----
	drawTextC(11, 426, "Player HP:", 0, 0, 0);
	drawTextC(10, 425, "Player HP:", 255, 255, 255);

	setColor(100, 0, 0);
	fillRect(100, 425, 200, 20);

	int hpWidth = (int)(200.0f * (player->hp / (float)player->maxHP));
	setColor(0, 255, 0);
	fillRect(100, 425, hpWidth, 20);

	// ---- Boss name + HP bar ----
	// Keep the title on its own line so longer boss names do not sit on top
	// of the red health bar area.
	drawTextC(501, 441, "Castle Knight", 0, 0, 0);
	drawTextC(500, 440, "Castle Knight", 255, 255, 255);

	setColor(100, 0, 0);
	fillRect(580, 421, 200, 18);

	int bossHPWidth = (int)(200.0f * (bossHP / (float)bossMaxHP));
	setColor(255, 100, 0);
	fillRect(580, 421, bossHPWidth, 18);

	// New work: boss 1 now shows its phase count just like the later bosses
	// so the HUD stays consistent across the full campaign.
	char phaseText[32];
	sprintf_s(phaseText, sizeof(phaseText), "PHASE %d/2", bossPhase);
	drawTextC(581, 403, phaseText, 0, 0, 0);
	drawTextC(580, 402, phaseText, 255, 100, 100);

	// ---- Victory message ----
	if (bossDefeated) {
		drawTextC(251, 226, "BOSS DEFEATED!", 0, 0, 0);
		drawTextC(250, 225, "BOSS DEFEATED!", 255, 215, 0);
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
void BossFightScene1::draw() {

	// ---- Background PNG (or fallback solid colour) ----
	if (bgTexture >= 0) {
		drawImage(0, 0, 800, 450, bgTexture);
	}
	else {
		// Fallback: dark stone grey
		setColor(40, 35, 30);
		fillRect(0, 0, 800, 450);
	}

	// ---- Player - drawn via PlayerBossFightController texture cache ----
	// Adjust 64x64 to match your actual sprite sheet frame size.
	int playerTex = player->getPlayerTexture();
	if (playerTex >= 0) {
		drawImage((int)player->x,
			(int)(player->y + player->jumpY),
			64, 64,
			playerTex);
	}
	else {
		// Fallback: simple blue rectangle
		setColor(50, 100, 200);
		fillRect((int)player->x,
			(int)(player->y + player->jumpY),
			32, 64);
	}

	// ---- Boss - drawn via local PNG texture cache ----
	// Sprite display size: 192x192
	// Collision box:       40x90  (centred on character body)
	if (!bossDefeated) {
		int bossTex = getBossTexture();
		if (bossTex >= 0) {
			drawImage((int)bossX, (int)bossY, BOSS_SPRITE_W, BOSS_SPRITE_H, bossTex);
		}
		else {
			// Fallback: dark armour silhouette at hitbox position
			int hx = (int)bossX + (bossFacingRight ? BOSS_HIT_OX_R : BOSS_HIT_OX_L);
			int hy = (int)bossY + BOSS_HIT_OY;
			setColor(60, 55, 50);
			fillRect(hx, hy, BOSS_HIT_W, BOSS_HIT_H);
		}
	}

	// ---- HUD overlay ----
	drawUI();
}