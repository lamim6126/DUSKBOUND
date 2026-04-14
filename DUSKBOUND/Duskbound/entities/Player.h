#ifndef PLAYER_H
#define PLAYER_H

#include "../core/Entity.h"

class Player : public Entity {
public:
    enum Dir { DOWN = 0, LEFT = 1, RIGHT = 2, UP = 3 };

    float speed;
    Dir dir;

    int animTick;
    int walkPhase;  // 0..3 => frame 0,1,0,2
    bool moving;

    // Overworld combat state.
    bool isAttacking;
    bool isBlocking;
    int  actionTick;
    int  actionFrame;

    // Overworld health state.
    int health;
    int maxHealth;
    int healthRegenDelayCounter;
    int healthRegenStepCounter;

    // Cached overworld PNG textures.
    // [direction][frame] where frame is 0,1,2.
    static bool spritesLoaded;
    static int sprites[4][3];

    // Cached overworld combat PNG textures.
    // [direction][frame] where frame is 0..4.
    static int attackSprites[4][5];
    static int blockSprites[4][5];

    Player();  // Constructor
    void update();  // Update player movement, overworld combat, and animation
    void draw();    // Draw player on the screen

    // Overworld health helpers for future creature combat.
    void takeDamage(int damage);
    int  getHealth() const;
    int  getMaxHealth() const;
    bool isRegenerating() const;

    // Overworld combat helpers for future enemy hit checks.
    bool isAttackHitFrame() const;
    bool isBlockingNow() const;
    Dir  getFacingDir() const;

private:
    // Loads all player PNG textures once.
    static void loadSprites();
};

#endif
