#include "Player.h"
#include "../core/Engine.h"
#include "../core/GameConfig.h" 
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/Sound.h"      
#include "../core/Utils.h"
#include "../scenes/PauseScene.h"
#include "../systems/CollisionMap.h" 
#include "../core/GameState.h"
#include <cstdio>
#include <string>

// ---- Footstep sound (WAV looped via PlaySoundA) ----
// Flag prevents restarting the sound every single frame while key is held.
static bool footstepPlaying = false;

static void startFootstep() {
    if (footstepPlaying) return;
    playSound("assets/audio/runing.wav", true); // true = loop
    footstepPlaying = true;
}

static void stopFootstep() {
    if (!footstepPlaying) return;
    stopSound();
    footstepPlaying = false;
}


// Static PNG texture cache for the overworld player.
bool Player::spritesLoaded = false;
int Player::sprites[4][3] = {
    {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}};
int Player::attackSprites[4][5] = {
    {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}};
int Player::blockSprites[4][5] = {
    {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}};

// loadSprites
// Loads the overworld player PNG frames once and keeps them in
// memory for the lifetime of the program.
void Player::loadSprites() {
  if (spritesLoaded)
    return;

  const char *directions[] = {"down", "left", "right", "up"};
  const char *attackNames[] = {"kdattack", "klattack", "krattack", "kuattack"};
  const char *blockNames[] = {"kdblock", "klblock", "krblock", "kublock"};

  for (int d = 0; d < 4; d++) {
    for (int f = 0; f < 3; f++) {
      char path[256];
      sprintf_s(path, sizeof(path), "assets/images/player/%s__0%d.png",
                directions[d], f);
      sprites[d][f] = loadImage(path);
    }

    // Load the new overworld combat PNG sequences.
    for (int f = 0; f < OVERWORLD_ATTACK_FRAMES; f++) {
      char attackPath[256];
      char blockPath[256];

      sprintf_s(attackPath, sizeof(attackPath),
                "assets/images/player/%s__0%d.png", attackNames[d], f);
      sprintf_s(blockPath, sizeof(blockPath),
                "assets/images/player/%s__0%d.png", blockNames[d], f);

      attackSprites[d][f] = loadImage(attackPath);
      blockSprites[d][f] = loadImage(blockPath);
    }
  }

  spritesLoaded = true;
}

// CONSTRUCTOR
// Sets up player at the centre of the world with default stats.
// PNG textures are cached once and then reused every frame.
Player::Player() {
  w = 32;
  h = 32;
  x = static_cast<float>(WORLD_W) / 2.0f;
  y = static_cast<float>(WORLD_H) / 2.0f;

  speed = 2.0f;
  dir = DOWN;

  animTick = 0;
  walkPhase = 0;
  moving = false;

  isAttacking = false;
  isBlocking = false;
  actionTick = 0;
  actionFrame = 0;

  int hpBonus = (getGameState().fruitQuestState >= 3) ? 20 : 0;
  health = PLAYER_MAX_HP + hpBonus;
  maxHealth = PLAYER_MAX_HP + hpBonus;
  healthRegenDelayCounter = 0;
  healthRegenStepCounter = 0;

  loadSprites();
}

