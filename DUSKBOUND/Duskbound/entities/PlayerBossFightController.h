#ifndef PLAYERBOSSFIGHTCONTROLLER_H
#define PLAYERBOSSFIGHTCONTROLLER_H

#include "../core/GameConfig.h"

// PLAYER BOSS FIGHT CONTROLLER
// Shared player mechanics AND PNG animations
// for all boss fights.

class PlayerBossFightController {
public:

	// PLAYER STATE
	float x, y;          // World position
	int   hp, maxHP;     // Health
	bool  facingRight;   // Direction

	// Animation state
	enum State { IDLE, WALK, ATTACK, BLOCK, JUMP };
	State state;
	int   animFrame;
	int   animTimer;

	// Jump physics (Y-up coordinate system)
	float jumpY;          // Vertical offset from ground (positive = up)
	float jumpVelocity;   // Vertical velocity
	bool  isJumping;      // Currently in air

	// Combat state
	int  attackCooldown;  // Frames until can attack again
	bool isBlocking;      // Currently blocking

	// STATIC PNG TEXTURE CACHE
	// Loaded once via loadPlayerTextures(),
	// shared across ALL boss fight scenes.
	static int playerWalkR[5];     // kright1.png .. kright5.png
	static int playerWalkL[5];     // kleft1.png  .. kleft5.png

	static int playerAttackR[5];   // krattack1.png .. krattack5.png
	static int playerAttackL[5];   // klattack1.png .. klattack5.png

	static int playerBlockR[5];    // krblock1.png  .. krblock5.png
	static int playerBlockL[5];    // klblock1.png  .. klblock5.png

	static int playerJumpR[9];     // jumpright__00.png .. jumpright__08.png
	static int playerJumpL[9];     // jumpleft__00.png  .. jumpleft__08.png

	static bool texturesLoaded;    // Guard: load only once

	// TEXTURE MANAGEMENT

	// Load all player PNG textures.
	// Called automatically by the constructor - no need to call manually.
	static void loadPlayerTextures();

	// Returns the correct texture ID for the current state/direction/frame.
	// Returns walkR[0] for IDLE (no separate idle sprite needed).
	int getPlayerTexture() const;

	// CONSTRUCTOR
	PlayerBossFightController();

	// UPDATE METHODS (call in this order)
	void  updateBlocking();    // 1. Highest priority
	void  updateJump();        // 2. Jump input + physics
	float updateMovement();    // 3. Horizontal movement, returns dx
	bool  updateAttack();      // 4. Attack input, returns true if triggered
	void  updateAnimation();   // 5. Advance sprite frames
	void  updateCooldowns();   // 6. Decrement timers

	// HELPER METHODS
	bool  canAttack()  const;
	bool  canJump()    const;
	bool  canMove()    const;

	float getMoveSpeed()                          const;
	int   getAttackDamage(bool isBackAttack = false) const;
	void  takeDamage(int baseDamage);
	void  resetToIdle();
};

#endif