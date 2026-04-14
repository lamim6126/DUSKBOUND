#ifndef GAMECONFIG_H
#define GAMECONFIG_H

// DUSKBOUND - GAME CONFIGURATION
// Centralized constants for easy balancing

// ----- SCREEN SETTINGS -----
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 450;

// ----- CHUNK SETTINGS -----
const int CHUNK_SIZE = 1024;
const int CHUNK_COLS = 5;
const int CHUNK_ROWS = 5;

// ----- WORLD SIZE -----
const int WORLD_WIDTH = CHUNK_SIZE * CHUNK_COLS;   // 5120
const int WORLD_HEIGHT = CHUNK_SIZE * CHUNK_ROWS;  // 5120

// ----- BOSS LOCATIONS -----
// Checkpoint 1: Castle (bottom-right area)
const int CASTLE_X = 3100;
const int CASTLE_Y = 4500;
const int CASTLE_WIDTH = 200;
const int CASTLE_HEIGHT = 250;

// Checkpoint 2: The Sanctum (top-right area)
const int SANCTUM_X = 4346;
const int SANCTUM_Y = 518;
const int SANCTUM_WIDTH = 200;
const int SANCTUM_HEIGHT = 250;

// Checkpoint 3: The Throne Room (center-left area) - UPDATED!
const int THRONE_X = 1580;
const int THRONE_Y = 1420;
const int THRONE_WIDTH = 250;
const int THRONE_HEIGHT = 250;

// ----- PLAYER SETTINGS -----
const float PLAYER_SPEED = 2.0f;
const int PLAYER_SIZE = 32;


// ----- OVERWORLD PLAYER COMBAT / RECOVERY -----
// These values drive the free-roam attack/block feel and the
// delayed AC-style vitality recovery.
const int OVERWORLD_ATTACK_FRAMES = 5;
const int OVERWORLD_BLOCK_FRAMES = 5;
const int OVERWORLD_ACTION_ANIM_SPEED = 3;
const int OVERWORLD_REGEN_DELAY_FRAMES = 300;
const int OVERWORLD_REGEN_STEP_FRAMES = 14;

// ----- OVERWORLD UI TUNING -----
// Vitality panel + coordinate plate positions live here so the
// HUD can be moved without searching through PlayScene.cpp.
const int OVERWORLD_VITALITY_PANEL_X = 14;
const int OVERWORLD_VITALITY_PANEL_Y = 394;
const int OVERWORLD_VITALITY_PANEL_W = 236;
const int OVERWORLD_VITALITY_PANEL_H = 42;
const int OVERWORLD_COORD_PANEL_W = 122;
const int OVERWORLD_COORD_PANEL_H = 22;
const int OVERWORLD_COORD_PANEL_MARGIN = 10;

// ----- OVERWORLD GUARD TUNING -----
// Boss-area guard waves now read their core gameplay numbers
// from here so health / damage / leash changes stay centralized.
const int OVERWORLD_GUARD_COUNT = 6;
const int OVERWORLD_GUARD_HP = 20;
const int OVERWORLD_GUARD_CONTACT_DAMAGE = 6;
const int OVERWORLD_GUARD_CONTACT_COOLDOWN = 28;
const float OVERWORLD_GUARD_TRIGGER_RADIUS = 220.0f;
const float OVERWORLD_GUARD_DESPAWN_RADIUS = 460.0f;
const float OVERWORLD_GOBLIN_GUARD_SPEED = 1.08f;
const float OVERWORLD_CULTIST_GUARD_SPEED = 0.90f;
const float OVERWORLD_THRONE_GUARD_SPEED = 0.95f;
const int OVERWORLD_GOBLIN_GUARD_ANIM_SPEED = 5;
const int OVERWORLD_CULTIST_GUARD_ANIM_SPEED = 6;
const int OVERWORLD_THRONE_GUARD_ANIM_SPEED = 7;

// ----- SKELETON SEEKERS -----
// Buried ambushers scattered across the overworld. Locations are
// fully data-driven so you can move / add them from here.
struct SkeletonSeekerSpawnConfig {
    float x;
    float y;
};

const int SKELETON_SEEKER_COUNT = 12;
const int SKELETON_SEEKER_DRAW_W = 32;
const int SKELETON_SEEKER_DRAW_H = 32;
const int SKELETON_SEEKER_MAX_HP = 15;
const int SKELETON_SEEKER_CONTACT_DAMAGE = 8;
const int SKELETON_SEEKER_CONTACT_COOLDOWN = 30;
const int SKELETON_SEEKER_HIT_COOLDOWN = 12;
const float SKELETON_SEEKER_TRIGGER_RADIUS = 78.0f;
const float SKELETON_SEEKER_LEASH_RADIUS = 210.0f;
const float SKELETON_SEEKER_RESET_RADIUS = 360.0f;
const float SKELETON_SEEKER_SPEED = 0.94f;
const int SKELETON_SEEKER_EMERGE_ANIM_SPEED = 5;
const int SKELETON_SEEKER_WALK_ANIM_SPEED = 6;
const int SKELETON_SEEKER_ATTACK_ANIM_SPEED = 4;
const int SKELETON_SEEKER_HURT_ANIM_SPEED = 5;
const int SKELETON_SEEKER_DEATH_ANIM_SPEED = 6;
const int SKELETON_SEEKER_BURIED_TALK_RADIUS = 92;
const int SKELETON_SEEKER_ATTACK_CONTACT_RANGE = 24;

