#include "PlayerBossFightController.h"
#include "../core/Input.h"
#include "../core/Sound.h"
#include "../systems/Powerups.h"
#include "../core/Renderer.h"
#include "../core/GameState.h"
#include <cmath>
#include <cstdio>

// ANIMATION CONSTANTS
const int PLAYER_ANIM_SPEED = 4;
const int PLAYER_WALK_FRAMES = 5;
const int PLAYER_ATTACK_FRAMES = 5;
const int PLAYER_BLOCK_FRAMES = 5;
const int PLAYER_JUMP_FRAMES = 9;

// STATIC MEMBER DEFINITIONS
// All -1 until loadPlayerTextures() runs.
int  PlayerBossFightController::playerWalkR[5] = { -1, -1, -1, -1, -1 };
int  PlayerBossFightController::playerWalkL[5] = { -1, -1, -1, -1, -1 };

int  PlayerBossFightController::playerAttackR[5] = { -1, -1, -1, -1, -1 };
int  PlayerBossFightController::playerAttackL[5] = { -1, -1, -1, -1, -1 };

int  PlayerBossFightController::playerBlockR[5] = { -1, -1, -1, -1, -1 };
int  PlayerBossFightController::playerBlockL[5] = { -1, -1, -1, -1, -1 };

int  PlayerBossFightController::playerJumpR[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int  PlayerBossFightController::playerJumpL[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };

bool PlayerBossFightController::texturesLoaded = false;

// LOAD PLAYER TEXTURES
// Called once automatically from the constructor.
// All subsequent PlayerBossFightController instances
// (boss fight 1, 2, 3) reuse these textures.
void PlayerBossFightController::loadPlayerTextures() {
	if (texturesLoaded) return;   // already done

	printf("=== LOADING PLAYER BOSS FIGHT TEXTURES ===\n");

	// ---- Walk (also serves as idle) ----
	// kright1.png .. kright5.png
	// kleft1.png  .. kleft5.png
	for (int i = 0; i < PLAYER_WALK_FRAMES; i++) {
		char pathR[256], pathL[256];
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/player_fight/kright%d.png", i + 1);
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/player_fight/kleft%d.png", i + 1);
		playerWalkR[i] = loadImage(pathR);
		playerWalkL[i] = loadImage(pathL);
	}
	printf("  Walk frames loaded (%d each side) - frame 0 also used for idle\n",
		PLAYER_WALK_FRAMES);

	// ---- Attack ----
	// krattack1.png .. krattack5.png
	// klattack1.png .. klattack5.png
	for (int i = 0; i < PLAYER_ATTACK_FRAMES; i++) {
		char pathR[256], pathL[256];
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/player_fight/krattack%d.png", i + 1);
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/player_fight/klattack%d.png", i + 1);
		playerAttackR[i] = loadImage(pathR);
		playerAttackL[i] = loadImage(pathL);
	}
	printf("  Attack frames loaded (%d each side)\n", PLAYER_ATTACK_FRAMES);

	// ---- Block ----
	// krblock1.png .. krblock5.png
	// klblock1.png .. klblock5.png
	for (int i = 0; i < PLAYER_BLOCK_FRAMES; i++) {
		char pathR[256], pathL[256];
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/player_fight/krblock%d.png", i + 1);
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/player_fight/klblock%d.png", i + 1);
		playerBlockR[i] = loadImage(pathR);
		playerBlockL[i] = loadImage(pathL);
	}
	printf("  Block frames loaded (%d each side)\n", PLAYER_BLOCK_FRAMES);

	// ---- Jump ----
	// jumpright__00.png .. jumpright__08.png
	// jumpleft__00.png  .. jumpleft__08.png
	for (int i = 0; i < PLAYER_JUMP_FRAMES; i++) {
		char pathR[256], pathL[256];
		sprintf_s(pathR, sizeof(pathR),
			"assets/images/player_fight/jumpright__%02d.png", i);
		sprintf_s(pathL, sizeof(pathL),
			"assets/images/player_fight/jumpleft__%02d.png", i);
		playerJumpR[i] = loadImage(pathR);
		playerJumpL[i] = loadImage(pathL);
	}
	printf("  Jump frames loaded (%d each side)\n", PLAYER_JUMP_FRAMES);

	texturesLoaded = true;
	printf("=== PLAYER TEXTURES READY ===\n");
}

// GET PLAYER TEXTURE
// Returns the correct PNG texture ID for the
// current player state, direction, and anim frame.
int PlayerBossFightController::getPlayerTexture() const {

	switch (state) {

	case IDLE:
		// Reuse walk frame 0 as the standing pose
		return facingRight ? playerWalkR[0] : playerWalkL[0];

	case WALK:
		return facingRight
			? playerWalkR[animFrame % PLAYER_WALK_FRAMES]
			: playerWalkL[animFrame % PLAYER_WALK_FRAMES];

	case ATTACK:
		return facingRight
			? playerAttackR[animFrame % PLAYER_ATTACK_FRAMES]
			: playerAttackL[animFrame % PLAYER_ATTACK_FRAMES];

	case BLOCK:
		return facingRight
			? playerBlockR[animFrame % PLAYER_BLOCK_FRAMES]
			: playerBlockL[animFrame % PLAYER_BLOCK_FRAMES];

	case JUMP:
		return facingRight
			? playerJumpR[animFrame % PLAYER_JUMP_FRAMES]
			: playerJumpL[animFrame % PLAYER_JUMP_FRAMES];
	}

	// Safe fallback - should never reach here
	return playerWalkR[0];
}

// CONSTRUCTOR
// Calls loadPlayerTextures() if not done yet.
PlayerBossFightController::PlayerBossFightController() {

	// Load textures the first time any boss fight is entered
	if (!texturesLoaded) {
		loadPlayerTextures();
	}

	// Position - start on the left side of the arena
	x = 150.0f;
	y = FLOOR_Y;

	// Health — +20 bonus if Elara's fruit quest is complete
	int hpBonus = (getGameState().fruitQuestState >= 3) ? 20 : 0;
	hp = PLAYER_MAX_HP + hpBonus;
	maxHP = PLAYER_MAX_HP + hpBonus;

	// Direction + animation
	facingRight = true;
	state = IDLE;
	animFrame = 0;
	animTimer = 0;

	// Jump physics
	jumpY = 0.0f;
	jumpVelocity = 0.0f;
	isJumping = false;

	// Combat
	attackCooldown = 0;
	isBlocking = false;
}

// BLOCKING
// Highest priority - checked before movement/attack
void PlayerBossFightController::updateBlocking() {
	if (keys['x'] || keys['X']) {
		if (!isBlocking) {
			state = BLOCK;
			isBlocking = true;
			animFrame = 0;
		}
	}
	else {
		isBlocking = false;
	}
}

// JUMP PHYSICS
// Y-up system: positive jumpY = above ground
void PlayerBossFightController::updateJump() {

	// Jump input
	if (consumeKey('w') || consumeKey('W')) {
		if (canJump()) {
			state = JUMP;
			animFrame = 0;
			isJumping = true;
			jumpY = 0.0f;
			jumpVelocity = JUMP_INITIAL_VELOCITY;   // from GameConfig
		}
	}

	// Apply physics while airborne
	if (isJumping) {
		jumpY += jumpVelocity;
		jumpVelocity -= GRAVITY;            // gravity pulls down (Y-up)

		// Landed
		if (jumpY <= 0.0f) {
			jumpY = 0.0f;
			jumpVelocity = 0.0f;
			isJumping = false;

			if (state == JUMP) state = IDLE;
		}
	}
}

// MOVEMENT
// Returns dx (horizontal delta this frame)
float PlayerBossFightController::updateMovement() {
	float dx = 0.0f;

	if (canMove()) {
		float speed = getMoveSpeed();

		if (keys['a'] && x > 0.0f) {
			dx = -speed;
			facingRight = false;
			if (state == IDLE) state = WALK;
		}
		if (keys['d'] && x < 736.0f) {   // 800 - 64 (sprite width)
			dx = speed;
			facingRight = true;
			if (state == IDLE) state = WALK;
		}
	}

	// No movement key held - if we were walking, stop the walk animation
	if (dx == 0.0f && state == WALK) {
		state = IDLE;
		animFrame = 0;
	}

	return dx;
}

// ATTACK
// Returns true the frame an attack is triggered
bool PlayerBossFightController::updateAttack() {
	if (consumeKey(' ') && canAttack()) {
		state = ATTACK;
		animFrame = 0;
		attackCooldown = PLAYER_ATTACK_FRAMES * PLAYER_ANIM_SPEED;
		playEffect("swordfx", "assets/audio/sword.wav");   // One-shot SFX path avoids interrupting boss background music
		return true;
	}
	return false;
}

// ANIMATION
// Advances the frame counter for the current state
void PlayerBossFightController::updateAnimation() {
	animTimer++;
	if (animTimer >= PLAYER_ANIM_SPEED) {
		animTimer = 0;
		animFrame++;

		switch (state) {
		case IDLE:
			animFrame = 0;   // No idle animation - hold frame 0
			break;

		case WALK:
			if (animFrame >= PLAYER_WALK_FRAMES)   animFrame = 0;
			break;

		case ATTACK:
			// Hold last frame until cooldown resets state
			if (animFrame >= PLAYER_ATTACK_FRAMES) animFrame = PLAYER_ATTACK_FRAMES - 1;
			break;

		case BLOCK:
			if (animFrame >= PLAYER_BLOCK_FRAMES)  animFrame = PLAYER_BLOCK_FRAMES - 1;
			break;

		case JUMP:
			if (animFrame >= PLAYER_JUMP_FRAMES)   animFrame = PLAYER_JUMP_FRAMES - 1;
			break;
		}
	}
}

// COOLDOWNS
// Decrements attack cooldown and resets state
void PlayerBossFightController::updateCooldowns() {
	if (attackCooldown > 0) {
		attackCooldown--;
		if (attackCooldown == 0 && state == ATTACK) {
			state = IDLE;
			animFrame = 0;
		}
	}
}

// HELPERS
bool PlayerBossFightController::canAttack() const {
	return attackCooldown == 0 && !isJumping;
}

bool PlayerBossFightController::canJump() const {
	return !isJumping && state != ATTACK;
}

bool PlayerBossFightController::canMove() const {
	return state != ATTACK;   // Can move while jumping
}

float PlayerBossFightController::getMoveSpeed() const {
	return gPowerups.speedBoost ? PLAYER_BOOSTED_SPEED : PLAYER_NORMAL_SPEED;
}

int PlayerBossFightController::getAttackDamage(bool isBackAttack) const {
	int damage = gPowerups.damageBoost ? PLAYER_BOOSTED_DAMAGE : PLAYER_BASE_DAMAGE;
	if (isBackAttack) damage += 10;   // Backstab bonus
	return damage;
}

void PlayerBossFightController::takeDamage(int baseDamage) {
	int damage = baseDamage;
	if (gPowerups.defenseBoost) damage -= 5;    // Defense powerup reduction
	if (isBlocking)             damage -= 10;   // Block reduction
	if (damage > 0) {
		hp -= damage;
		if (hp < 0) hp = 0;
	}
}

void PlayerBossFightController::resetToIdle() {
	if (state != ATTACK && state != JUMP && state != WALK) {
		state = IDLE;
	}
}