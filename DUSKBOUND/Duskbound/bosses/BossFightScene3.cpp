#include "BossFightScene3.h"
#include "BossCutscene3.h"
#include "../scenes/GameOverScene.h"
#include "../core/SaveSystem.h"
#include "../core/GameState.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/Utils.h"
#include "../core/Sound.h"
#include "../scenes/PlayScene.h"
#include "../core/GameConfig.h"
#include "../systems/Powerups.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Boss-fight background music
static const char* BOSS_FIGHT_MUSIC = "assets/audio/boss.wav";

// Boss 3 animation counts
// The executioner now uses mirrored left/right frame sets.
static const int BOSS_ANIM_SPEED          = 5;
static const int MINION_ANIM_SPEED        = 6;
static const int BOSS_IDLE_FRAMES         = 4;
static const int BOSS_ADVANCE_FRAMES      = 4;   // same loop as idle
static const int BOSS_CLEAVE_FRAMES       = 13;
static const int BOSS_CURSE_FRAMES        = 12;
static const int BOSS_SUMMON_FRAMES       = 5;
static const int BOSS_DEATH_FRAMES        = 18;
static const int MINION_APPEAR_FRAMES     = 6;
static const int MINION_IDLE_FRAMES       = 4;
static const int MINION_DEATH_FRAMES      = 5;

// Arena / gameplay tuning
static const int   BOSS_DRAW_W            = 200;
static const int   BOSS_DRAW_H            = 200;
static const int   BOSS_DRAW_Y_OFFSET     = -20;
static const int   MINION_DRAW_W          = 80;
static const int   MINION_DRAW_H          = 80;
static const int   MINION_DRAW_Y_OFFSET   = -8;

static const float BOSS_MIN_X             = 170.0f;
static const float BOSS_MAX_X             = 610.0f;
static const float BOSS_PREFERRED_GAP     = 145.0f;
static const float BOSS_ADVANCE_SPEED_P1  = 0.85f;
static const float BOSS_ADVANCE_SPEED_P4  = 1.60f;
static const float BOSS_CLEAVE_RANGE      = 155.0f;
static const float BOSS_CURSE_RANGE       = 260.0f;
static const float BOSS_FRONT_TOLERANCE   = 28.0f;
static const float MINION_CHASE_SPEED     = 1.25f;
static const float MINION_HIT_RANGE       = 46.0f;
static const float WAVE_HIT_RANGE         = 30.0f;

// Small helper functions for the final boss fight.
// Boss 3 and its minions both use framed sprites with
// empty margins, so combat should use visual centres
// instead of raw sprite-left coordinates.
static float boss3PlayerCentreX(const PlayerBossFightController* player) {
    return player->x + 32.0f;   // 64px player sprite -> centre
}

static float boss3PlayerCentreY(const PlayerBossFightController* player) {
    return player->y + player->jumpY + 32.0f;
}

static float boss3BodyCentreX(float bossX) {
    return bossX + BOSS_DRAW_W * 0.5f;
}

static float boss3MinionCentreX(float minionX) {
    return minionX + MINION_DRAW_W * 0.5f;
}