static const SkeletonSeekerSpawnConfig SKELETON_SEEKER_SPAWNS[SKELETON_SEEKER_COUNT] = {
    { 226.0f, 4352.0f },
    { 936.0f, 4458.0f },
    { 924.0f, 336.0f },
    { 1014.0f, 328.0f },
    { 944.0f, 414.0f },
    { 1048.0f, 402.0f },
    { 1716.0f, 4838.0f },
    { 1836.0f, 4902.0f },
    { 1260.0f, 3988.0f },
    { 1360.0f, 3980.0f },
    { 1276.0f, 4088.0f },
    { 1368.0f, 4102.0f }
};

// ----- OVERWORLD LOADING SCREEN -----
// Used by the staged PlayScene loader so the menu background
// can hold a simple loading panel while the map streams in.
const int OVERWORLD_LOADING_BAR_W = 260;
const int OVERWORLD_LOADING_BAR_H = 18;
const int OVERWORLD_LOADING_PANEL_W = 360;
const int OVERWORLD_LOADING_PANEL_H = 110;



// ----- OVERWORLD CREATURE SCORE -----
// Standard reward granted for defeating any free-roam creature,
// including boss-gate guards and rescue ambushers.
const int OVERWORLD_CREATURE_SCORE = 2;

// ----- OVERWORLD CREATURE RESPAWN -----
// Free-roam overworld enemies return exactly three minutes after
// being killed. Boss-gate guards and the distressed child rescue
// wave do not use this respawn timer.
const unsigned long OVERWORLD_CREATURE_RESPAWN_DELAY_MS = 180000UL;

// ----- DISTRESSED CHILD RESCUE EVENT -----
// Approaching the child triggers a one-time skeleton seeker rush.
struct OverworldSpawnPoint {
    float x;
    float y;
};

const float DISTRESSED_CHILD_EVENT_X = 3342.0f;
const float DISTRESSED_CHILD_EVENT_Y = 940.0f;
const float DISTRESSED_CHILD_EVENT_TRIGGER_RADIUS = 110.0f;
const int DISTRESSED_CHILD_SEEKER_COUNT = 4;

static const OverworldSpawnPoint DISTRESSED_CHILD_SEEKER_SPAWNS[DISTRESSED_CHILD_SEEKER_COUNT] = {
    { 3282.0f, 900.0f },
    { 3410.0f, 902.0f },
    { 3296.0f, 1004.0f },
    { 3394.0f, 1008.0f }
};

// ----- ADDITIONAL OVERWORLD CREATURES -----
// Spawn locations for the new roaming enemies live here so they
// can be repositioned without touching PlayScene.cpp.
const int OLDGUARDIAN_COUNT = 13;
const int OLDGUARDIAN_MAX_HP = 30;
const int OLDGUARDIAN_CONTACT_DAMAGE = 10;
const int OLDGUARDIAN_CONTACT_COOLDOWN = 34;
const float OLDGUARDIAN_TRIGGER_RADIUS = 180.0f;
const float OLDGUARDIAN_LEASH_RADIUS = 280.0f;
const float OLDGUARDIAN_RESET_RADIUS = 420.0f;
const float OLDGUARDIAN_SPEED = 0.72f;

static const OverworldSpawnPoint OLDGUARDIAN_SPAWNS[OLDGUARDIAN_COUNT] = {
    { 1180.0f, 3320.0f },
    { 2440.0f, 3560.0f },
    { 4460.0f, 2090.0f },
    { 3348.0f, 332.0f },
    { 3420.0f, 314.0f },
    { 3492.0f, 338.0f },
    { 3410.0f, 410.0f },
    { 188.0f, 3162.0f },
    { 264.0f, 3160.0f },
    { 208.0f, 3250.0f },
    { 284.0f, 3252.0f },
    { 1740.0f, 4788.0f },
    { 1810.0f, 4946.0f }
};