// UPDATE
// Reads keyboard input, moves player with collision, animates
// sprite, updates overworld combat state, regenerates health,
// and checks for a pause request.
// Called every fixed-update frame (~60 fps).
void Player::update() {

  // ---- Overworld health regeneration ----
  // The timer only matters while wounded. After the delay expires,
  // health restores slowly until the bar is full again.
  if (health < maxHealth) {
    healthRegenDelayCounter++;
    if (healthRegenDelayCounter >= OVERWORLD_REGEN_DELAY_FRAMES) {
      healthRegenStepCounter++;
      if (healthRegenStepCounter >= OVERWORLD_REGEN_STEP_FRAMES) {
        healthRegenStepCounter = 0;
        health++;
        if (health > maxHealth)
          health = maxHealth;
      }
    }
  } else {
    healthRegenDelayCounter = 0;
    healthRegenStepCounter = 0;
  }

  // ---- Block input (highest priority) ----
  // Holding X locks the player into a defensive pose and plays
  // the directional block animation until the key is released.
  if (keys['x'] != 0 || keys['X'] != 0) {
    if (!isBlocking) {
      isBlocking = true;
      isAttacking = false;
      actionTick = 0;
      actionFrame = 0;
    }

    moving = false;
    animTick = 0;
    walkPhase = 0;
    stopFootstep();

    actionTick++;
    if (actionTick >= OVERWORLD_ACTION_ANIM_SPEED) {
      actionTick = 0;
      if (actionFrame < OVERWORLD_BLOCK_FRAMES - 1)
        actionFrame++;
    }

    if (consumeKey(27)) { // 27 = Escape key
      Engine::changeScene(new PauseScene());
    }
    return;
  }

  if (isBlocking) {
    isBlocking = false;
    actionTick = 0;
    actionFrame = 0;
  }

  // ---- Attack input ----
  // SPACE triggers a short 5-frame directional sword swing.
  if (!isAttacking && consumeKey(' ')) {
    isAttacking = true;
    actionTick = 0;
    actionFrame = 0;
    stopFootstep();
    playEffect("overworld_swing", "assets/audio/sword.wav");
  }

  if (isAttacking) {
    moving = false;
    animTick = 0;
    walkPhase = 0;

    actionTick++;
    if (actionTick >= OVERWORLD_ACTION_ANIM_SPEED) {
      actionTick = 0;
      actionFrame++;
      if (actionFrame >= OVERWORLD_ATTACK_FRAMES) {
        isAttacking = false;
        actionFrame = 0;
      }
    }

    if (consumeKey(27)) { // 27 = Escape key
      Engine::changeScene(new PauseScene());
    }
    return;
  }

  // ---- Build movement delta ----
  float dx = 0.0f, dy = 0.0f;

  // Explicit != 0 comparisons suppress int-to-bool warnings
  if (keys['a'] != 0)
    dx -= speed;
  if (keys['d'] != 0)
    dx += speed;
  if (keys['w'] != 0)
    dy += speed;
  if (keys['s'] != 0)
    dy -= speed;

  // Diagonal walking speed
  if (dx != 0.0f && dy != 0.0f) {
	  dx *= 0.7071f;  // 1 / sqrt(2)
	  dy *= 0.7071f;
  }

  moving = (dx != 0.0f || dy != 0.0f);

  // ---- Update facing direction ----
  // Priority: horizontal > vertical so left/right sprites take precedence
  if (dx < 0.0f)
    dir = LEFT;
  else if (dx > 0.0f)
    dir = RIGHT;
  else if (dy > 0.0f)
    dir = UP;
  else if (dy < 0.0f)
    dir = DOWN;

  // ---- Collision-aware position update ----
  // Each axis is tested independently so the player slides along walls
  // instead of stopping completely when a corner touches a solid pixel.
  if (canPlayerMoveTo(x + dx, y, w, h))
    x += dx;
  if (canPlayerMoveTo(x, y + dy, w, h))
    y += dy;

  // ---- Hard clamp to world boundaries ----
  // Keeps player inside the map even for chunks with no mask file.
  x = clampf(x, 0.0f, static_cast<float>(WORLD_W - w));
  y = clampf(y, 0.0f, static_cast<float>(WORLD_H - h));

  // ---- Walk animation ----
  if (moving) {
    animTick++;
    if (animTick % 6 == 0) {
      walkPhase = (walkPhase + 1) % 4; // cycles 0->1->2->3->0
    }
  } else {
    // Reset animation when standing still
    animTick = 0;
    walkPhase = 0;
  }

  // ---- Footstep sound: play while moving, stop when still ----
  if (moving && !footstepPlaying) {
    startFootstep();
  } else if (!moving && footstepPlaying) {
    stopFootstep();
  }

  // ---- Pause toggle ----
  if (consumeKey(27)) { // 27 = Escape key
    if (footstepPlaying) stopFootstep();
    Engine::changeScene(new PauseScene());
  }
}

// Picks the correct cached PNG frame based on direction and walk
// phase, then draws it without reloading the image every frame.
void Player::draw() {

  // Convert world position to screen position using the camera
  int sx = static_cast<int>(x - cam.x);
  int sy = static_cast<int>(y - cam.y);

  // ---- Skip drawing if completely off screen (early-out) ----
  if (sx + w < 0 || sx > SCREEN_WIDTH)
    return;
  if (sy + h < 0 || sy > SCREEN_HEIGHT)
    return;

  // ---- Pick animation frame from walk phase ----
  // walkPhase cycles 0-1-2-3; frames used are 0, 1, 0, 2
  int frame = 0;
  if (moving) {
    if (walkPhase == 1)
      frame = 1;
    else if (walkPhase == 3)
      frame = 2;
  }

  int texId = sprites[(int)dir][frame];

  // Overworld combat sprites override the regular walk/idle frame.
  if (isAttacking) {
    texId = attackSprites[(int)dir][actionFrame];
  } else if (isBlocking) {
    texId = blockSprites[(int)dir][actionFrame];
  }

  // Draw the cached PNG when available.
  // If the PNG is missing, fall back to the original BMP name
  // so the overworld player still renders during asset conversion.
  if (texId >= 0) {
    drawImage(sx, sy, w, h, texId);
  } else {
    const char *directions[] = {"down", "left", "right", "up"};
    std::string path = "assets/images/player/" + std::string(directions[dir]) +
                       "__0" + std::to_string(frame) + ".bmp";
    drawBMP2(sx, sy, path.c_str(), 0xFFFFFF);
  }
}

// Applies incoming overworld damage and restarts the health
// regeneration delay so recovery does not begin immediately.
void Player::takeDamage(int damage) {
  if (damage <= 0)
    return;

  health -= damage;
  if (health < 0)
    health = 0;

  // Restart the AC-style regeneration countdown after every hit.
  healthRegenDelayCounter = 0;
  healthRegenStepCounter = 0;

  // Releasing block on hit makes creature knockback / stun easier to add later.
  isBlocking = false;
  actionTick = 0;
  actionFrame = 0;
}

int Player::getHealth() const {
  return health;
}

int Player::getMaxHealth() const {
  return maxHealth;
}

bool Player::isRegenerating() const {
  return (health > 0 && health < maxHealth &&
          healthRegenDelayCounter >= OVERWORLD_REGEN_DELAY_FRAMES);
}

bool Player::isAttackHitFrame() const {
  return isAttacking && (actionFrame >= 2 && actionFrame <= 3);
}

bool Player::isBlockingNow() const {
  return isBlocking;
}

Player::Dir Player::getFacingDir() const {
  return dir;
}