// Static texture storage
int  BossFightScene3::bossIdleL[4] = { -1, -1, -1, -1 };
int  BossFightScene3::bossIdleR[4] = { -1, -1, -1, -1 };
int  BossFightScene3::bossAttackL[13] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene3::bossAttackR[13] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene3::bossSkillL[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene3::bossSkillR[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene3::bossSummonL[5] = { -1, -1, -1, -1, -1 };
int  BossFightScene3::bossSummonR[5] = { -1, -1, -1, -1, -1 };
int  BossFightScene3::bossDeathL[18] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int  BossFightScene3::bossDeathR[18] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

int  BossFightScene3::minionAppear[6] = { -1, -1, -1, -1, -1, -1 };
int  BossFightScene3::minionIdle[4] = { -1, -1, -1, -1 };
int  BossFightScene3::minionDeath[5] = { -1, -1, -1, -1, -1 };

int  BossFightScene3::bgTexture = -1;
bool BossFightScene3::texturesLoaded = false;

// Constructor
BossFightScene3::BossFightScene3() {
    std::memset(keys, 0, sizeof(keys));

    player = new PlayerBossFightController();
    loadTextures();

    bossX = 560.0f;
    bossY = FLOOR_Y;
    bossHP = BOSS3_MAX_HP;
    bossMaxHP = BOSS3_MAX_HP;
    bossPhase = 1;
    bossFacingRight = false;

    bossState = BOSS_IDLE;
    bossAnimFrame = 0;
    bossAnimTimer = 0;
    actionTriggered = false;

    bossAttackTimer = BOSS3_ATTACK_COOLDOWN_PHASE1;
    bossAttackCooldown = BOSS3_ATTACK_COOLDOWN_PHASE1;
    bossMeleeCooldownTimer = 0;
    bossSummonTimer = 340;
    nextAttackType = 0;

    for (int i = 0; i < 6; ++i) {
        waves[i].active = false;
        waves[i].x = 0.0f;
        waves[i].y = 0.0f;
        waves[i].vx = 0.0f;
        waves[i].size = 0;
    }

    for (int i = 0; i < 3; ++i) {
        minions[i].active = false;
        minions[i].x = 0.0f;
        minions[i].y = FLOOR_Y;
        minions[i].hp = BOSS3_MINION_HP;
        minions[i].state = Minion::APPEAR;
        minions[i].animFrame = 0;
        minions[i].animTimer = 0;
        minions[i].damageCooldown = 0;
    }

    victoryTimer = 0;
    bossDefeated = false;

    // Start boss background music.
    playMusic(BOSS_FIGHT_MUSIC);
}

// Destructor
BossFightScene3::~BossFightScene3() {
    stopEffect("swordfx");
    delete player;
}

// loadTextures
void BossFightScene3::loadTextures() {
    if (texturesLoaded) return;

    printf("=== LOADING BOSS 3 EXECUTIONER PNG TEXTURES ===\n");

    bgTexture = loadImage("assets/images/boss_fight3/bg.png");
    printf("  bg.png : id=%d\n", bgTexture);

    for (int i = 0; i < BOSS_IDLE_FRAMES; ++i) {
        char pathL[256], pathR[256];
        sprintf_s(pathL, sizeof(pathL), "assets/images/boss_fight3/boss_idle_left_%02d.png", i);
        sprintf_s(pathR, sizeof(pathR), "assets/images/boss_fight3/boss_idle_right_%02d.png", i);
        bossIdleL[i] = loadImage(pathL);
        bossIdleR[i] = loadImage(pathR);
    }

    for (int i = 0; i < BOSS_CLEAVE_FRAMES; ++i) {
        char pathL[256], pathR[256];
        sprintf_s(pathL, sizeof(pathL), "assets/images/boss_fight3/boss_attack_left_%02d.png", i);
        sprintf_s(pathR, sizeof(pathR), "assets/images/boss_fight3/boss_attack_right_%02d.png", i);
        bossAttackL[i] = loadImage(pathL);
        bossAttackR[i] = loadImage(pathR);
    }

    for (int i = 0; i < BOSS_CURSE_FRAMES; ++i) {
        char pathL[256], pathR[256];
        sprintf_s(pathL, sizeof(pathL), "assets/images/boss_fight3/boss_skill_left_%02d.png", i);
        sprintf_s(pathR, sizeof(pathR), "assets/images/boss_fight3/boss_skill_right_%02d.png", i);
        bossSkillL[i] = loadImage(pathL);
        bossSkillR[i] = loadImage(pathR);
    }

    for (int i = 0; i < BOSS_SUMMON_FRAMES; ++i) {
        char pathL[256], pathR[256];
        sprintf_s(pathL, sizeof(pathL), "assets/images/boss_fight3/boss_summon_left_%02d.png", i);
        sprintf_s(pathR, sizeof(pathR), "assets/images/boss_fight3/boss_summon_right_%02d.png", i);
        bossSummonL[i] = loadImage(pathL);
        bossSummonR[i] = loadImage(pathR);
    }

    for (int i = 0; i < BOSS_DEATH_FRAMES; ++i) {
        char pathL[256], pathR[256];
        sprintf_s(pathL, sizeof(pathL), "assets/images/boss_fight3/boss_death_left_%02d.png", i);
        sprintf_s(pathR, sizeof(pathR), "assets/images/boss_fight3/boss_death_right_%02d.png", i);
        bossDeathL[i] = loadImage(pathL);
        bossDeathR[i] = loadImage(pathR);
    }

    for (int i = 0; i < MINION_APPEAR_FRAMES; ++i) {
        char path[256];
        sprintf_s(path, sizeof(path), "assets/images/boss_fight3/minion_appear_%02d.png", i);
        minionAppear[i] = loadImage(path);
    }

    for (int i = 0; i < MINION_IDLE_FRAMES; ++i) {
        char path[256];
        sprintf_s(path, sizeof(path), "assets/images/boss_fight3/minion_idle_%02d.png", i);
        minionIdle[i] = loadImage(path);
    }

    for (int i = 0; i < MINION_DEATH_FRAMES; ++i) {
        char path[256];
        sprintf_s(path, sizeof(path), "assets/images/boss_fight3/minion_death_%02d.png", i);
        minionDeath[i] = loadImage(path);
    }

    texturesLoaded = true;
}

// updatePlayer
// The player can now cross behind Boss 3 freely because the
// boss art supports both facings. No one-direction clamp here.

void BossFightScene3::updatePlayer() {
    player->updateBlocking();
    player->updateAnimation();
    player->updateCooldowns();

    if (player->isBlocking) {
        return;
    }

    player->updateJump();

    float dx = player->updateMovement();
    player->x += dx;

    // Push the player away from active minions.
    if (!player->isJumping) {
        for (int i = 0; i < 3; ++i) {
            if (!minions[i].active || minions[i].state == Minion::DYING) continue;
            if (std::fabs(player->x - minions[i].x) < MINION_COLLISION_RADIUS) {
                if (player->x < minions[i].x) player->x = minions[i].x - MINION_COLLISION_RADIUS;
                else player->x = minions[i].x + MINION_COLLISION_RADIUS;
            }
        }
    }

    // Sword attacks should prioritise summoned minions.
    // If a swing connects with a minion, do not also damage the boss on the same hit.
    if (player->updateAttack()) {
        float playerCentreX = boss3PlayerCentreX(player);
        bool hitMinion = false;

        for (int i = 0; i < 3; ++i) {
            if (!minions[i].active || minions[i].state == Minion::DYING) continue;

            float minionCentreX = boss3MinionCentreX(minions[i].x);
            if (std::fabs(playerCentreX - minionCentreX) < 70.0f) {
                minions[i].hp = 0;
                minions[i].state = Minion::DYING;
                minions[i].animFrame = 0;
                minions[i].animTimer = 0;
                hitMinion = true;
                break;   // one swing, one minion kill
            }
        }

        if (!hitMinion && !bossDefeated && bossState != BOSS_DEATH) {
            float bossCentreX = boss3BodyCentreX(bossX);

            if (std::fabs(playerCentreX - bossCentreX) < ATTACK_RANGE) {
                bool isBackAttack = (playerCentreX < bossCentreX && bossFacingRight) ||
                                    (playerCentreX > bossCentreX && !bossFacingRight);

                bossHP -= player->getAttackDamage(isBackAttack);
                if (bossHP < 0) bossHP = 0;
            }
        }
    }
}

// updateBoss
// Boss 3 uses three readable moves instead of the old slam:
//   - cleave at close range
//   - curse wave at medium range
//   - summon in later phases
void BossFightScene3::updateBoss() {
    if (bossDefeated) return;

    // Start the death animation the moment HP reaches zero.
    if (bossHP <= 0 && bossState != BOSS_DEATH) {
        bossState = BOSS_DEATH;
        bossAnimFrame = 0;
        bossAnimTimer = 0;
        actionTriggered = false;
        return;
    }

    // Wait until the death animation is fully shown before saving victory.
    if (bossState == BOSS_DEATH) {
        if (bossAnimFrame >= BOSS_DEATH_FRAMES - 1) {
            bossDefeated = true;
            victoryTimer = 0;

            int hpScore = player->hp * 5;
            getGameState().totalScore += hpScore;

            GameState& data = getGameState();
            data.bossDefeated[2] = 1;
            setWorldBanner("CORRUPTED KING DEFEATED", 180);
            saveActiveGame();
        }
        return;
    }

    float playerCentreX = boss3PlayerCentreX(player);
    float bossCentreX = boss3BodyCentreX(bossX);
    float dxToPlayer = playerCentreX - bossCentreX;
    float dist = std::fabs(dxToPlayer);

    // Let the boss face the player while idle or gliding forward.
    // Active attacks keep their current facing until the animation ends.
    if (bossState == BOSS_IDLE || bossState == BOSS_ADVANCE) {
        bossFacingRight = (playerCentreX > bossCentreX);
    }

    // Phase-specific pacing values keep the final boss dangerous while still
    // leaving readable recovery between moves. Later phases shorten the pause
    // but do not remove it entirely.
    int postActionPause = 28;
    int summonInterval = 340;
    if (bossPhase == 2) {
        postActionPause = 22;
        summonInterval = 300;
    } else if (bossPhase == 3) {
        postActionPause = 18;
        summonInterval = 260;
    } else if (bossPhase >= 4) {
        postActionPause = 14;
        summonInterval = 210;
    }

    // Phase progression keeps the final boss clearly escalating.
    // More HP plus wider thresholds makes each phase last long enough
    // for the player to actually notice the pattern change.
    if (bossHP < bossMaxHP * 0.78f && bossPhase == 1) {
        bossPhase = 2;
        bossAttackCooldown = BOSS3_ATTACK_COOLDOWN_PHASE2;
    }
    if (bossHP < bossMaxHP * 0.54f && bossPhase == 2) {
        bossPhase = 3;
        bossAttackCooldown = BOSS3_ATTACK_COOLDOWN_PHASE3;
        bossSummonTimer = 280;
    }
    if (bossHP < bossMaxHP * 0.30f && bossPhase == 3) {
        bossPhase = 4;
        bossAttackCooldown = BOSS3_ATTACK_COOLDOWN_PHASE4;
        bossSummonTimer = 210;
    }

    bossAttackTimer--;
    bossSummonTimer--;
    if (bossMeleeCooldownTimer > 0) bossMeleeCooldownTimer--;

    // Resolve multi-frame actions before choosing a new behaviour.
    if (bossState == BOSS_CLEAVE) {
        if (!actionTriggered && bossAnimFrame >= 6) {
            bool playerInFront = bossFacingRight
                ? (dxToPlayer >= -BOSS_FRONT_TOLERANCE && dxToPlayer <= 120.0f)
                : (dxToPlayer <=  BOSS_FRONT_TOLERANCE && dxToPlayer >= -120.0f);

            if (playerInFront && !player->isJumping) {
                player->takeDamage(BOSS3_DAMAGE + (bossPhase - 1) * 2);
            }
            actionTriggered = true;
        }
        if (bossAnimFrame >= BOSS_CLEAVE_FRAMES - 1) {
            bossState = BOSS_IDLE;
            bossAnimFrame = 0;
            bossAnimTimer = 0;
            actionTriggered = false;

            // Give the melee cleave its own extra recovery so Boss 3 does not
            // chain close-range swings too quickly compared with the other moves.
            int cleaveRecovery = postActionPause + 22;
            if (bossPhase >= 3) cleaveRecovery = postActionPause + 18;
            if (bossPhase >= 4) cleaveRecovery = postActionPause + 14;

            bossMeleeCooldownTimer = cleaveRecovery;
            bossAttackTimer = bossAttackCooldown + cleaveRecovery;
        }
        return;
    }

    if (bossState == BOSS_CURSE) {
        if (!actionTriggered && bossAnimFrame >= 5) {
            // Spawn one or two horizontal cursed waves at player-height.
            float dir = bossFacingRight ? 1.0f : -1.0f;
            float startX = bossCentreX + (bossFacingRight ? 28.0f : -46.0f);

            for (int n = 0; n < ((bossPhase >= 4) ? 2 : 1); ++n) {
                for (int i = 0; i < 6; ++i) {
                    if (!waves[i].active) {
                        waves[i].active = true;
                        waves[i].size = 18 + n * 4;
                        waves[i].x = startX + dir * (n * 14.0f);
                        waves[i].y = FLOOR_Y + 16.0f + n * 12.0f;
                        waves[i].vx = dir * (3.2f + n * 0.6f);
                        break;
                    }
                }
            }
            actionTriggered = true;
        }
        if (bossAnimFrame >= BOSS_CURSE_FRAMES - 1) {
            bossState = BOSS_IDLE;
            bossAnimFrame = 0;
            bossAnimTimer = 0;
            actionTriggered = false;

            // Give the melee cleave its own extra recovery so Boss 3 does not
            // chain close-range swings too quickly compared with the other moves.
            int cleaveRecovery = postActionPause + 22;
            if (bossPhase >= 3) cleaveRecovery = postActionPause + 18;
            if (bossPhase >= 4) cleaveRecovery = postActionPause + 14;

            bossMeleeCooldownTimer = cleaveRecovery;
            bossAttackTimer = bossAttackCooldown + cleaveRecovery;
        }
        return;
    }

    if (bossState == BOSS_SUMMON) {
        if (!actionTriggered && bossAnimFrame >= 2) {
            int summonsToCreate = (bossPhase >= 4) ? 2 : 1;
            float dir = bossFacingRight ? 1.0f : -1.0f;

            for (int s = 0; s < summonsToCreate; ++s) {
                for (int i = 0; i < 3; ++i) {
                    if (!minions[i].active) {
                        minions[i].active = true;
                        minions[i].x = clampf(bossCentreX + dir * (55.0f + s * 45.0f) - MINION_DRAW_W * 0.5f,
                                              0.0f, 800.0f - MINION_DRAW_W);
                        minions[i].y = FLOOR_Y;
                        minions[i].hp = BOSS3_MINION_HP;
                        minions[i].state = Minion::APPEAR;
                        minions[i].animFrame = 0;
                        minions[i].animTimer = 0;
                        minions[i].damageCooldown = 25;
                        break;
                    }
                }
            }
            actionTriggered = true;
            bossSummonTimer = summonInterval;
        }
        if (bossAnimFrame >= BOSS_SUMMON_FRAMES - 1) {
            bossState = BOSS_IDLE;
            bossAnimFrame = 0;
            bossAnimTimer = 0;
            actionTriggered = false;
            if (bossAttackTimer < postActionPause + 6) bossAttackTimer = postActionPause + 6;
        }
        return;
    }

    // Choose the next attack when the cooldown expires.
    if (bossAttackTimer <= 0) {
        bool hasFreeMinionSlot = false;
        for (int i = 0; i < 3; ++i) {
            if (!minions[i].active) {
                hasFreeMinionSlot = true;
                break;
            }
        }

        if (bossPhase >= 3 && hasFreeMinionSlot && bossSummonTimer <= 0) {
            bossState = BOSS_SUMMON;
        } else if (dist <= 120.0f && bossMeleeCooldownTimer <= 0 &&
                   (nextAttackType % 2 == 0 || bossPhase == 1)) {
            bossState = BOSS_CLEAVE;
        } else {
            bossState = BOSS_CURSE;
        }

        bossAnimFrame = 0;
        bossAnimTimer = 0;
        actionTriggered = false;
        bossAttackTimer = bossAttackCooldown;
        nextAttackType++;
        return;
    }

    // Slow forward pressure when the player backs away too much.
    if (dist > BOSS_PREFERRED_GAP) {
        bossState = BOSS_ADVANCE;
        float speed = (bossPhase < 4) ? BOSS_ADVANCE_SPEED_P1 + 0.2f * (bossPhase - 1)
                                      : BOSS_ADVANCE_SPEED_P4;

        if (dxToPlayer > 0.0f) bossX += speed;
        else bossX -= speed;

        bossX = clampf(bossX, BOSS_MIN_X, BOSS_MAX_X);
    } else {
        bossState = BOSS_IDLE;
    }
}

// updateMinions
// Summoned spirits appear, hover toward the player and then
// play a short death animation when defeated.
void BossFightScene3::updateMinions() {
    for (int i = 0; i < 3; ++i) {
        if (!minions[i].active) continue;

        if (minions[i].damageCooldown > 0)
            minions[i].damageCooldown--;

        if (minions[i].state == Minion::APPEAR) {
            if (minions[i].animFrame >= MINION_APPEAR_FRAMES - 1) {
                minions[i].state = Minion::IDLE;
                minions[i].animFrame = 0;
                minions[i].animTimer = 0;
            }
            continue;
        }

        if (minions[i].state == Minion::DYING) {
            if (minions[i].animFrame >= MINION_DEATH_FRAMES - 1) {
                minions[i].active = false;
            }
            continue;
        }

        // Idle minions drift toward the player and chip damage on contact.
        float playerCentreX = boss3PlayerCentreX(player);
        float minionCentreX = boss3MinionCentreX(minions[i].x);

        if (playerCentreX < minionCentreX) minions[i].x -= MINION_CHASE_SPEED;
        else minions[i].x += MINION_CHASE_SPEED;

        minions[i].x = clampf(minions[i].x, 0.0f, 800.0f - MINION_DRAW_W);
        minionCentreX = boss3MinionCentreX(minions[i].x);

        if (std::fabs(playerCentreX - minionCentreX) < 34.0f &&
            !player->isJumping && minions[i].damageCooldown == 0) {
            player->takeDamage(BOSS3_MINION_DAMAGE);
            minions[i].damageCooldown = 45;
        }
    }
}

// updateProjectiles
// Simple cursed waves fired from the skill animation.
void BossFightScene3::updateProjectiles() {
    for (int i = 0; i < 6; ++i) {
        if (!waves[i].active) continue;

        waves[i].x += waves[i].vx;

        float playerCentreX = boss3PlayerCentreX(player);
        float playerCentreY = boss3PlayerCentreY(player);
        float waveCentreX = waves[i].x + waves[i].size * 0.5f;
        float waveCentreY = waves[i].y + waves[i].size * 0.5f;

        if (std::fabs(playerCentreX - waveCentreX) < WAVE_HIT_RANGE &&
            std::fabs(playerCentreY - waveCentreY) < 28.0f) {
            player->takeDamage(BOSS3_DAMAGE - 4 + bossPhase);
            waves[i].active = false;
            continue;
        }

        if (waves[i].x < -100.0f || waves[i].x > 900.0f) {
            waves[i].active = false;
        }
    }
}

// updateAnimations
void BossFightScene3::updateAnimations() {
    // Boss animation
    bossAnimTimer++;
    if (bossAnimTimer >= BOSS_ANIM_SPEED) {
        bossAnimTimer = 0;
        bossAnimFrame++;

        switch (bossState) {
        case BOSS_IDLE:
            if (bossAnimFrame >= BOSS_IDLE_FRAMES) bossAnimFrame = 0;
            break;
        case BOSS_ADVANCE:
            if (bossAnimFrame >= BOSS_ADVANCE_FRAMES) bossAnimFrame = 0;
            break;
        case BOSS_CLEAVE:
            if (bossAnimFrame >= BOSS_CLEAVE_FRAMES) bossAnimFrame = BOSS_CLEAVE_FRAMES - 1;
            break;
        case BOSS_CURSE:
            if (bossAnimFrame >= BOSS_CURSE_FRAMES) bossAnimFrame = BOSS_CURSE_FRAMES - 1;
            break;
        case BOSS_SUMMON:
            if (bossAnimFrame >= BOSS_SUMMON_FRAMES) bossAnimFrame = BOSS_SUMMON_FRAMES - 1;
            break;
        case BOSS_DEATH:
            if (bossAnimFrame >= BOSS_DEATH_FRAMES) bossAnimFrame = BOSS_DEATH_FRAMES - 1;
            break;
        }
    }

    // Minion animation
    for (int i = 0; i < 3; ++i) {
        if (!minions[i].active) continue;

        minions[i].animTimer++;
        if (minions[i].animTimer < MINION_ANIM_SPEED) continue;

        minions[i].animTimer = 0;
        minions[i].animFrame++;

        if (minions[i].state == Minion::APPEAR) {
            if (minions[i].animFrame >= MINION_APPEAR_FRAMES)
                minions[i].animFrame = MINION_APPEAR_FRAMES - 1;
        } else if (minions[i].state == Minion::IDLE) {
            if (minions[i].animFrame >= MINION_IDLE_FRAMES)
                minions[i].animFrame = 0;
        } else if (minions[i].state == Minion::DYING) {
            if (minions[i].animFrame >= MINION_DEATH_FRAMES)
                minions[i].animFrame = MINION_DEATH_FRAMES - 1;
        }
    }
}

// checkCollisions
void BossFightScene3::checkCollisions() {
    if (player->hp <= 0) {
        stopMusic();
        stopEffect("swordfx");
        Engine::changeScene(new GameOverScene());
    }
}

// Texture selection helpers
int BossFightScene3::getBossTexture() const {
    switch (bossState) {
    case BOSS_IDLE:
        return bossFacingRight
            ? bossIdleR[bossAnimFrame % BOSS_IDLE_FRAMES]
            : bossIdleL[bossAnimFrame % BOSS_IDLE_FRAMES];

    case BOSS_ADVANCE:
        // Floating advance reuses the idle loop.
        return bossFacingRight
            ? bossIdleR[bossAnimFrame % BOSS_ADVANCE_FRAMES]
            : bossIdleL[bossAnimFrame % BOSS_ADVANCE_FRAMES];

    case BOSS_CLEAVE:
        return bossFacingRight
            ? bossAttackR[bossAnimFrame % BOSS_CLEAVE_FRAMES]
            : bossAttackL[bossAnimFrame % BOSS_CLEAVE_FRAMES];

    case BOSS_CURSE:
        return bossFacingRight
            ? bossSkillR[bossAnimFrame % BOSS_CURSE_FRAMES]
            : bossSkillL[bossAnimFrame % BOSS_CURSE_FRAMES];

    case BOSS_SUMMON:
        return bossFacingRight
            ? bossSummonR[bossAnimFrame % BOSS_SUMMON_FRAMES]
            : bossSummonL[bossAnimFrame % BOSS_SUMMON_FRAMES];

    case BOSS_DEATH:
        return bossFacingRight
            ? bossDeathR[bossAnimFrame % BOSS_DEATH_FRAMES]
            : bossDeathL[bossAnimFrame % BOSS_DEATH_FRAMES];
    }

    return bossFacingRight ? bossIdleR[0] : bossIdleL[0];
}

int BossFightScene3::getMinionTexture(const Minion& minion) const {
    if (minion.state == Minion::APPEAR)
        return minionAppear[minion.animFrame % MINION_APPEAR_FRAMES];
    if (minion.state == Minion::DYING)
        return minionDeath[minion.animFrame % MINION_DEATH_FRAMES];
    return minionIdle[minion.animFrame % MINION_IDLE_FRAMES];
}

// update
void BossFightScene3::update() {
    // After the death animation finishes, keep the victory text
    // visible for a short moment and then go to the post-boss cutscene.
    if (bossDefeated) {
        victoryTimer++;
        if (victoryTimer > 180) {
            stopMusic();
            stopEffect("swordfx");
            Engine::changeScene(new BossCutscene3(BossCutscene3::POST_BOSS));
        }
        return;
    }

    updatePlayer();
    updateBoss();
    updateMinions();
    updateProjectiles();
    updateAnimations();
    checkCollisions();

    if (consumeKey('b') || consumeKey('B')) {
        stopMusic();
        stopEffect("swordfx");
        Engine::changeScene(new PlayScene());
    }
}

// drawUI
void BossFightScene3::drawUI() {
    drawTextC(11, 426, "Player HP:", 0, 0, 0);
    drawTextC(10, 425, "Player HP:", 255, 255, 255);

    setColor(100, 0, 0);
    fillRect(100, 425, 200, 20);

    int playerBarWidth = (int)(200.0f * (player->hp / (float)player->maxHP));
    setColor(0, 200, 0);
    fillRect(100, 425, playerBarWidth, 20);

    // Keep the final boss label on its own line so the HUD layout matches
    // the other boss fights and stays readable with longer names.
    drawTextC(501, 441, "Corrupted King", 0, 0, 0);
    drawTextC(500, 440, "Corrupted King", 255, 255, 255);

    setColor(100, 0, 0);
    fillRect(580, 421, 200, 18);

    int bossBarWidth = (int)(200.0f * (bossHP / (float)bossMaxHP));
    setColor(150, 0, 150);
    fillRect(580, 421, bossBarWidth, 18);

    char phaseText[32];
    sprintf_s(phaseText, sizeof(phaseText), "PHASE %d/4", bossPhase);
    drawTextC(581, 403, phaseText, 0, 0, 0);
    drawTextC(580, 402, phaseText, 255, 50, 255);

    if (bossDefeated) {
        drawTextC(251, 226, "THE KING FALLS!", 0, 0, 0);
        drawTextC(250, 225, "THE KING FALLS!", 255, 215, 0);

        char scoreText[64];
        int hpBonus = player->hp * 5;
        sprintf_s(scoreText, sizeof(scoreText), "HP Bonus: +%d  Total: %d",
                  hpBonus, getGameState().totalScore);
        drawTextC(251, 201, scoreText, 0, 0, 0);
        drawTextC(250, 200, scoreText, 100, 255, 100);
    }

    drawTextC(11, 11, "A/D=Move  SPACE=Attack  X=Block  W=Jump  B=Leave", 0, 0, 0);
    drawTextC(10, 10, "A/D=Move  SPACE=Attack  X=Block  W=Jump  B=Leave", 200, 200, 200);
}

// draw
void BossFightScene3::draw() {
    if (bgTexture >= 0) {
        drawImage(0, 0, 800, 450, bgTexture);
    } else {
        setColor(20, 0, 40);
        fillRect(0, 0, 800, 450);
    }

    // Draw cursed wave projectiles.
    for (int i = 0; i < 6; ++i) {
        if (!waves[i].active) continue;
        setColor(180, 0, 220);
        fillRect((int)waves[i].x, (int)waves[i].y, waves[i].size, waves[i].size);
        setColor(255, 120, 255);
        fillRect((int)waves[i].x + 4, (int)waves[i].y + 4,
                 waves[i].size - 8, waves[i].size - 8);
    }

    // Player
    int playerTex = player->getPlayerTexture();
    if (playerTex >= 0) {
        drawImage((int)player->x, (int)(player->y + player->jumpY), 64, 64, playerTex);
    } else {
        int px = (int)player->x;
        int py = (int)(player->y + player->jumpY);
        setColor(50, 100, 200);
        fillRect(px + 16, py + 16, 32, 48);
        setColor(100, 150, 255);
        fillRect(px + 24, py + 8, 16, 16);
    }

    // Boss
    if (!bossDefeated || bossState == BOSS_DEATH) {
        int bossTex = getBossTexture();
        if (bossTex >= 0) {
            drawImage((int)bossX, (int)bossY + BOSS_DRAW_Y_OFFSET, BOSS_DRAW_W, BOSS_DRAW_H, bossTex);
        } else {
            setColor(80, 0, 120);
            fillRect((int)bossX + 70, (int)bossY + 40, 70, 120);
            setColor(255, 0, 0);
            fillRect((int)bossX + 90, (int)bossY + 120, 10, 10);
            fillRect((int)bossX + 115, (int)bossY + 120, 10, 10);
        }
    }

    // Minions
    for (int i = 0; i < 3; ++i) {
        if (!minions[i].active) continue;

        int tex = getMinionTexture(minions[i]);
        if (tex >= 0) {
            drawImage((int)minions[i].x, (int)minions[i].y + MINION_DRAW_Y_OFFSET,
                      MINION_DRAW_W, MINION_DRAW_H, tex);
        } else {
            setColor(100, 0, 150);
            fillRect((int)minions[i].x + 10, (int)minions[i].y + 10, 40, 40);
        }
    }

    drawUI();
}