const int SKELETONLIGHTER_COUNT = 13;
const int SKELETONLIGHTER_MAX_HP = 20;
const int SKELETONLIGHTER_CONTACT_DAMAGE = 7;
const int SKELETONLIGHTER_CONTACT_COOLDOWN = 28;
const float SKELETONLIGHTER_TRIGGER_RADIUS = 165.0f;
const float SKELETONLIGHTER_LEASH_RADIUS = 250.0f;
const float SKELETONLIGHTER_RESET_RADIUS = 380.0f;
const float SKELETONLIGHTER_SPEED = 0.96f;

static const OverworldSpawnPoint SKELETONLIGHTER_SPAWNS[SKELETONLIGHTER_COUNT] = {
    { 820.0f, 2740.0f },
    { 1740.0f, 3520.0f },
    { 4080.0f, 880.0f },
    { 4396.0f, 1340.0f },
    { 4468.0f, 1328.0f },
    { 4408.0f, 1440.0f },
    { 4498.0f, 1452.0f },
    { 1260.0f, 4008.0f },
    { 1340.0f, 3992.0f },
    { 1282.0f, 4090.0f },
    { 1362.0f, 4104.0f },
    { 1714.0f, 4894.0f },
    { 1860.0f, 4840.0f }
};


// Horde ambush markers. These are used for one-time monologues
// before the player pushes into a packed creature pocket.
enum OverworldHordeFamily {
    HORDE_FAMILY_SKELETON = 0,
    HORDE_FAMILY_OLDGUARDIAN = 1,
    HORDE_FAMILY_SKELETONLIGHTER = 2,
    HORDE_FAMILY_MIXED = 3
};

struct OverworldHordeConfig {
    float x;
    float y;
    float triggerRadius;
    int family;
};

const int OVERWORLD_HORDE_COUNT = 6;
static const OverworldHordeConfig OVERWORLD_HORDES[OVERWORLD_HORDE_COUNT] = {
    { 4454.0f, 1382.0f, 170.0f, HORDE_FAMILY_SKELETONLIGHTER },
    { 3412.0f, 344.0f, 170.0f, HORDE_FAMILY_OLDGUARDIAN },
    { 984.0f, 344.0f, 170.0f, HORDE_FAMILY_SKELETON },
    { 230.0f, 3206.0f, 170.0f, HORDE_FAMILY_OLDGUARDIAN },
    { 1322.0f, 4048.0f, 175.0f, HORDE_FAMILY_SKELETONLIGHTER },
    { 1776.0f, 4870.0f, 220.0f, HORDE_FAMILY_MIXED }
};

// ----- MAP SETTINGS -----
const int MAP_TRANSPARENT_COLOR = 0xFF00FF;  // Magenta

// ----- BOSS FIGHT SETTINGS -----
// Floor Y position - shared across all boss fights
const float FLOOR_Y = 67.0f;

// Player base stats
const int PLAYER_MAX_HP = 100;
const int PLAYER_BASE_DAMAGE = 10;
const int PLAYER_BOOSTED_DAMAGE = 20;  // With damage powerup

// Movement speeds
const float PLAYER_NORMAL_SPEED = 3.0f;
const float PLAYER_BOOSTED_SPEED = 4.5f;  // With speed powerup

// Collision radii
const float BOSS_COLLISION_RADIUS = 80.0f;
const float MINION_COLLISION_RADIUS = 40.0f;
const float ATTACK_RANGE = 150.0f;

// Jump physics
const float JUMP_INITIAL_VELOCITY = 11.0f;  // Positive = upward (Y-up system)
const float GRAVITY = 0.5f;                 // Gravity pulls down

// Boss 1 - Castle Knight (Melee Fighter)
const int BOSS1_MAX_HP = 300;
const int BOSS1_DAMAGE = 15;
const int BOSS1_ATTACK_COOLDOWN_PHASE1 = 140;  // Frames between attacks
const int BOSS1_ATTACK_COOLDOWN_PHASE2 = 80;   // Phase 2 is faster

// Boss 2 - Sanctum Sorcerer (Ranged Fighter)
const int BOSS2_MAX_HP = 400;
const int BOSS2_DAMAGE = 15;
const int BOSS2_ATTACK_COOLDOWN_PHASE1 = 60;
const int BOSS2_ATTACK_COOLDOWN_PHASE2 = 50;
const int BOSS2_ATTACK_COOLDOWN_PHASE3 = 40;
const float BOSS2_PROJECTILE_SPEED = 2.5f;

// Boss 3 - Corrupted King (Final Boss - Hybrid)
const int BOSS3_MAX_HP = 450;
const int BOSS3_DAMAGE = 20;
const int BOSS3_MINION_HP = 50;
const int BOSS3_MINION_DAMAGE = 10;
const int BOSS3_ATTACK_COOLDOWN_PHASE1 = 120;
const int BOSS3_ATTACK_COOLDOWN_PHASE2 = 100;
const int BOSS3_ATTACK_COOLDOWN_PHASE3 = 85;
const int BOSS3_ATTACK_COOLDOWN_PHASE4 = 62;

#endif