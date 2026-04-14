#ifndef BOSSFIGHTSCENE3_H
#define BOSSFIGHTSCENE3_H

#include "../core/Scene.h"
#include "../entities/PlayerBossFightController.h"

// BOSS FIGHT SCENE 3 - CORRUPTED KING / EXECUTIONER
// Final boss built around three readable attack ideas:
//   1) heavy melee cleave
//   2) cursed ranged wave
//   3) undead minion summon

class BossFightScene3 : public Scene {
private:

    // PLAYER
    PlayerBossFightController* player;

    // BOSS CORE STATE
    float bossX, bossY;
    int   bossHP, bossMaxHP;
    int   bossPhase;
    bool  bossFacingRight;

    enum BossState {
        BOSS_IDLE,
        BOSS_ADVANCE,
        BOSS_CLEAVE,
        BOSS_CURSE,
        BOSS_SUMMON,
        BOSS_DEATH
    };

    BossState bossState;
    int  bossAnimFrame;
    int  bossAnimTimer;
    bool actionTriggered;

    // CURSED WAVE PROJECTILES
    struct DarkWave {
        float x, y;
        float vx;
        int   size;
        bool  active;
    };
    DarkWave waves[6];

    // SUMMONED MINIONS
    struct Minion {
        enum State { APPEAR, IDLE, DYING };

        float x, y;
        int   hp;
        bool  active;
        State state;
        int   animFrame;
        int   animTimer;
        int   damageCooldown;
    };
    Minion minions[3];

    // BOSS AI TIMERS
    int bossAttackTimer;
    int bossAttackCooldown;
    int bossMeleeCooldownTimer;
    int bossSummonTimer;
    int nextAttackType;

    // PNG TEXTURE CACHE
    // All boss assets are cut into individual files and loaded
    // once. See BOSS3_ASSET_GUIDE.txt for the expected names.
    //
    // Note:
    // The executioner floats, so advance movement reuses the idle
    // animation instead of requiring a separate walk sheet.
    static int bossIdleL[4];
    static int bossIdleR[4];
    static int bossAttackL[13];
    static int bossAttackR[13];
    static int bossSkillL[12];
    static int bossSkillR[12];
    static int bossSummonL[5];
    static int bossSummonR[5];
    static int bossDeathL[18];
    static int bossDeathR[18];

    static int minionAppear[6];
    static int minionIdle[4];
    static int minionDeath[5];

    static int bgTexture;
    static bool texturesLoaded;

    // UI / VICTORY FLOW
    int  victoryTimer;
    bool bossDefeated;

public:
    BossFightScene3();
    ~BossFightScene3();

    void update();
    void draw();

private:
    void loadTextures();

    void updatePlayer();
    void updateBoss();
    void updateMinions();
    void updateProjectiles();
    void updateAnimations();
    void checkCollisions();
    void drawUI();

    int getBossTexture() const;
    int getMinionTexture(const Minion& minion) const;
};

#endif
