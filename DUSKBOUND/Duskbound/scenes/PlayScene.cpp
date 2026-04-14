#include "PlayScene.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/Sound.h"
#include "../core/Utils.h"
#include "../core/GameConfig.h"
#include "../core/SaveSystem.h"
#include "../core/GameState.h"
#include "../systems/CollisionMap.h"  
#include "../entities/Player.h"
#include "../entities/NPC.h"
#include "../bosses/BossFightScene1.h"
#include "../bosses/BossFightScene2.h"
#include "../bosses/BossFightScene3.h"
#include "../bosses/BossCutscene1.h"
#include "../bosses/BossCutscene2.h"
#include "../bosses/BossCutscene3.h"
#include "GameOverScene.h"
#include "../systems/Powerups.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// POWERUP WORLD OBJECTS
// WorldPowerup describes a collectible icon sitting in world
// space. The actual progression flags live in gPowerups.
struct WorldPowerup {
	float x, y;    // World-space position
	int   type;    // 0 = speed, 1 = damage, 2 = defense
	bool  collected;
};

static WorldPowerup worldPowerups[3];
static const int POWERUP_UNLOCK_NPC[3] = { 3, 1, 2 };

// PNG texture IDs - index matches powerup type (0/1/2)
static int  powerupTextures[3] = { -1, -1, -1 };

// One-time init guards
static bool powerupsInitialized = false;
static bool powerupTexturesLoaded = false;

// FRUIT QUEST ITEMS (moonberries for Elara's quest)
struct FruitItem {
	float x, y;
	bool  collected;
};

static FruitItem fruitItems[3];
static bool fruitsInitialized = false;

static void initializeFruits() {
	// Three moonberries clustered near the old oak grove (west of village)
	fruitItems[0] = { 1580.0f, 3020.0f, false };
	fruitItems[1] = { 1620.0f, 3060.0f, false };
	fruitItems[2] = { 1545.0f, 3060.0f, false };
	fruitsInitialized = true;
}

// INSPECT POINTS
// Monologue nodes the player triggers by pressing E near a
// location. Uses the NPC dialogue system with hidePortrait=true.
static const int INSPECT_COUNT = 6;   
static NPC       inspectPoints[INSPECT_COUNT];
static int       activeInspectIdx = -1;
static int       inspectInputCooldown = 0;
static bool      inspectPointsBuilt = false;

// SHARED SCENE OBJECTS
// Static so they survive across update/draw calls but are
// invisible outside this translation unit.
static Player*     player = nullptr;
static NPCManager* npcManager = nullptr;

// CHUNK TEXTURE CACHE
// All 25 chunks (5x5 grid) are pre-loaded into OpenGL textures
// once after iGraphics starts.
static int  chunkTex[CHUNK_ROWS][CHUNK_COLS];  // -1 = not loaded
static bool chunksLoaded = false;
static int  minimapTex = -1;  // Full overview map texture (map.bmp)

// Quest scroll toggle state.
static bool questScrollVisible = false;

// WORLD LOAD / TRANSITION PRESENTATION
// The overworld now boots in small staged steps so the menu can
// hand off to a visible loading screen instead of freezing on a
// blank wait. Each update advances exactly one loading task.
enum WorldLoadStage {
	WORLD_LOAD_STAGE_SAVE_SLOT = 0,
	WORLD_LOAD_STAGE_CHUNKS,
	WORLD_LOAD_STAGE_COLLISION,
	WORLD_LOAD_STAGE_POWERUP_WORLD,
	WORLD_LOAD_STAGE_POWERUP_TEX,
	WORLD_LOAD_STAGE_GUARD_TEX,
	WORLD_LOAD_STAGE_SKELETON_TEX,
	WORLD_LOAD_STAGE_FRUITS,
	WORLD_LOAD_STAGE_SKELETON_INIT,
	WORLD_LOAD_STAGE_SYNC_WORLD,
	WORLD_LOAD_STAGE_INSPECTS,
	WORLD_LOAD_STAGE_DONE
};

static bool  worldLoadStarted = false;
static bool  worldLoadFinished = false;
static int   worldLoadStage = WORLD_LOAD_STAGE_SAVE_SLOT;
static int   worldLoadChunkCursor = 0;
static DWORD worldLoadStartTick = 0;
static DWORD worldLoadElapsedMs = 0;
static const int WORLD_LOAD_CHUNK_STEPS = CHUNK_ROWS * CHUNK_COLS + 1;
static const int WORLD_LOAD_TOTAL_STEPS = 36;


// Forward helper declaration
// distSq is defined later in this file but the overworld guard
// logic uses it earlier, so declare it here for clarity.
static inline float distSq(float ax, float ay, float bx, float by);

// Forward declarations for overworld bootstrap helpers
// The staged loading flow calls these before their full
// definitions appear later in the file.
static void initializeWorldPowerups();
static void loadPowerupTexturesIfNeeded();
static void syncLiveWorldFromSave();
static void buildAllInspectPoints();
static void loadAllChunks();

// awardOverworldKillScore
// Grants the shared overworld creature score bonus for any
// overworld enemy or boss-gate minion kill.
static inline void awardOverworldKillScore() {
	getGameState().totalScore += OVERWORLD_CREATURE_SCORE;
}

// OVERWORLD GUARD ENCOUNTERS (Boss 1 / 2 / 3 entrances)
// Each major boss location can raise a local guard wave before
// the player is allowed to enter the boss cutscene. The first
// two waves reuse the new boss-specific overworld creature art,
// while boss 3 keeps the throne-guard set already in use.
enum OverworldGuardStyle {
	GUARD_STYLE_GOBLIN = 0,
	GUARD_STYLE_CULTIST = 1,
	GUARD_STYLE_THRONE = 2
};

enum OverworldGuardEncounterId {
	GUARD_ENCOUNTER_CASTLE = 0,
	GUARD_ENCOUNTER_SANCTUM = 1,
	GUARD_ENCOUNTER_THRONE = 2,
	GUARD_ENCOUNTER_COUNT = 3
};

struct OverworldMinion {
	enum State { SPAWNING, CHASING, DYING };

	float x, y;
	float homeX, homeY;
	int   hp;
	int   style;
	int   facingDir;
	bool  active;
	State state;
	int   animFrame;
	int   animTimer;
	int   contactCooldown;
	int   hitCooldown;
	unsigned long respawnTick;
};

struct GuardEncounterState {
	OverworldMinion guards[6];
	bool active;
	bool cleared;
};

// Guard count / health / damage / trigger / fallback radius now
// come from GameConfig.h so encounter tuning stays centralized.
static const int OVERWORLD_GOBLIN_DRAW_W = 32;
static const int OVERWORLD_GOBLIN_DRAW_H = 32;
static const int OVERWORLD_CULTIST_DRAW_W = 32;
static const int OVERWORLD_CULTIST_DRAW_H = 32;
static const int OVERWORLD_THRONE_DRAW_W = 42;
static const int OVERWORLD_THRONE_DRAW_H = 42;

static const int OVERWORLD_GOBLIN_WALK_FRAMES = 6;
static const int OVERWORLD_GOBLIN_ATTACK_FRAMES = 3;
static const int OVERWORLD_GOBLIN_DEATH_FRAMES = 3;
static const int OVERWORLD_CULTIST_IDLE_FRAMES = 5;
static const int OVERWORLD_CULTIST_MOVE_FRAMES = 6;
static const int OVERWORLD_CULTIST_ATTACK_FRAMES = 5;
static const int OVERWORLD_CULTIST_HURT_FRAMES = 4;
static const int OVERWORLD_CULTIST_DEATH_FRAMES = 6;
static const int OVERWORLD_THRONE_APPEAR_FRAMES = 6;
static const int OVERWORLD_THRONE_IDLE_FRAMES = 4;
static const int OVERWORLD_THRONE_DEATH_FRAMES = 5;
static const int OVERWORLD_THRONE_SPAWN_ANIM_SPEED = 5;

static GuardEncounterState guardEncounters[GUARD_ENCOUNTER_COUNT];
static bool overworldCreatureTexturesLoaded = false;

static int overworldGoblinWalkTex[4][OVERWORLD_GOBLIN_WALK_FRAMES] = {
	{ -1, -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1 } };
static int overworldGoblinAttackTex[4][OVERWORLD_GOBLIN_ATTACK_FRAMES] = {
	{ -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 } };
static int overworldGoblinIdleTex = -1;
static int overworldGoblinHurtTex = -1;
static int overworldGoblinDeathTex[OVERWORLD_GOBLIN_DEATH_FRAMES] = { -1, -1, -1 };

static int overworldCultistIdleTex[OVERWORLD_CULTIST_IDLE_FRAMES] = { -1, -1, -1, -1, -1 };
static int overworldCultistMoveTex[OVERWORLD_CULTIST_MOVE_FRAMES] = { -1, -1, -1, -1, -1, -1 };
static int overworldCultistAttackTex[OVERWORLD_CULTIST_ATTACK_FRAMES] = { -1, -1, -1, -1, -1 };
static int overworldCultistHurtTex[OVERWORLD_CULTIST_HURT_FRAMES] = { -1, -1, -1, -1 };
static int overworldCultistDeathTex[OVERWORLD_CULTIST_DEATH_FRAMES] = { -1, -1, -1, -1, -1, -1 };

static int overworldMinionAppearTex[OVERWORLD_THRONE_APPEAR_FRAMES] = { -1, -1, -1, -1, -1, -1 };
static int overworldMinionIdleTex[OVERWORLD_THRONE_IDLE_FRAMES] = { -1, -1, -1, -1 };
static int overworldMinionDeathTex[OVERWORLD_THRONE_DEATH_FRAMES] = { -1, -1, -1, -1, -1 };


// SKELETON SEEKERS
// Skeleton seekers are buried ambushers scattered across
// the map, while the roaming families below cover the heavier
// free-roam enemies. Positions / health / ranges live in GameConfig.
struct SkeletonSeeker {
	enum State { BURIED, INTRO_HOLD, EMERGING, CHASING, DYING, DEAD };

	float x, y;
	float homeX, homeY;
	int   hp;
	State state;
	int   animFrame;
	int   animTimer;
	int   contactCooldown;
	int   hitCooldown;
	// Respawn timer is only used by free-roam seekers.
	// Child-rescue seekers and scripted boss-related cases keep this at 0.
	unsigned long respawnTick;
};

static SkeletonSeeker skeletonSeekers[SKELETON_SEEKER_COUNT];
static bool skeletonSeekersInitialized = false;
static bool skeletonSeekerTexturesLoaded = false;
static int skeletonSeekerBuriedTex = -1;
static int skeletonSeekerEmergeTex[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static int skeletonSeekerIdleTex[6] = { -1, -1, -1, -1, -1, -1 };
static int skeletonSeekerWalkTex[6] = { -1, -1, -1, -1, -1, -1 };
static int skeletonSeekerAttackTex[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static int skeletonSeekerHurtTex[4] = { -1, -1, -1, -1 };
static int skeletonSeekerDeathTex[5] = { -1, -1, -1, -1, -1 };

// DISTRESSED CHILD RESCUE + ADDITIONAL ROAMERS
// The child rescue wave uses skeleton seeker visuals, while the
// Old Guardian and Skeleton Lighter roam fixed map pockets.
// Their spawn data is centralized in GameConfig.h.
struct DistressedChildRescueState {
	SkeletonSeeker seekers[DISTRESSED_CHILD_SEEKER_COUNT];
	bool triggered;
	bool active;
	bool cleared;
};

enum AdditionalCreatureFamily {
	ADDITIONAL_FAMILY_OLDGUARDIAN = 0,
	ADDITIONAL_FAMILY_SKELETONLIGHTER = 1
};

struct AdditionalOverworldCreature {
	enum State { IDLE, CHASING, DYING, DEAD };

	float x, y;
	float homeX, homeY;
	int   hp;
	int   family;
	State state;
	int   animFrame;
	int   animTimer;
	int   contactCooldown;
	int   hitCooldown;
	int   attackVariant;
	unsigned long respawnTick;
};

static DistressedChildRescueState distressedChildRescue = {};
static bool additionalCreaturesInitialized = false;

static AdditionalOverworldCreature oldGuardians[OLDGUARDIAN_COUNT];
static AdditionalOverworldCreature skeletonLighters[SKELETONLIGHTER_COUNT];

static bool additionalOverworldCreatureTexturesLoaded = false;

static int oldGuardianIdleTex[6] = { -1, -1, -1, -1, -1, -1 };
static int oldGuardianWalkTex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int oldGuardianAttackPrimaryTex[11] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static int oldGuardianAttackSecondaryTex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int oldGuardianHurtTex[4] = { -1, -1, -1, -1 };
static int oldGuardianDeathTex[11] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

static int skeletonLighterIdleTex[4] = { -1, -1, -1, -1 };
static int skeletonLighterWalkTex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int skeletonLighterAttackPrimaryTex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int skeletonLighterAttackSecondaryTex[11] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static int skeletonLighterHurtTex[4] = { -1, -1, -1, -1 };
static int skeletonLighterDeathTex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

static NPC  skeletonIntroDialogue;
static bool skeletonIntroDialogueBuilt = false;
static int  pendingSkeletonIntroIdx = -1;
static int  skeletonIntroInputCooldown = 0;

// Generic overworld monologue channel used for horde warnings,
// first-time creature sightings, and the distressed child rush.
static NPC  overworldMonologueDialogue;
static int  overworldMonologueInputCooldown = 0;
static int  overworldMonologuePendingAction = 0;
static int  overworldMonologuePendingValue = -1;

static void buildSkeletonIntroDialogue();
static void beginSkeletonIntroDialogue(int seekerIdx);
static void finishSkeletonIntroDialogue();
static bool updateSkeletonIntroDialogue();
static void activateDistressedChildRescueWave();
static void beginOverworldMonologue(const char* line1, const char* line2, const char* line3,
	int pendingAction = 0, int pendingValue = -1);
static void finishOverworldMonologue();
static bool updateOverworldMonologue();
static bool isHordeStillAlive(int hordeIdx);
static void awakenHordeEncounter(int hordeIdx);
static void triggerHordesInRange();
static bool checkOverworldHordeMonologues();
static void loadSkeletonSeekerTexturesIfNeeded();
static void initializeSkeletonSeekers();
static bool isSkeletonInsidePlayerStrike(const SkeletonSeeker& seeker);
static bool updateSkeletonSeekers();
static int  getSkeletonSeekerTexture(const SkeletonSeeker& seeker);
static void drawSkeletonSeekers();

// Forward declarations for the distressed child rescue event and
// extra roaming overworld creature families.
static bool checkDistressedChildRescueTrigger();
static bool updateDistressedChildRescue();
static void drawDistressedChildRescue();
static void initializeDistressedChildRescue();

static void initializeAdditionalOverworldCreatures();
static bool updateAdditionalOverworldCreatures();
static void drawAdditionalOverworldCreatures();

// Forward declarations for staged overworld loading.
static void beginWorldLoadIfNeeded();
static bool updateWorldLoadStep();
static int  getWorldLoadCompletedStepCount();
static void drawWorldLoadingScreen();
static void syncWorldFromSaveIfNeeded();

// getEncounterCenterX / getEncounterCenterY
// Returns the fixed overworld location for each boss entrance.
static float getEncounterCenterX(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE) return (float)CASTLE_X;
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return (float)SANCTUM_X;
	return (float)THRONE_X;
}

static float getEncounterCenterY(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE) return (float)CASTLE_Y;
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return (float)SANCTUM_Y;
	return (float)THRONE_Y;
}

// getEncounterStyle
// Maps each boss entrance to its overworld creature family.
static int getEncounterStyle(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE) return GUARD_STYLE_GOBLIN;
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return GUARD_STYLE_CULTIST;
	return GUARD_STYLE_THRONE;
}

// isEncounterUnlocked
// Boss 1 guards only spawn after the Village Elder reveals the
// castle. Boss 2 and 3 guards unlock after the prior boss falls.
static bool isEncounterUnlocked(int encounterId, const GameState& data) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE) {
		return (gPowerups.bossLocationsRevealed && !data.bossDefeated[0]);
	}
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) {
		return (data.bossDefeated[0] != 0 && !data.bossDefeated[1]);
	}
	return (data.bossDefeated[1] != 0 && !data.bossDefeated[2]);
}

// getEncounterSpawnBanner / getEncounterFallbackBanner
// Short messages for guard-wave spawn and fallback events.
static const char* getEncounterSpawnBanner(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE)  return "GOBLIN GUARDS RUSH OUT!";
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return "SANCTUM ACOLYTES DESCEND!";
	return "THRONE GUARDS EMERGE!";
}

static const char* getEncounterFallbackBanner(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE)  return "The goblin guards fall back.";
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return "The sanctum acolytes fall back.";
	return "The throne guards fall back.";
}

static const char* getEncounterClearBanner(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE)  return "The castle path is clear.";
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return "The sanctum path is clear.";
	return "The throne path is clear.";
}

static const char* getEncounterBlockMessage(int encounterId) {
	if (encounterId == GUARD_ENCOUNTER_CASTLE)  return "Defeat the goblin guards first.";
	if (encounterId == GUARD_ENCOUNTER_SANCTUM) return "Defeat the sanctum acolytes first.";
	return "Defeat the throne guards first.";
}

// getGuardDrawW / getGuardDrawH / getGuardSpeed
// Per-style rendering and movement tuning.
static int getGuardDrawW(int style) {
	if (style == GUARD_STYLE_GOBLIN) return OVERWORLD_GOBLIN_DRAW_W;
	if (style == GUARD_STYLE_CULTIST) return OVERWORLD_CULTIST_DRAW_W;
	return OVERWORLD_THRONE_DRAW_W;
}

static int getGuardDrawH(int style) {
	if (style == GUARD_STYLE_GOBLIN) return OVERWORLD_GOBLIN_DRAW_H;
	if (style == GUARD_STYLE_CULTIST) return OVERWORLD_CULTIST_DRAW_H;
	return OVERWORLD_THRONE_DRAW_H;
}

static float getGuardSpeed(int style) {
	if (style == GUARD_STYLE_GOBLIN) return OVERWORLD_GOBLIN_GUARD_SPEED;
	if (style == GUARD_STYLE_CULTIST) return OVERWORLD_CULTIST_GUARD_SPEED;
	return OVERWORLD_THRONE_GUARD_SPEED;
}

static int getGuardChaseAnimSpeed(int style) {
	if (style == GUARD_STYLE_GOBLIN) return OVERWORLD_GOBLIN_GUARD_ANIM_SPEED;
	if (style == GUARD_STYLE_CULTIST) return OVERWORLD_CULTIST_GUARD_ANIM_SPEED;
	return OVERWORLD_THRONE_GUARD_ANIM_SPEED;
}

// loadOverworldCreatureTexturesIfNeeded
// Caches the three overworld guard families once.
static void loadOverworldCreatureTexturesIfNeeded() {
	if (overworldCreatureTexturesLoaded) return;

	for (int d = 0; d < 4; d++) {
		const char* dirName = (d == 0) ? "down" : (d == 1) ? "left" : (d == 2) ? "right" : "up";

		for (int i = 0; i < OVERWORLD_GOBLIN_WALK_FRAMES; i++) {
			char path[256];
			sprintf_s(path, sizeof(path),
				"assets/images/overworld_creatures/b1goblin_walk_%s__%02d.png",
				dirName, i);
			overworldGoblinWalkTex[d][i] = loadImage(path);
		}

		for (int i = 0; i < OVERWORLD_GOBLIN_ATTACK_FRAMES; i++) {
			char path[256];
			sprintf_s(path, sizeof(path),
				"assets/images/overworld_creatures/b1goblin_attack_%s__%02d.png",
				dirName, i);
			overworldGoblinAttackTex[d][i] = loadImage(path);
		}
	}

	overworldGoblinIdleTex = loadImage("assets/images/overworld_creatures/b1goblin_idle__00.png");
	overworldGoblinHurtTex = loadImage("assets/images/overworld_creatures/b1goblin_hurt__00.png");
	for (int i = 0; i < OVERWORLD_GOBLIN_DEATH_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/b1goblin_death__%02d.png", i);
		overworldGoblinDeathTex[i] = loadImage(path);
	}

	for (int i = 0; i < OVERWORLD_CULTIST_IDLE_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/b2cultist_idle__%02d.png", i);
		overworldCultistIdleTex[i] = loadImage(path);
	}
	for (int i = 0; i < OVERWORLD_CULTIST_MOVE_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/b2cultist_move__%02d.png", i);
		overworldCultistMoveTex[i] = loadImage(path);
	}
	for (int i = 0; i < OVERWORLD_CULTIST_ATTACK_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/b2cultist_attack__%02d.png", i);
		overworldCultistAttackTex[i] = loadImage(path);
	}
	for (int i = 0; i < OVERWORLD_CULTIST_HURT_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/b2cultist_hurt__%02d.png", i);
		overworldCultistHurtTex[i] = loadImage(path);
	}
	for (int i = 0; i < OVERWORLD_CULTIST_DEATH_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/b2cultist_death__%02d.png", i);
		overworldCultistDeathTex[i] = loadImage(path);
	}

	for (int i = 0; i < OVERWORLD_THRONE_APPEAR_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/minion_appear_%02d.png", i);
		overworldMinionAppearTex[i] = loadImage(path);
	}

	for (int i = 0; i < OVERWORLD_THRONE_IDLE_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/minion_idle_%02d.png", i);
		overworldMinionIdleTex[i] = loadImage(path);
	}

	for (int i = 0; i < OVERWORLD_THRONE_DEATH_FRAMES; i++) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/minion_death_%02d.png", i);
		overworldMinionDeathTex[i] = loadImage(path);
	}

	for (int i = 0; i < 6; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/oldguardian_idle__%02d.png", i);
		oldGuardianIdleTex[i] = loadImage(path);
	}
	for (int i = 0; i < 8; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/oldguardian_walk__%02d.png", i);
		oldGuardianWalkTex[i] = loadImage(path);
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/oldguardian_attack_secondary__%02d.png", i);
		oldGuardianAttackSecondaryTex[i] = loadImage(path);
	}
	for (int i = 0; i < 11; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/oldguardian_attack_primary__%02d.png", i);
		oldGuardianAttackPrimaryTex[i] = loadImage(path);
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/oldguardian_death__%02d.png", i);
		oldGuardianDeathTex[i] = loadImage(path);
	}
	for (int i = 0; i < 4; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/oldguardian_hurt__%02d.png", i);
		oldGuardianHurtTex[i] = loadImage(path);
	}

	for (int i = 0; i < 4; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonlighter_idle__%02d.png", i);
		skeletonLighterIdleTex[i] = loadImage(path);
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonlighter_hurt__%02d.png", i);
		skeletonLighterHurtTex[i] = loadImage(path);
	}
	for (int i = 0; i < 8; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonlighter_walk__%02d.png", i);
		skeletonLighterWalkTex[i] = loadImage(path);
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonlighter_attack_primary__%02d.png", i);
		skeletonLighterAttackPrimaryTex[i] = loadImage(path);
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonlighter_death__%02d.png", i);
		skeletonLighterDeathTex[i] = loadImage(path);
	}
	for (int i = 0; i < 11; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonlighter_attack_secondary__%02d.png", i);
		skeletonLighterAttackSecondaryTex[i] = loadImage(path);
	}

	overworldCreatureTexturesLoaded = true;
}

// resetGuardEncounter / resetAllGuardEncounters
// Clears live overworld guard waves. When keepClearedState is
// false the encounter can spawn again on a later approach.
static void resetGuardEncounter(int encounterId, bool keepClearedState) {
	GuardEncounterState& encounter = guardEncounters[encounterId];

	for (int i = 0; i < OVERWORLD_GUARD_COUNT; i++) {
		encounter.guards[i].x = 0.0f;
		encounter.guards[i].y = 0.0f;
		encounter.guards[i].homeX = 0.0f;
		encounter.guards[i].homeY = 0.0f;
		encounter.guards[i].hp = OVERWORLD_GUARD_HP;
		encounter.guards[i].style = getEncounterStyle(encounterId);
		encounter.guards[i].facingDir = Player::DOWN;
		encounter.guards[i].active = false;
		encounter.guards[i].state = OverworldMinion::SPAWNING;
		encounter.guards[i].animFrame = 0;
		encounter.guards[i].animTimer = 0;
		encounter.guards[i].contactCooldown = 0;
		encounter.guards[i].hitCooldown = 0;
	}

	encounter.active = false;
	if (!keepClearedState) {
		encounter.cleared = false;
	}
}

static void resetAllGuardEncounters(bool keepClearedState) {
	for (int encounterId = 0; encounterId < GUARD_ENCOUNTER_COUNT; encounterId++) {
		resetGuardEncounter(encounterId, keepClearedState);
	}
}

// countActiveGuards
// Returns how many guard instances still exist on screen. Guards
// in their death animation still count until they fully vanish.
static int countActiveGuards(int encounterId) {
	int count = 0;
	const GuardEncounterState& encounter = guardEncounters[encounterId];
	for (int i = 0; i < OVERWORLD_GUARD_COUNT; i++) {
		if (encounter.guards[i].active) count++;
	}
	return count;
}

// spawnGuardEncounter
// Spawns one boss-location overworld guard wave around its
// entrance using fixed offsets for a readable formation.
static void spawnGuardEncounter(int encounterId) {
	static const float spawnOffsets[GUARD_ENCOUNTER_COUNT][OVERWORLD_GUARD_COUNT][2] = {
		{   // Castle goblin guards
			{ -90.0f, -36.0f },
			{ -54.0f, -82.0f },
			{ -6.0f, -96.0f },
			{ 48.0f, -78.0f },
			{ 92.0f, -28.0f },
			{ 10.0f, 34.0f }
		},
		{   // Sanctum cultist guards
			{ -84.0f, -42.0f },
			{ -42.0f, -88.0f },
			{ 12.0f, -96.0f },
			{ 68.0f, -58.0f },
			{ 86.0f, 10.0f },
			{ -18.0f, 34.0f }
		},
		{   // Throne guards
			{ -88.0f, -58.0f },
			{ -34.0f, -84.0f },
			{ 28.0f, -84.0f },
			{ 84.0f, -46.0f },
			{ 58.0f, 26.0f },
			{ -60.0f, 22.0f }
		}
	};

	GuardEncounterState& encounter = guardEncounters[encounterId];
	const int style = getEncounterStyle(encounterId);
	const float centerX = getEncounterCenterX(encounterId);
	const float centerY = getEncounterCenterY(encounterId);

	loadOverworldCreatureTexturesIfNeeded();

	for (int i = 0; i < OVERWORLD_GUARD_COUNT; i++) {
		encounter.guards[i].homeX = centerX + spawnOffsets[encounterId][i][0];
		encounter.guards[i].homeY = centerY + spawnOffsets[encounterId][i][1];
		encounter.guards[i].x = encounter.guards[i].homeX;
		encounter.guards[i].y = encounter.guards[i].homeY;
		encounter.guards[i].hp = OVERWORLD_GUARD_HP;
		encounter.guards[i].style = style;
		encounter.guards[i].facingDir = Player::DOWN;
		encounter.guards[i].active = true;
		encounter.guards[i].state = (style == GUARD_STYLE_THRONE)
			? OverworldMinion::SPAWNING
			: OverworldMinion::CHASING;
		encounter.guards[i].animFrame = 0;
		encounter.guards[i].animTimer = 0;
		encounter.guards[i].contactCooldown = 0;
		encounter.guards[i].hitCooldown = 0;
	}

	encounter.active = true;
	encounter.cleared = false;
	setWorldBanner(getEncounterSpawnBanner(encounterId), 90);
}

// isMinionInsidePlayerStrike
// Directional overworld hit check that matches the player's
// facing direction and keeps creature combat readable.
static bool isMinionInsidePlayerStrike(const OverworldMinion& minion) {
	if (!player) return false;

	const int minionW = getGuardDrawW(minion.style);
	const int minionH = getGuardDrawH(minion.style);

	float playerCx = player->x + player->w * 0.5f;
	float playerCy = player->y + player->h * 0.5f;
	float minionCx = minion.x + minionW * 0.5f;
	float minionCy = minion.y + minionH * 0.5f;
	float dx = minionCx - playerCx;
	float dy = minionCy - playerCy;

	switch (player->getFacingDir()) {
	case Player::DOWN:
		return std::fabs(dx) < 30.0f && dy < 10.0f && dy > -58.0f;
	case Player::UP:
		return std::fabs(dx) < 30.0f && dy > -10.0f && dy < 58.0f;
	case Player::LEFT:
		return std::fabs(dy) < 28.0f && dx < 10.0f && dx > -58.0f;
	case Player::RIGHT:
		return std::fabs(dy) < 28.0f && dx > -10.0f && dx < 58.0f;
	}

	return false;
}

static bool isWorldRectInsidePlayerStrike(float worldX, float worldY, int drawW, int drawH) {
	if (!player) return false;

	float playerCx = player->x + player->w * 0.5f;
	float playerCy = player->y + player->h * 0.5f;
	float targetCx = worldX + drawW * 0.5f;
	float targetCy = worldY + drawH * 0.5f;
	float dx = targetCx - playerCx;
	float dy = targetCy - playerCy;

	switch (player->getFacingDir()) {
	case Player::DOWN:
		return std::fabs(dx) < 30.0f && dy < 10.0f && dy > -58.0f;
	case Player::UP:
		return std::fabs(dx) < 30.0f && dy > -10.0f && dy < 58.0f;
	case Player::LEFT:
		return std::fabs(dy) < 28.0f && dx < 10.0f && dx > -58.0f;
	case Player::RIGHT:
		return std::fabs(dy) < 28.0f && dx > -10.0f && dx < 58.0f;
	}
	return false;
}


// updateSingleGuardEncounter
// Handles one overworld wave: spawn, chase, player hits,
// contact damage, despawn on escape, and clear-state tracking.
// Returns true if the scene changed (for example, on death).
static bool updateSingleGuardEncounter(int encounterId) {
	if (!player) return false;

	GuardEncounterState& encounter = guardEncounters[encounterId];
	const GameState& data = getGameState();

	if (!isEncounterUnlocked(encounterId, data)) {
		resetGuardEncounter(encounterId, false);
		return false;
	}

	const float centerX = getEncounterCenterX(encounterId);
	const float centerY = getEncounterCenterY(encounterId);
	const float playerToCenterSq = distSq(player->x, player->y, centerX, centerY);

	if (!encounter.active && !encounter.cleared &&
		playerToCenterSq < OVERWORLD_GUARD_TRIGGER_RADIUS * OVERWORLD_GUARD_TRIGGER_RADIUS) {
		spawnGuardEncounter(encounterId);
	}

	if (!encounter.active) return false;

	if (playerToCenterSq > OVERWORLD_GUARD_DESPAWN_RADIUS * OVERWORLD_GUARD_DESPAWN_RADIUS) {
		resetGuardEncounter(encounterId, false);
		setWorldBanner(getEncounterFallbackBanner(encounterId), 90);
		return false;
	}

	const int playerDamage = gPowerups.damageBoost ? PLAYER_BOOSTED_DAMAGE : PLAYER_BASE_DAMAGE;

	for (int i = 0; i < OVERWORLD_GUARD_COUNT; i++) {
		OverworldMinion& minion = encounter.guards[i];
		if (!minion.active) continue;

		if (minion.contactCooldown > 0) minion.contactCooldown--;
		if (minion.hitCooldown > 0) minion.hitCooldown--;

		minion.animTimer++;

		if (minion.state == OverworldMinion::SPAWNING) {
			if (minion.animTimer >= OVERWORLD_THRONE_SPAWN_ANIM_SPEED) {
				minion.animTimer = 0;
				minion.animFrame++;
				if (minion.animFrame >= OVERWORLD_THRONE_APPEAR_FRAMES) {
					minion.state = OverworldMinion::CHASING;
					minion.animFrame = 0;
					minion.animTimer = 0;
				}
			}
			continue;
		}

		if (minion.state == OverworldMinion::DYING) {
			int deathFrames = (minion.style == GUARD_STYLE_GOBLIN) ? OVERWORLD_GOBLIN_DEATH_FRAMES :
				(minion.style == GUARD_STYLE_CULTIST) ? OVERWORLD_CULTIST_DEATH_FRAMES :
				OVERWORLD_THRONE_DEATH_FRAMES;
			if (minion.animTimer >= OVERWORLD_THRONE_SPAWN_ANIM_SPEED) {
				minion.animTimer = 0;
				minion.animFrame++;
				if (minion.animFrame >= deathFrames) {
					minion.active = false;
					awardOverworldKillScore();
				}
			}
			continue;
		}

		if (player->isAttackHitFrame() && minion.hitCooldown == 0 &&
			isMinionInsidePlayerStrike(minion)) {
			minion.hp -= playerDamage;
			minion.hitCooldown = 12;

			if (minion.hp <= 0) {
				minion.state = OverworldMinion::DYING;
				minion.animFrame = 0;
				minion.animTimer = 0;
				minion.contactCooldown = 0;
			}
			continue;
		}

		const int chaseAnimSpeed = getGuardChaseAnimSpeed(minion.style);
		if (minion.animTimer >= chaseAnimSpeed) {
			minion.animTimer = 0;

			if (minion.style == GUARD_STYLE_GOBLIN) {
				minion.animFrame = (minion.animFrame + 1) % OVERWORLD_GOBLIN_WALK_FRAMES;
			}
			else if (minion.style == GUARD_STYLE_CULTIST) {
				minion.animFrame = (minion.animFrame + 1) % OVERWORLD_CULTIST_MOVE_FRAMES;
			}
			else {
				minion.animFrame = (minion.animFrame + 1) % OVERWORLD_THRONE_IDLE_FRAMES;
			}
		}

		float dx = player->x - minion.x;
		float dy = player->y - minion.y;
		float len = std::sqrt(dx * dx + dy * dy);
		if (len > 0.001f) {
			const float speed = getGuardSpeed(minion.style);
			minion.x += (dx / len) * speed;
			minion.y += (dy / len) * speed;
		}

		if (minion.style == GUARD_STYLE_GOBLIN) {
			if (std::fabs(dx) > std::fabs(dy)) {
				minion.facingDir = (dx < 0.0f) ? Player::LEFT : Player::RIGHT;
			}
			else {
				minion.facingDir = (dy < 0.0f) ? Player::DOWN : Player::UP;
			}
		}

		minion.x = clampf(minion.x, 0.0f, (float)(WORLD_W - getGuardDrawW(minion.style)));
		minion.y = clampf(minion.y, 0.0f, (float)(WORLD_H - getGuardDrawH(minion.style)));

		float playerCx = player->x + player->w * 0.5f;
		float playerCy = player->y + player->h * 0.5f;
		float minionCx = minion.x + getGuardDrawW(minion.style) * 0.5f;
		float minionCy = minion.y + getGuardDrawH(minion.style) * 0.5f;

		if (std::fabs(playerCx - minionCx) < 24.0f &&
			std::fabs(playerCy - minionCy) < 24.0f &&
			minion.contactCooldown == 0) {
			minion.contactCooldown = OVERWORLD_GUARD_CONTACT_COOLDOWN;

			if (!player->isBlockingNow()) {
				player->takeDamage(OVERWORLD_GUARD_CONTACT_DAMAGE);
				if (player->getHealth() <= 0) {
					Engine::changeScene(new GameOverScene());
					return true;
				}
			}
		}
	}

	if (countActiveGuards(encounterId) == 0) {
		encounter.active = false;
		encounter.cleared = true;
		setWorldBanner(getEncounterClearBanner(encounterId), 90);
	}

	return false;
}

// updateOverworldGuardEncounters
// Runs all three boss-area guard waves after the player update.
static bool updateOverworldGuardEncounters() {
	for (int encounterId = 0; encounterId < GUARD_ENCOUNTER_COUNT; encounterId++) {
		if (updateSingleGuardEncounter(encounterId)) {
			return true;
		}
	}
	return false;
}

// getGuardTexture
// Chooses the right cached creature frame for the current state.
static int getGuardTexture(const OverworldMinion& minion) {
	if (minion.style == GUARD_STYLE_GOBLIN) {
		if (minion.state == OverworldMinion::DYING) {
			return overworldGoblinDeathTex[minion.animFrame % OVERWORLD_GOBLIN_DEATH_FRAMES];
		}

		if (minion.hitCooldown > 6 && overworldGoblinHurtTex >= 0) {
			return overworldGoblinHurtTex;
		}

		if (minion.contactCooldown > OVERWORLD_GUARD_CONTACT_COOLDOWN / 2) {
			int attackFrame = (OVERWORLD_GUARD_CONTACT_COOLDOWN - minion.contactCooldown) / 4;
			if (attackFrame < 0) attackFrame = 0;
			if (attackFrame >= OVERWORLD_GOBLIN_ATTACK_FRAMES) attackFrame = OVERWORLD_GOBLIN_ATTACK_FRAMES - 1;
			return overworldGoblinAttackTex[minion.facingDir][attackFrame];
		}

		int texId = overworldGoblinWalkTex[minion.facingDir][minion.animFrame % OVERWORLD_GOBLIN_WALK_FRAMES];
		if (texId >= 0) return texId;
		return overworldGoblinIdleTex;
	}

	if (minion.style == GUARD_STYLE_CULTIST) {
		if (minion.state == OverworldMinion::DYING) {
			return overworldCultistDeathTex[minion.animFrame % OVERWORLD_CULTIST_DEATH_FRAMES];
		}

		if (minion.hitCooldown > 6 && overworldCultistHurtTex[0] >= 0) {
			return overworldCultistHurtTex[minion.animFrame % OVERWORLD_CULTIST_HURT_FRAMES];
		}

		if (minion.contactCooldown > OVERWORLD_GUARD_CONTACT_COOLDOWN / 2) {
			int attackFrame = (OVERWORLD_GUARD_CONTACT_COOLDOWN - minion.contactCooldown) / 3;
			if (attackFrame < 0) attackFrame = 0;
			if (attackFrame >= OVERWORLD_CULTIST_ATTACK_FRAMES) attackFrame = OVERWORLD_CULTIST_ATTACK_FRAMES - 1;
			return overworldCultistAttackTex[attackFrame];
		}

		return overworldCultistMoveTex[minion.animFrame % OVERWORLD_CULTIST_MOVE_FRAMES];
	}

	if (minion.state == OverworldMinion::SPAWNING) {
		return overworldMinionAppearTex[minion.animFrame % OVERWORLD_THRONE_APPEAR_FRAMES];
	}
	if (minion.state == OverworldMinion::DYING) {
		return overworldMinionDeathTex[minion.animFrame % OVERWORLD_THRONE_DEATH_FRAMES];
	}
	return overworldMinionIdleTex[minion.animFrame % OVERWORLD_THRONE_IDLE_FRAMES];
}

// drawOverworldGuards
// Draws every live overworld guard wave in world space.
static void drawOverworldGuards() {
	for (int encounterId = 0; encounterId < GUARD_ENCOUNTER_COUNT; encounterId++) {
		const GuardEncounterState& encounter = guardEncounters[encounterId];

		for (int i = 0; i < OVERWORLD_GUARD_COUNT; i++) {
			const OverworldMinion& minion = encounter.guards[i];
			if (!minion.active) continue;

			const int drawW = getGuardDrawW(minion.style);
			const int drawH = getGuardDrawH(minion.style);
			int sx = (int)(minion.x - cam.x);
			int sy = (int)(minion.y - cam.y);

			if (sx + drawW < 0 || sx > SCREEN_WIDTH) continue;
			if (sy + drawH < 0 || sy > SCREEN_HEIGHT) continue;

			int texId = getGuardTexture(minion);
			if (texId >= 0) {
				drawImage(sx, sy, drawW, drawH, texId);
			}
			else {
				if (minion.style == GUARD_STYLE_GOBLIN) setColor(40, 120, 30);
				else if (minion.style == GUARD_STYLE_CULTIST) setColor(110, 70, 120);
				else setColor(130, 20, 30);
				fillRect(sx, sy, drawW - 6, drawH - 6);
			}
		}
	}
}


static void loadSkeletonSeekerTexturesIfNeeded() {
	if (skeletonSeekerTexturesLoaded) return;

	skeletonSeekerBuriedTex = loadImage("assets/images/overworld_creatures/skeletonseeker_buried__00.png");

	for (int i = 0; i < 10; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonseeker_emerge__%02d.png", i);
		skeletonSeekerEmergeTex[i] = loadImage(path);
	}

	for (int i = 0; i < 6; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonseeker_idle__%02d.png", i);
		skeletonSeekerIdleTex[i] = loadImage(path);
	}

	for (int i = 0; i < 6; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonseeker_walk__%02d.png", i);
		skeletonSeekerWalkTex[i] = loadImage(path);
	}

	for (int i = 0; i < 10; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonseeker_attack__%02d.png", i);
		skeletonSeekerAttackTex[i] = loadImage(path);
	}

	for (int i = 0; i < 4; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonseeker_hurt__%02d.png", i);
		skeletonSeekerHurtTex[i] = loadImage(path);
	}

	for (int i = 0; i < 5; ++i) {
		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/overworld_creatures/skeletonseeker_death__%02d.png", i);
		skeletonSeekerDeathTex[i] = loadImage(path);
	}

	skeletonSeekerTexturesLoaded = true;
}

static void initializeSkeletonSeekers() {
	for (int i = 0; i < SKELETON_SEEKER_COUNT; ++i) {
		skeletonSeekers[i].homeX = SKELETON_SEEKER_SPAWNS[i].x;
		skeletonSeekers[i].homeY = SKELETON_SEEKER_SPAWNS[i].y;
		skeletonSeekers[i].x = skeletonSeekers[i].homeX;
		skeletonSeekers[i].y = skeletonSeekers[i].homeY;
		skeletonSeekers[i].hp = SKELETON_SEEKER_MAX_HP;
		skeletonSeekers[i].state = SkeletonSeeker::BURIED;
		skeletonSeekers[i].animFrame = 0;
		skeletonSeekers[i].animTimer = 0;
		skeletonSeekers[i].contactCooldown = 0;
		skeletonSeekers[i].hitCooldown = 0;
		skeletonSeekers[i].respawnTick = 0;
	}

	skeletonSeekersInitialized = true;
	initializeDistressedChildRescue();
	initializeAdditionalOverworldCreatures();
}

// Distressed child rescue + extra roamers
// The child encounter reuses skeleton seeker art, while the
// roaming enemies pull from their own sprite families.
static void updateRescueSeekerOne(SkeletonSeeker& seeker, float targetX, float targetY) {
	const int playerDamage = gPowerups.damageBoost ? PLAYER_BOOSTED_DAMAGE : PLAYER_BASE_DAMAGE;

	if (seeker.contactCooldown > 0) seeker.contactCooldown--;
	if (seeker.hitCooldown > 0) seeker.hitCooldown--;

	if (seeker.state == SkeletonSeeker::DEAD) return;

	seeker.animTimer++;

	if (seeker.state == SkeletonSeeker::EMERGING) {
		if (seeker.animTimer >= SKELETON_SEEKER_EMERGE_ANIM_SPEED) {
			seeker.animTimer = 0;
			seeker.animFrame++;
			if (seeker.animFrame >= 10) {
				seeker.state = SkeletonSeeker::CHASING;
				seeker.animFrame = 0;
				seeker.animTimer = 0;
			}
		}
		return;
	}

	if (seeker.state == SkeletonSeeker::DYING) {
		if (seeker.animTimer >= SKELETON_SEEKER_DEATH_ANIM_SPEED) {
			seeker.animTimer = 0;
			seeker.animFrame++;
			if (seeker.animFrame >= 5) {
				seeker.state = SkeletonSeeker::DEAD;
				awardOverworldKillScore();
			}
		}
		return;
	}

	if (player && player->isAttackHitFrame() && seeker.hitCooldown == 0 &&
		isWorldRectInsidePlayerStrike(seeker.x, seeker.y, SKELETON_SEEKER_DRAW_W, SKELETON_SEEKER_DRAW_H)) {
		seeker.hp -= playerDamage;
		seeker.hitCooldown = SKELETON_SEEKER_HIT_COOLDOWN;
		seeker.animFrame = 0;
		seeker.animTimer = 0;
		if (seeker.hp <= 0) {
			seeker.state = SkeletonSeeker::DYING;
			seeker.animFrame = 0;
			seeker.animTimer = 0;
			seeker.contactCooldown = 0;
		}
		return;
	}

	float dx = targetX - seeker.x;
	float dy = targetY - seeker.y;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len > 0.0001f) {
		seeker.x += (dx / len) * SKELETON_SEEKER_SPEED;
		seeker.y += (dy / len) * SKELETON_SEEKER_SPEED;
	}

	if (seeker.animTimer >= SKELETON_SEEKER_WALK_ANIM_SPEED) {
		seeker.animTimer = 0;
		seeker.animFrame = (seeker.animFrame + 1) % 6;
	}

	if (player) {
		float pdx = (player->x + player->w * 0.5f) - (seeker.x + SKELETON_SEEKER_DRAW_W * 0.5f);
		float pdy = (player->y + player->h * 0.5f) - (seeker.y + SKELETON_SEEKER_DRAW_H * 0.5f);
		if (pdx * pdx + pdy * pdy <= (float)(SKELETON_SEEKER_ATTACK_CONTACT_RANGE * SKELETON_SEEKER_ATTACK_CONTACT_RANGE)) {
			if (seeker.contactCooldown == 0) {
				seeker.contactCooldown = SKELETON_SEEKER_CONTACT_COOLDOWN;
				if (!player->isBlockingNow()) {
					player->takeDamage(SKELETON_SEEKER_CONTACT_DAMAGE);
				}
			}
		}
	}
}

static void initializeDistressedChildRescue() {
	distressedChildRescue.triggered = false;
	distressedChildRescue.active = false;
	distressedChildRescue.cleared = false;

	for (int i = 0; i < DISTRESSED_CHILD_SEEKER_COUNT; ++i) {
		SkeletonSeeker& seeker = distressedChildRescue.seekers[i];
		seeker.homeX = DISTRESSED_CHILD_SEEKER_SPAWNS[i].x;
		seeker.homeY = DISTRESSED_CHILD_SEEKER_SPAWNS[i].y;
		seeker.x = seeker.homeX;
		seeker.y = seeker.homeY;
		seeker.hp = SKELETON_SEEKER_MAX_HP;
		seeker.state = SkeletonSeeker::DEAD;
		seeker.animFrame = 0;
		seeker.animTimer = 0;
		seeker.contactCooldown = 0;
		seeker.hitCooldown = 0;
		seeker.respawnTick = 0;
	}
}

static void activateDistressedChildRescueWave() {
	distressedChildRescue.triggered = true;
	distressedChildRescue.active = true;
	for (int i = 0; i < DISTRESSED_CHILD_SEEKER_COUNT; ++i) {
		SkeletonSeeker& seeker = distressedChildRescue.seekers[i];
		seeker.x = seeker.homeX;
		seeker.y = seeker.homeY;
		seeker.hp = SKELETON_SEEKER_MAX_HP;
		seeker.state = SkeletonSeeker::EMERGING;
		seeker.animFrame = 0;
		seeker.animTimer = 0;
		seeker.contactCooldown = 0;
		seeker.hitCooldown = 0;
		seeker.respawnTick = 0;
	}
	getSaveRuntime().childRescueIntroSeen = 1;
	setWorldBanner("Protect Pruette from the attackers!", 150);
}

static bool checkDistressedChildRescueTrigger() {
	if (!player || distressedChildRescue.triggered || distressedChildRescue.cleared || overworldMonologueDialogue.inConversation) return false;

	float d2 = distSq(player->x, player->y, DISTRESSED_CHILD_EVENT_X, DISTRESSED_CHILD_EVENT_Y);
	if (d2 > DISTRESSED_CHILD_EVENT_TRIGGER_RADIUS * DISTRESSED_CHILD_EVENT_TRIGGER_RADIUS) return false;

	beginOverworldMonologue(
		"No... the child is surrounded.",
		"I cannot let these things drag the kid away.",
		"Stand back, child. I will clear them out.",
		2, 0);
	return true;
}

static bool updateDistressedChildRescue() {
	if (!player || !distressedChildRescue.active) return false;

	int aliveCount = 0;
	for (int i = 0; i < DISTRESSED_CHILD_SEEKER_COUNT; ++i) {
		SkeletonSeeker& seeker = distressedChildRescue.seekers[i];
		if (seeker.state == SkeletonSeeker::DEAD) continue;
		aliveCount++;

		float playerDx = player->x - seeker.x;
		float playerDy = player->y - seeker.y;
		float playerD2 = playerDx * playerDx + playerDy * playerDy;
		float targetX = (playerD2 < 180.0f * 180.0f) ? player->x : DISTRESSED_CHILD_EVENT_X;
		float targetY = (playerD2 < 180.0f * 180.0f) ? player->y : DISTRESSED_CHILD_EVENT_Y;
		updateRescueSeekerOne(seeker, targetX, targetY);

		if (player->getHealth() <= 0) {
			Engine::changeScene(new GameOverScene());
			return true;
		}
	}

	if (aliveCount == 0) {
		distressedChildRescue.active = false;
		distressedChildRescue.cleared = true;
		setWorldBanner("Pruette is safe for now.", 150);
	}

	return false;
}

static void drawDistressedChildRescue() {
	if (!distressedChildRescue.active) return;

	for (int i = 0; i < DISTRESSED_CHILD_SEEKER_COUNT; ++i) {
		const SkeletonSeeker& seeker = distressedChildRescue.seekers[i];
		if (seeker.state == SkeletonSeeker::DEAD) continue;

		int sx = (int)(seeker.x - cam.x);
		int sy = (int)(seeker.y - cam.y);

		if (sx + SKELETON_SEEKER_DRAW_W < 0 || sx > SCREEN_WIDTH) continue;
		if (sy + SKELETON_SEEKER_DRAW_H < 0 || sy > SCREEN_HEIGHT) continue;

		int texId = getSkeletonSeekerTexture(seeker);
		if (texId >= 0) drawImage(sx, sy, SKELETON_SEEKER_DRAW_W, SKELETON_SEEKER_DRAW_H, texId);
		else {
			setColor(112, 112, 112);
			fillRect(sx + 4, sy + 4, SKELETON_SEEKER_DRAW_W - 8, SKELETON_SEEKER_DRAW_H - 8);
		}
	}
}

static int getAdditionalCreatureDrawW(int family) { return 32; }
static int getAdditionalCreatureDrawH(int family) { return 32; }

static float getAdditionalCreatureSpeed(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_SPEED : SKELETONLIGHTER_SPEED;
}

static int getAdditionalCreatureMaxHP(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_MAX_HP : SKELETONLIGHTER_MAX_HP;
}

static int getAdditionalCreatureDamage(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_CONTACT_DAMAGE : SKELETONLIGHTER_CONTACT_DAMAGE;
}

static int getAdditionalCreatureContactCooldown(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_CONTACT_COOLDOWN : SKELETONLIGHTER_CONTACT_COOLDOWN;
}

static float getAdditionalCreatureTriggerRadius(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_TRIGGER_RADIUS : SKELETONLIGHTER_TRIGGER_RADIUS;
}

static float getAdditionalCreatureLeashRadius(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_LEASH_RADIUS : SKELETONLIGHTER_LEASH_RADIUS;
}

static float getAdditionalCreatureResetRadius(int family) {
	return (family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? OLDGUARDIAN_RESET_RADIUS : SKELETONLIGHTER_RESET_RADIUS;
}

static void initializeAdditionalCreatureOne(AdditionalOverworldCreature& c, float x, float y, int family) {
	c.x = x;
	c.y = y;
	c.homeX = x;
	c.homeY = y;
	c.hp = getAdditionalCreatureMaxHP(family);
	c.family = family;
	c.state = AdditionalOverworldCreature::IDLE;
	c.animFrame = 0;
	c.animTimer = 0;
	c.contactCooldown = 0;
	c.hitCooldown = 0;
	c.attackVariant = 0;
	c.respawnTick = 0;
}

static void initializeAdditionalOverworldCreatures() {
	for (int i = 0; i < OLDGUARDIAN_COUNT; ++i) {
		initializeAdditionalCreatureOne(oldGuardians[i], OLDGUARDIAN_SPAWNS[i].x, OLDGUARDIAN_SPAWNS[i].y, ADDITIONAL_FAMILY_OLDGUARDIAN);
		oldGuardians[i].attackVariant = i % 2;
	}
	for (int i = 0; i < SKELETONLIGHTER_COUNT; ++i) {
		initializeAdditionalCreatureOne(skeletonLighters[i], SKELETONLIGHTER_SPAWNS[i].x, SKELETONLIGHTER_SPAWNS[i].y, ADDITIONAL_FAMILY_SKELETONLIGHTER);
		skeletonLighters[i].attackVariant = i % 2;
	}
	additionalCreaturesInitialized = true;
}

// respawnFreeRoamAdditionalCreature
// Restores a roaming Old Guardian / Skeleton Lighter at its home
// point after the shared overworld respawn delay expires.
static void respawnFreeRoamAdditionalCreature(AdditionalOverworldCreature& c) {
	const int keptAttackVariant = c.attackVariant;
	const int keptFamily = c.family;
	const float keptHomeX = c.homeX;
	const float keptHomeY = c.homeY;
	initializeAdditionalCreatureOne(c, keptHomeX, keptHomeY, keptFamily);
	c.attackVariant = keptAttackVariant;
}

static void updateAdditionalCreatureOne(AdditionalOverworldCreature& c) {
	if (!player) return;

	if (c.state == AdditionalOverworldCreature::DEAD) {
		// Free-roam creatures return after the shared overworld
		// respawn delay. Their first-sighting monologues stay spent.
		if (c.respawnTick != 0 && (long)(GetTickCount() - c.respawnTick) >= 0) {
			respawnFreeRoamAdditionalCreature(c);
		}
		return;
	}

	if (c.contactCooldown > 0) c.contactCooldown--;
	if (c.hitCooldown > 0) c.hitCooldown--;

	const int drawW = getAdditionalCreatureDrawW(c.family);
	const int drawH = getAdditionalCreatureDrawH(c.family);
	const int playerDamage = gPowerups.damageBoost ? PLAYER_BOOSTED_DAMAGE : PLAYER_BASE_DAMAGE;

	if (c.state == AdditionalOverworldCreature::DYING) {
		const int deathFrames = (c.family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? 11 : 8;
		c.animTimer++;
		if (c.animTimer >= 6) {
			c.animTimer = 0;
			c.animFrame++;
			if (c.animFrame >= deathFrames) {
				c.state = AdditionalOverworldCreature::DEAD;
				awardOverworldKillScore();
			}
		}
		return;
	}

	if (player->isAttackHitFrame() && c.hitCooldown == 0 &&
		isWorldRectInsidePlayerStrike(c.x, c.y, drawW, drawH)) {
		c.hp -= playerDamage;
		c.hitCooldown = 12;
		c.animFrame = 0;
		c.animTimer = 0;
		if (c.hp <= 0) {
			c.state = AdditionalOverworldCreature::DYING;
			c.animFrame = 0;
			c.animTimer = 0;
			c.contactCooldown = 0;
			c.respawnTick = GetTickCount() + OVERWORLD_CREATURE_RESPAWN_DELAY_MS;
		}
		return;
	}

	float playerD2 = distSq(player->x, player->y, c.homeX, c.homeY);
	float triggerR = getAdditionalCreatureTriggerRadius(c.family);

	if (playerD2 <= triggerR * triggerR) {
		if (c.family == ADDITIONAL_FAMILY_OLDGUARDIAN && !getSaveRuntime().oldGuardianIntroSeen &&
			!overworldMonologueDialogue.inConversation && !skeletonIntroDialogue.inConversation) {
			getSaveRuntime().oldGuardianIntroSeen = 1;
			beginOverworldMonologue(
				"What is this old thing doing out here?",
				"It still guards these roads after all this time.",
				"I will put it down.");
			return;
		}
		if (c.family == ADDITIONAL_FAMILY_SKELETONLIGHTER && !getSaveRuntime().skeletonLighterIntroSeen &&
			!overworldMonologueDialogue.inConversation && !skeletonIntroDialogue.inConversation) {
			getSaveRuntime().skeletonLighterIntroSeen = 1;
			beginOverworldMonologue(
				"A lighter skeleton... quick and restless.",
				"It looks frail, but it will try to swarm me.",
				"Stay focused.");
			return;
		}

		// Once a roaming creature notices the player, it stays in
		// full pursuit until one of them dies. There is no fallback.
		c.state = AdditionalOverworldCreature::CHASING;
	}

	float targetX = c.homeX;
	float targetY = c.homeY;

	if (c.state == AdditionalOverworldCreature::CHASING) {
		targetX = player->x;
		targetY = player->y;
	}
	else {
		c.state = AdditionalOverworldCreature::IDLE;
	}

	float dx = targetX - c.x;
	float dy = targetY - c.y;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len > 0.0001f) {
		float speed = getAdditionalCreatureSpeed(c.family);
		if (c.state == AdditionalOverworldCreature::IDLE && distSq(c.x, c.y, c.homeX, c.homeY) < 4.0f) {
			// stay planted in idle
		}
		else {
			c.x += (dx / len) * speed;
			c.y += (dy / len) * speed;
		}
	}

	c.animTimer++;
	const int animSpeed = (c.family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? 5 : 5;
	if (c.animTimer >= animSpeed) {
		c.animTimer = 0;
		if (c.state == AdditionalOverworldCreature::CHASING) {
			c.animFrame = (c.animFrame + 1) % 8;
		}
		else {
			c.animFrame = (c.animFrame + 1) % ((c.family == ADDITIONAL_FAMILY_OLDGUARDIAN) ? 6 : 4);
		}
	}

	float cx = c.x + drawW * 0.5f;
	float cy = c.y + drawH * 0.5f;
	float pcx = player->x + player->w * 0.5f;
	float pcy = player->y + player->h * 0.5f;
	float pdx = pcx - cx;
	float pdy = pcy - cy;
	if (pdx * pdx + pdy * pdy <= 26.0f * 26.0f && c.contactCooldown == 0) {
		c.contactCooldown = getAdditionalCreatureContactCooldown(c.family);
		if (!player->isBlockingNow()) {
			player->takeDamage(getAdditionalCreatureDamage(c.family));
		}
	}
}

static bool updateAdditionalOverworldCreatures() {
	if (!player || !additionalCreaturesInitialized) return false;

	for (int i = 0; i < OLDGUARDIAN_COUNT; ++i) {
		updateAdditionalCreatureOne(oldGuardians[i]);
		if (player->getHealth() <= 0) {
			Engine::changeScene(new GameOverScene());
			return true;
		}
	}
	for (int i = 0; i < SKELETONLIGHTER_COUNT; ++i) {
		updateAdditionalCreatureOne(skeletonLighters[i]);
		if (player->getHealth() <= 0) {
			Engine::changeScene(new GameOverScene());
			return true;
		}
	}
	return false;
}

static int getAdditionalCreatureTexture(const AdditionalOverworldCreature& c) {
	if (c.family == ADDITIONAL_FAMILY_OLDGUARDIAN) {
		if (c.state == AdditionalOverworldCreature::DYING) return oldGuardianDeathTex[c.animFrame % 11];
		if (c.hitCooldown > 6) return oldGuardianHurtTex[c.animFrame % 4];
		if (c.contactCooldown > getAdditionalCreatureContactCooldown(c.family) / 2) {
			if (c.attackVariant == 0) return oldGuardianAttackPrimaryTex[c.animFrame % 11];
			return oldGuardianAttackSecondaryTex[c.animFrame % 8];
		}
		if (c.state == AdditionalOverworldCreature::CHASING) return oldGuardianWalkTex[c.animFrame % 8];
		return oldGuardianIdleTex[c.animFrame % 6];
	}

	if (c.state == AdditionalOverworldCreature::DYING) return skeletonLighterDeathTex[c.animFrame % 8];
	if (c.hitCooldown > 6) return skeletonLighterHurtTex[c.animFrame % 4];
	if (c.contactCooldown > getAdditionalCreatureContactCooldown(c.family) / 2) {
		if (c.attackVariant == 0) return skeletonLighterAttackPrimaryTex[c.animFrame % 8];
		return skeletonLighterAttackSecondaryTex[c.animFrame % 11];
	}
	if (c.state == AdditionalOverworldCreature::CHASING) return skeletonLighterWalkTex[c.animFrame % 8];
	return skeletonLighterIdleTex[c.animFrame % 4];
}

static void drawAdditionalCreatureOne(const AdditionalOverworldCreature& c) {
	if (c.state == AdditionalOverworldCreature::DEAD) return;

	const int drawW = getAdditionalCreatureDrawW(c.family);
	const int drawH = getAdditionalCreatureDrawH(c.family);
	int sx = (int)(c.x - cam.x);
	int sy = (int)(c.y - cam.y);
	if (sx + drawW < 0 || sx > SCREEN_WIDTH) return;
	if (sy + drawH < 0 || sy > SCREEN_HEIGHT) return;

	int texId = getAdditionalCreatureTexture(c);
	if (texId >= 0) drawImage(sx, sy, drawW, drawH, texId);
	else {
		if (c.family == ADDITIONAL_FAMILY_OLDGUARDIAN) setColor(90, 120, 80);
		else setColor(120, 120, 150);
		fillRect(sx, sy, drawW - 4, drawH - 4);
	}
}

static void drawAdditionalOverworldCreatures() {
	if (!additionalCreaturesInitialized) return;
	for (int i = 0; i < OLDGUARDIAN_COUNT; ++i) drawAdditionalCreatureOne(oldGuardians[i]);
	for (int i = 0; i < SKELETONLIGHTER_COUNT; ++i) drawAdditionalCreatureOne(skeletonLighters[i]);
}


// build / begin / finish / update skeleton intro dialogue
// Uses a short one-time player-only monologue before the first
// buried ambusher rises from the ground. It reuses the same
// 1-key dialogue flow as inspect points, but auto-triggers.
static void buildSkeletonIntroDialogue() {
	if (skeletonIntroDialogueBuilt) return;

	skeletonIntroDialogue.x = 0.0f;
	skeletonIntroDialogue.y = 0.0f;
	skeletonIntroDialogue.w = 32;
	skeletonIntroDialogue.h = 32;
	skeletonIntroDialogue.colorR = 0;
	skeletonIntroDialogue.colorG = 0;
	skeletonIntroDialogue.colorB = 0;
	skeletonIntroDialogue.name = "Xeralt";
	skeletonIntroDialogue.tex[0] = skeletonIntroDialogue.tex[1] = -1;
	skeletonIntroDialogue.animFrame = 0;
	skeletonIntroDialogue.animTimer = 0;
	skeletonIntroDialogue.inConversation = false;
	skeletonIntroDialogue.currentNode = 0;
	skeletonIntroDialogue.checkpointNode = 0;
	skeletonIntroDialogue.dialogueCompleted = false;
	skeletonIntroDialogue.hidePortrait = true;
	skeletonIntroDialogue.tree.nodeCount = 0;

	DialogueNode& n = skeletonIntroDialogue.tree.nodes[skeletonIntroDialogue.tree.nodeCount++];
	n.npcLine1 = "What is this...?";
	n.npcLine2 = "Something is moving beneath the soil.";
	n.npcLine3 = "Stay sharp.";
	n.responseCount = 1;
	n.responses[0] = { "[Ready yourself]", -1 };

	skeletonIntroDialogueBuilt = true;
}

static void beginSkeletonIntroDialogue(int seekerIdx) {
	buildSkeletonIntroDialogue();
	if (seekerIdx < 0 || seekerIdx >= SKELETON_SEEKER_COUNT) return;

	pendingSkeletonIntroIdx = seekerIdx;
	skeletonIntroInputCooldown = 20;
	skeletonSeekers[seekerIdx].state = SkeletonSeeker::INTRO_HOLD;
	skeletonSeekers[seekerIdx].animFrame = 0;
	skeletonSeekers[seekerIdx].animTimer = 0;
	skeletonIntroDialogue.beginDialogue();
}

static void finishSkeletonIntroDialogue() {
	skeletonIntroDialogue.endDialogue();
	getSaveRuntime().skeletonIntroSeen = 1;

	if (pendingSkeletonIntroIdx >= 0 && pendingSkeletonIntroIdx < SKELETON_SEEKER_COUNT) {
		SkeletonSeeker& seeker = skeletonSeekers[pendingSkeletonIntroIdx];
		if (seeker.state != SkeletonSeeker::DEAD) {
			seeker.state = SkeletonSeeker::EMERGING;
			seeker.animFrame = 0;
			seeker.animTimer = 0;
		}
	}

	pendingSkeletonIntroIdx = -1;
	skeletonIntroInputCooldown = 0;
}

static bool updateSkeletonIntroDialogue() {
	if (!skeletonIntroDialogue.inConversation) return false;

	if (skeletonIntroInputCooldown > 0) skeletonIntroInputCooldown--;

	if (skeletonIntroInputCooldown == 0) {
		if (consumeKey('1') || consumeKey(27)) {
			finishSkeletonIntroDialogue();
		}
	}

	return true;
}

// begin / finish / update generic overworld monologue dialogue
// Reuses the NPC dialogue UI for horde warnings, rescue beats,
// and first-time sightings of the new roaming creature families.
static void beginOverworldMonologue(const char* line1, const char* line2, const char* line3,
	int pendingAction, int pendingValue) {
	overworldMonologueDialogue.x = 0.0f;
	overworldMonologueDialogue.y = 0.0f;
	overworldMonologueDialogue.w = 32;
	overworldMonologueDialogue.h = 32;
	overworldMonologueDialogue.colorR = 0;
	overworldMonologueDialogue.colorG = 0;
	overworldMonologueDialogue.colorB = 0;
	overworldMonologueDialogue.name = "Xeralt";
	overworldMonologueDialogue.tex[0] = overworldMonologueDialogue.tex[1] = -1;
	overworldMonologueDialogue.animFrame = 0;
	overworldMonologueDialogue.animTimer = 0;
	overworldMonologueDialogue.inConversation = false;
	overworldMonologueDialogue.currentNode = 0;
	overworldMonologueDialogue.checkpointNode = 0;
	overworldMonologueDialogue.dialogueCompleted = false;
	overworldMonologueDialogue.hidePortrait = true;
	overworldMonologueDialogue.tree.nodeCount = 0;

	DialogueNode& n = overworldMonologueDialogue.tree.nodes[overworldMonologueDialogue.tree.nodeCount++];
	n.npcLine1 = line1 ? line1 : "";
	n.npcLine2 = line2 ? line2 : "";
	n.npcLine3 = line3 ? line3 : "";
	n.responseCount = 1;
	n.responses[0] = { "[Continue]", -1 };

	overworldMonologuePendingAction = pendingAction;
	overworldMonologuePendingValue = pendingValue;
	overworldMonologueInputCooldown = 20;
	overworldMonologueDialogue.beginDialogue();
}

static void finishOverworldMonologue() {
	overworldMonologueDialogue.endDialogue();
	if (overworldMonologuePendingAction == 1) {
		if (overworldMonologuePendingValue >= 0 && overworldMonologuePendingValue < OVERWORLD_HORDE_COUNT) {
			getSaveRuntime().hordeIntroSeen[overworldMonologuePendingValue] = 1;
			awakenHordeEncounter(overworldMonologuePendingValue);
		}
	}
	else if (overworldMonologuePendingAction == 2) {
		activateDistressedChildRescueWave();
	}
	overworldMonologuePendingAction = 0;
	overworldMonologuePendingValue = -1;
	overworldMonologueInputCooldown = 0;
}

static bool updateOverworldMonologue() {
	if (!overworldMonologueDialogue.inConversation) return false;
	if (overworldMonologueInputCooldown > 0) overworldMonologueInputCooldown--;
	if (overworldMonologueInputCooldown == 0) {
		if (consumeKey('1') || consumeKey(27)) {
			finishOverworldMonologue();
		}
	}
	return true;
}

static bool isHordeStillAlive(int hordeIdx) {
	if (hordeIdx < 0 || hordeIdx >= OVERWORLD_HORDE_COUNT) return false;
	const OverworldHordeConfig& cfg = OVERWORLD_HORDES[hordeIdx];
	const float r2 = (cfg.triggerRadius + 48.0f) * (cfg.triggerRadius + 48.0f);

	if (cfg.family == HORDE_FAMILY_SKELETON || cfg.family == HORDE_FAMILY_MIXED) {
		for (int i = 0; i < SKELETON_SEEKER_COUNT; ++i) {
			if (skeletonSeekers[i].state == SkeletonSeeker::DEAD) continue;
			if (distSq(skeletonSeekers[i].homeX, skeletonSeekers[i].homeY, cfg.x, cfg.y) <= r2) return true;
		}
	}
	if (cfg.family == HORDE_FAMILY_OLDGUARDIAN || cfg.family == HORDE_FAMILY_MIXED) {
		for (int i = 0; i < OLDGUARDIAN_COUNT; ++i) {
			if (oldGuardians[i].state == AdditionalOverworldCreature::DEAD) continue;
			if (distSq(oldGuardians[i].homeX, oldGuardians[i].homeY, cfg.x, cfg.y) <= r2) return true;
		}
	}
	if (cfg.family == HORDE_FAMILY_SKELETONLIGHTER || cfg.family == HORDE_FAMILY_MIXED) {
		for (int i = 0; i < SKELETONLIGHTER_COUNT; ++i) {
			if (skeletonLighters[i].state == AdditionalOverworldCreature::DEAD) continue;
			if (distSq(skeletonLighters[i].homeX, skeletonLighters[i].homeY, cfg.x, cfg.y) <= r2) return true;
		}
	}
	return false;
}

// awakenHordeEncounter / triggerHordesInRange
// Horde pockets should wake up as a group. Once the player trips
// a horde radius, every living member inside that pocket joins
// the chase together instead of staggering in one by one.
static void awakenHordeEncounter(int hordeIdx) {
	if (hordeIdx < 0 || hordeIdx >= OVERWORLD_HORDE_COUNT) return;

	const OverworldHordeConfig& cfg = OVERWORLD_HORDES[hordeIdx];
	const float wakeRadius = cfg.triggerRadius + 48.0f;
	const float wakeRadiusSq = wakeRadius * wakeRadius;

	if (cfg.family == HORDE_FAMILY_SKELETON || cfg.family == HORDE_FAMILY_MIXED) {
		for (int i = 0; i < SKELETON_SEEKER_COUNT; ++i) {
			SkeletonSeeker& seeker = skeletonSeekers[i];
			if (seeker.state == SkeletonSeeker::DEAD || seeker.state == SkeletonSeeker::DYING) continue;
			if (distSq(seeker.homeX, seeker.homeY, cfg.x, cfg.y) > wakeRadiusSq) continue;

			if (seeker.state == SkeletonSeeker::BURIED || seeker.state == SkeletonSeeker::INTRO_HOLD) {
				seeker.state = SkeletonSeeker::EMERGING;
				seeker.animFrame = 0;
				seeker.animTimer = 0;
				seeker.contactCooldown = 0;
				seeker.hitCooldown = 0;
			}
		}
	}

	if (cfg.family == HORDE_FAMILY_OLDGUARDIAN || cfg.family == HORDE_FAMILY_MIXED) {
		for (int i = 0; i < OLDGUARDIAN_COUNT; ++i) {
			AdditionalOverworldCreature& c = oldGuardians[i];
			if (c.state == AdditionalOverworldCreature::DEAD || c.state == AdditionalOverworldCreature::DYING) continue;
			if (distSq(c.homeX, c.homeY, cfg.x, cfg.y) > wakeRadiusSq) continue;

			if (c.state != AdditionalOverworldCreature::CHASING) {
				c.state = AdditionalOverworldCreature::CHASING;
				c.animFrame = 0;
				c.animTimer = 0;
			}
		}
	}

	if (cfg.family == HORDE_FAMILY_SKELETONLIGHTER || cfg.family == HORDE_FAMILY_MIXED) {
		for (int i = 0; i < SKELETONLIGHTER_COUNT; ++i) {
			AdditionalOverworldCreature& c = skeletonLighters[i];
			if (c.state == AdditionalOverworldCreature::DEAD || c.state == AdditionalOverworldCreature::DYING) continue;
			if (distSq(c.homeX, c.homeY, cfg.x, cfg.y) > wakeRadiusSq) continue;

			if (c.state != AdditionalOverworldCreature::CHASING) {
				c.state = AdditionalOverworldCreature::CHASING;
				c.animFrame = 0;
				c.animTimer = 0;
			}
		}
	}
}

static void triggerHordesInRange() {
	if (!player) return;

	for (int i = 0; i < OVERWORLD_HORDE_COUNT; ++i) {
		const OverworldHordeConfig& cfg = OVERWORLD_HORDES[i];
		if (!isHordeStillAlive(i)) continue;
		if (distSq(player->x, player->y, cfg.x, cfg.y) > cfg.triggerRadius * cfg.triggerRadius) continue;
		awakenHordeEncounter(i);
	}
}

static bool checkOverworldHordeMonologues() {
	if (!player || overworldMonologueDialogue.inConversation) return false;
	for (int i = 0; i < OVERWORLD_HORDE_COUNT; ++i) {
		if (getSaveRuntime().hordeIntroSeen[i] || !isHordeStillAlive(i)) continue;
		const OverworldHordeConfig& cfg = OVERWORLD_HORDES[i];
		if (distSq(player->x, player->y, cfg.x, cfg.y) > cfg.triggerRadius * cfg.triggerRadius) continue;

		if (cfg.family == HORDE_FAMILY_SKELETON) {
			getSaveRuntime().skeletonIntroSeen = 1;
			beginOverworldMonologue(
				"A whole nest of them...",
				"The ground here is crawling with seekers.",
				"I will have to break through.",
				1, i);
		}
		else if (cfg.family == HORDE_FAMILY_OLDGUARDIAN) {
			getSaveRuntime().oldGuardianIntroSeen = 1;
			beginOverworldMonologue(
				"Ancient guardians bar this route.",
				"Their armor still moves with hateful purpose.",
				"Steady now.",
				1, i);
		}
		else if (cfg.family == HORDE_FAMILY_SKELETONLIGHTER) {
			getSaveRuntime().skeletonLighterIntroSeen = 1;
			beginOverworldMonologue(
				"More dead things... and these ones burn bright.",
				"Fast feet, quick cuts, no hesitation.",
				"I move now.",
				1, i);
		}
		else {
			getSaveRuntime().skeletonIntroSeen = 1;
			getSaveRuntime().oldGuardianIntroSeen = 1;
			getSaveRuntime().skeletonLighterIntroSeen = 1;
			beginOverworldMonologue(
				"An entire horde waits ahead.",
				"Seekers, guardians, and burning bones in one place.",
				"There is no slipping past this lot.",
				1, i);
		}
		return true;
	}
	return false;
}

// isSkeletonInsidePlayerStrike
// Keeps the buried ambusher combat aligned with the player's
// directional overworld sword range.
// respawnFreeRoamSkeletonSeeker
// Restores a buried seeker at its original ambush point after the
// shared overworld respawn delay expires.
static void respawnFreeRoamSkeletonSeeker(SkeletonSeeker& seeker) {
	seeker.x = seeker.homeX;
	seeker.y = seeker.homeY;
	seeker.hp = SKELETON_SEEKER_MAX_HP;
	seeker.state = SkeletonSeeker::BURIED;
	seeker.animFrame = 0;
	seeker.animTimer = 0;
	seeker.contactCooldown = 0;
	seeker.hitCooldown = 0;
	seeker.respawnTick = 0;
}

static bool isSkeletonInsidePlayerStrike(const SkeletonSeeker& seeker) {
	if (!player) return false;

	float playerCx = player->x + player->w * 0.5f;
	float playerCy = player->y + player->h * 0.5f;
	float seekerCx = seeker.x + SKELETON_SEEKER_DRAW_W * 0.5f;
	float seekerCy = seeker.y + SKELETON_SEEKER_DRAW_H * 0.5f;
	float dx = seekerCx - playerCx;
	float dy = seekerCy - playerCy;

	switch (player->getFacingDir()) {
	case Player::DOWN:
		return std::fabs(dx) < 30.0f && dy < 12.0f && dy > -60.0f;
	case Player::UP:
		return std::fabs(dx) < 30.0f && dy > -12.0f && dy < 60.0f;
	case Player::LEFT:
		return std::fabs(dy) < 28.0f && dx < 12.0f && dx > -60.0f;
	case Player::RIGHT:
		return std::fabs(dy) < 28.0f && dx > -12.0f && dx < 60.0f;
	}

	return false;
}

// updateSkeletonSeekers / getTexture / draw
// Buried seekers stay dormant until approached, then emerge and
// stay in pursuit once awakened. Killing them removes them for
// this scene, but they no longer fall back to their spawn point.
static bool updateSkeletonSeekers() {
	if (!player || !skeletonSeekersInitialized) return false;

	const int playerDamage = gPowerups.damageBoost ? PLAYER_BOOSTED_DAMAGE : PLAYER_BASE_DAMAGE;

	for (int i = 0; i < SKELETON_SEEKER_COUNT; ++i) {
		SkeletonSeeker& seeker = skeletonSeekers[i];

		if (seeker.contactCooldown > 0) seeker.contactCooldown--;
		if (seeker.hitCooldown > 0) seeker.hitCooldown--;

		if (seeker.state == SkeletonSeeker::DEAD) {
			// Free-roam buried ambushers return after the shared
			// respawn delay, but they do not replay intro monologues.
			if (seeker.respawnTick != 0 && (long)(GetTickCount() - seeker.respawnTick) >= 0) {
				respawnFreeRoamSkeletonSeeker(seeker);
			}
			continue;
		}

		float homeDistSq = distSq(player->x, player->y, seeker.homeX, seeker.homeY);

		if (seeker.state == SkeletonSeeker::BURIED) {
			if (!getSaveRuntime().skeletonIntroSeen &&
				homeDistSq <= (float)(SKELETON_SEEKER_BURIED_TALK_RADIUS * SKELETON_SEEKER_BURIED_TALK_RADIUS) &&
				!skeletonIntroDialogue.inConversation && pendingSkeletonIntroIdx < 0) {
				beginSkeletonIntroDialogue(i);
				return false;
			}

			if (homeDistSq <= SKELETON_SEEKER_TRIGGER_RADIUS * SKELETON_SEEKER_TRIGGER_RADIUS) {
				seeker.state = SkeletonSeeker::EMERGING;
				seeker.animFrame = 0;
				seeker.animTimer = 0;
			}
			continue;
		}

		if (seeker.state == SkeletonSeeker::INTRO_HOLD) {
			continue;
		}

		// Once a seeker wakes up, it keeps hunting the player and
		// no longer falls back to its buried home position.
		seeker.animTimer++;

		if (seeker.state == SkeletonSeeker::EMERGING) {
			if (seeker.animTimer >= SKELETON_SEEKER_EMERGE_ANIM_SPEED) {
				seeker.animTimer = 0;
				seeker.animFrame++;
				if (seeker.animFrame >= 10) {
					seeker.state = SkeletonSeeker::CHASING;
					seeker.animFrame = 0;
					seeker.animTimer = 0;
				}
			}
			continue;
		}

		if (seeker.state == SkeletonSeeker::DYING) {
			if (seeker.animTimer >= SKELETON_SEEKER_DEATH_ANIM_SPEED) {
				seeker.animTimer = 0;
				seeker.animFrame++;
				if (seeker.animFrame >= 5) {
					seeker.state = SkeletonSeeker::DEAD;
					awardOverworldKillScore();
				}
			}
			continue;
		}

		if (player->isAttackHitFrame() && seeker.hitCooldown == 0 &&
			isSkeletonInsidePlayerStrike(seeker)) {
			seeker.hp -= playerDamage;
			seeker.hitCooldown = SKELETON_SEEKER_HIT_COOLDOWN;
			seeker.animFrame = 0;
			seeker.animTimer = 0;

			if (seeker.hp <= 0) {
				seeker.state = SkeletonSeeker::DYING;
				seeker.animFrame = 0;
				seeker.animTimer = 0;
				seeker.contactCooldown = 0;
				seeker.respawnTick = GetTickCount() + OVERWORLD_CREATURE_RESPAWN_DELAY_MS;
			}
			continue;
		}

		float targetX = player->x;
		float targetY = player->y;

		float dx = targetX - seeker.x;
		float dy = targetY - seeker.y;
		float len = std::sqrt(dx * dx + dy * dy);
		if (len > 0.001f) {
			seeker.x += (dx / len) * SKELETON_SEEKER_SPEED;
			seeker.y += (dy / len) * SKELETON_SEEKER_SPEED;
		}

		seeker.x = clampf(seeker.x, 0.0f, (float)(WORLD_W - SKELETON_SEEKER_DRAW_W));
		seeker.y = clampf(seeker.y, 0.0f, (float)(WORLD_H - SKELETON_SEEKER_DRAW_H));

		const int animSpeed = (seeker.contactCooldown > 0)
			? SKELETON_SEEKER_ATTACK_ANIM_SPEED
			: (seeker.hitCooldown > 0)
			? SKELETON_SEEKER_HURT_ANIM_SPEED
			: SKELETON_SEEKER_WALK_ANIM_SPEED;
		if (seeker.animTimer >= animSpeed) {
			seeker.animTimer = 0;
			if (seeker.contactCooldown > 0) {
				seeker.animFrame = (seeker.animFrame + 1) % 10;
			}
			else if (seeker.hitCooldown > 0) {
				seeker.animFrame = (seeker.animFrame + 1) % 4;
			}
			else {
				seeker.animFrame = (seeker.animFrame + 1) % 6;
			}
		}

		float playerCx = player->x + player->w * 0.5f;
		float playerCy = player->y + player->h * 0.5f;
		float seekerCx = seeker.x + SKELETON_SEEKER_DRAW_W * 0.5f;
		float seekerCy = seeker.y + SKELETON_SEEKER_DRAW_H * 0.5f;

		if (std::fabs(playerCx - seekerCx) < (float)SKELETON_SEEKER_ATTACK_CONTACT_RANGE &&
			std::fabs(playerCy - seekerCy) < (float)SKELETON_SEEKER_ATTACK_CONTACT_RANGE &&
			seeker.contactCooldown == 0) {
			seeker.contactCooldown = SKELETON_SEEKER_CONTACT_COOLDOWN;
			seeker.animFrame = 0;
			seeker.animTimer = 0;

			if (!player->isBlockingNow()) {
				player->takeDamage(SKELETON_SEEKER_CONTACT_DAMAGE);
				if (player->getHealth() <= 0) {
					Engine::changeScene(new GameOverScene());
					return true;
				}
			}
		}
	}

	return false;
}

static int getSkeletonSeekerTexture(const SkeletonSeeker& seeker) {
	if (seeker.state == SkeletonSeeker::BURIED || seeker.state == SkeletonSeeker::INTRO_HOLD) {
		return skeletonSeekerBuriedTex;
	}
	if (seeker.state == SkeletonSeeker::EMERGING) {
		return skeletonSeekerEmergeTex[seeker.animFrame % 10];
	}
	if (seeker.state == SkeletonSeeker::DYING) {
		return skeletonSeekerDeathTex[seeker.animFrame % 5];
	}
	if (seeker.hitCooldown > 0) {
		return skeletonSeekerHurtTex[seeker.animFrame % 4];
	}
	if (seeker.contactCooldown > 0) {
		return skeletonSeekerAttackTex[seeker.animFrame % 10];
	}
	return skeletonSeekerWalkTex[seeker.animFrame % 6];
}

static void drawSkeletonSeekers() {
	if (!skeletonSeekersInitialized) return;

	for (int i = 0; i < SKELETON_SEEKER_COUNT; ++i) {
		const SkeletonSeeker& seeker = skeletonSeekers[i];
		if (seeker.state == SkeletonSeeker::DEAD) continue;

		int sx = (int)(seeker.x - cam.x);
		int sy = (int)(seeker.y - cam.y);

		if (sx + SKELETON_SEEKER_DRAW_W < 0 || sx > SCREEN_WIDTH) continue;
		if (sy + SKELETON_SEEKER_DRAW_H < 0 || sy > SCREEN_HEIGHT) continue;

		int texId = getSkeletonSeekerTexture(seeker);
		if (texId >= 0) {
			drawImage(sx, sy, SKELETON_SEEKER_DRAW_W, SKELETON_SEEKER_DRAW_H, texId);
		}
		else {
			setColor(112, 112, 112);
			fillRect(sx + 4, sy + 4, SKELETON_SEEKER_DRAW_W - 8, SKELETON_SEEKER_DRAW_H - 8);
		}
	}
}

// beginWorldLoadIfNeeded / getWorldLoadCompletedStepCount
// Tracks the staged overworld bootstrap so a loading overlay can
// stay on screen for the full real loading duration.
static void beginWorldLoadIfNeeded() {
	if (worldLoadStarted) return;

	worldLoadStarted = true;
	worldLoadFinished = false;
	worldLoadStage = WORLD_LOAD_STAGE_SAVE_SLOT;
	worldLoadChunkCursor = chunksLoaded ? WORLD_LOAD_CHUNK_STEPS : 0;
	worldLoadStartTick = GetTickCount();
	worldLoadElapsedMs = 0;
}

static int getWorldLoadCompletedStepCount() {
	switch (worldLoadStage) {
	case WORLD_LOAD_STAGE_SAVE_SLOT:      return 0;
	case WORLD_LOAD_STAGE_CHUNKS:         return 1 + worldLoadChunkCursor;
	case WORLD_LOAD_STAGE_COLLISION:      return 1 + WORLD_LOAD_CHUNK_STEPS;
	case WORLD_LOAD_STAGE_POWERUP_WORLD:  return 28;
	case WORLD_LOAD_STAGE_POWERUP_TEX:    return 29;
	case WORLD_LOAD_STAGE_GUARD_TEX:      return 30;
	case WORLD_LOAD_STAGE_SKELETON_TEX:   return 31;
	case WORLD_LOAD_STAGE_FRUITS:         return 32;
	case WORLD_LOAD_STAGE_SKELETON_INIT:  return 33;
	case WORLD_LOAD_STAGE_SYNC_WORLD:     return 34;
	case WORLD_LOAD_STAGE_INSPECTS:       return 35;
	case WORLD_LOAD_STAGE_DONE:           return WORLD_LOAD_TOTAL_STEPS;
	}
	return 0;
}

// loadChunkTextureStep
// Loads exactly one world-map texture per update so the new
// loading bar can visibly advance instead of stalling.
static void loadChunkTextureStep(int stepIndex) {
	if (stepIndex < 0 || stepIndex > CHUNK_ROWS * CHUNK_COLS) return;

	if (stepIndex < CHUNK_ROWS * CHUNK_COLS) {
		int row = stepIndex / CHUNK_COLS + 1;
		int col = stepIndex % CHUNK_COLS + 1;

		char path[256];
		sprintf_s(path, sizeof(path),
			"assets/images/map/map%d%d.bmp", row, col);
		chunkTex[row - 1][col - 1] = loadImage(path);
		return;
	}

	minimapTex = loadImage("assets/images/map/map.bmp");
	chunksLoaded = true;
}

// syncWorldFromSaveIfNeeded
// Rebuilds the live overworld objects after a new game, save
// load, or any pending post-boss world sync request.
static void syncWorldFromSaveIfNeeded() {
	if (!player || !npcManager || consumePendingWorldSync()) {
		syncLiveWorldFromSave();
		questScrollVisible = false;

		// Re-seed the overworld creature runtime after any save sync.
		initializeSkeletonSeekers();
	}
}

// updateWorldLoadStep
// Advances one bootstrap stage per update while PlayScene::draw
// shows the menu-background loading overlay.
static bool updateWorldLoadStep() {
	beginWorldLoadIfNeeded();
	if (worldLoadFinished) return true;

	worldLoadElapsedMs = GetTickCount() - worldLoadStartTick;

	switch (worldLoadStage) {
	case WORLD_LOAD_STAGE_SAVE_SLOT:
		if (!hasActiveSaveSlot()) {
			beginNewSaveSlot();
		}
		worldLoadStage = WORLD_LOAD_STAGE_CHUNKS;
		break;

	case WORLD_LOAD_STAGE_CHUNKS:
		if (!chunksLoaded) {
			loadChunkTextureStep(worldLoadChunkCursor);
			worldLoadChunkCursor++;
			if (worldLoadChunkCursor >= WORLD_LOAD_CHUNK_STEPS) {
				chunksLoaded = true;
				worldLoadStage = WORLD_LOAD_STAGE_COLLISION;
			}
		}
		else {
			worldLoadChunkCursor = WORLD_LOAD_CHUNK_STEPS;
			worldLoadStage = WORLD_LOAD_STAGE_COLLISION;
		}
		break;

	case WORLD_LOAD_STAGE_COLLISION:
		loadCollisionMasks();
		worldLoadStage = WORLD_LOAD_STAGE_POWERUP_WORLD;
		break;

	case WORLD_LOAD_STAGE_POWERUP_WORLD:
		if (!powerupsInitialized) initializeWorldPowerups();
		worldLoadStage = WORLD_LOAD_STAGE_POWERUP_TEX;
		break;

	case WORLD_LOAD_STAGE_POWERUP_TEX:
		loadPowerupTexturesIfNeeded();
		worldLoadStage = WORLD_LOAD_STAGE_GUARD_TEX;
		break;

	case WORLD_LOAD_STAGE_GUARD_TEX:
		loadOverworldCreatureTexturesIfNeeded();
		worldLoadStage = WORLD_LOAD_STAGE_SKELETON_TEX;
		break;

	case WORLD_LOAD_STAGE_SKELETON_TEX:
		loadSkeletonSeekerTexturesIfNeeded();
		worldLoadStage = WORLD_LOAD_STAGE_FRUITS;
		break;

	case WORLD_LOAD_STAGE_FRUITS:
		if (!fruitsInitialized) initializeFruits();
		worldLoadStage = WORLD_LOAD_STAGE_SKELETON_INIT;
		break;

	case WORLD_LOAD_STAGE_SKELETON_INIT:
		if (!skeletonSeekersInitialized) initializeSkeletonSeekers();
		worldLoadStage = WORLD_LOAD_STAGE_SYNC_WORLD;
		break;

	case WORLD_LOAD_STAGE_SYNC_WORLD:
		syncWorldFromSaveIfNeeded();
		worldLoadStage = WORLD_LOAD_STAGE_INSPECTS;
		break;

	case WORLD_LOAD_STAGE_INSPECTS:
		if (!inspectPointsBuilt) buildAllInspectPoints();
		worldLoadStage = WORLD_LOAD_STAGE_DONE;
		break;

	case WORLD_LOAD_STAGE_DONE:
		worldLoadFinished = true;
		break;
	}

	worldLoadElapsedMs = GetTickCount() - worldLoadStartTick;
	return worldLoadFinished;
}

// drawWorldLoadingScreen
// Uses the menu background plus a dark framed panel so scene
// changes into the overworld feel intentional instead of frozen.
static void drawWorldLoadingScreen() {
	drawBMP(0, 0, "assets/images/menu_bg.bmp");

	const int panelW = OVERWORLD_LOADING_PANEL_W;
	const int panelH = OVERWORLD_LOADING_PANEL_H;
	const int panelX = (SCREEN_WIDTH - panelW) / 2;
	const int panelY = (SCREEN_HEIGHT - panelH) / 2 - 18;
	const int barW = OVERWORLD_LOADING_BAR_W;
	const int barH = OVERWORLD_LOADING_BAR_H;
	const int barX = (SCREEN_WIDTH - barW) / 2;
	const int barY = panelY + 30;

	int completed = getWorldLoadCompletedStepCount();
	if (completed < 0) completed = 0;
	if (completed > WORLD_LOAD_TOTAL_STEPS) completed = WORLD_LOAD_TOTAL_STEPS;
	int fillW = (WORLD_LOAD_TOTAL_STEPS > 0) ? (completed * (barW - 4)) / WORLD_LOAD_TOTAL_STEPS : 0;

	// Large dark panel over the menu art for a cleaner transition.
	setColor(10, 10, 14);
	fillRect(panelX, panelY, panelW, panelH);
	setColor(88, 72, 38);
	drawRectOutline(panelX, panelY, panelW, panelH);
	setColor(145, 124, 72);
	drawRectOutline(panelX + 1, panelY + 1, panelW - 2, panelH - 2);

	setColor(28, 18, 18);
	fillRect(barX, barY, barW, barH);
	setColor(95, 82, 42);
	drawRectOutline(barX, barY, barW, barH);

	if (fillW > 0) {
		setColor(188, 136, 48);
		fillRect(barX + 2, barY + 2, fillW, barH - 4);
		if (fillW > 10) {
			setColor(238, 210, 128);
			fillRect(barX + 3, barY + barH - 7, fillW - 2, 3);
		}
	}

	drawTextC(barX + 47, barY + 30, "Loading the overworld...", 240, 224, 176);

	char pctLine[64];
	sprintf_s(pctLine, sizeof(pctLine), "%d%%", (completed * 100) / WORLD_LOAD_TOTAL_STEPS);
	drawTextC(barX + barW - 28, barY + 30, pctLine, 240, 224, 176);

	char timeLine[64];
	sprintf_s(timeLine, sizeof(timeLine), "%.2f s", worldLoadElapsedMs / 1000.0f);
	drawTextC(barX + 108, panelY + 12, timeLine, 170, 196, 210);
}

// ---- loadAllChunks -----------------------------------------
// Path convention: assets/images/map/map{ROW}{COL}.bmp
// Missing files give id = -1; drawChunk() silently skips those.
static void loadAllChunks() {
	for (int row = 1; row <= CHUNK_ROWS; row++) {
		for (int col = 1; col <= CHUNK_COLS; col++) {
			char path[256];
			sprintf_s(path, sizeof(path),
				"assets/images/map/map%d%d.bmp", row, col);
			chunkTex[row - 1][col - 1] = loadImage(path);
		}
	}
	// Load the single full-world overview image for the mini-map
	minimapTex = loadImage("assets/images/map/map.bmp");
	chunksLoaded = true;
}

// CHUNK RENDERING
// Only draws the 3x3 neighbourhood of chunks around the camera
// centre - at most 9 GL draw calls per frame regardless of
// how large the world is.
static void drawVisibleChunks() {
	if (!chunksLoaded) return;

	int camCenterX = (int)cam.x + SCREEN_WIDTH / 2;
	int camCenterY = (int)cam.y + SCREEN_HEIGHT / 2;
	int centerCol = (int)(camCenterX / CHUNK_SIZE) + 1;
	int centerRow = (int)(camCenterY / CHUNK_SIZE) + 1;

	for (int row = centerRow - 1; row <= centerRow + 1; row++) {
		for (int col = centerCol - 1; col <= centerCol + 1; col++) {
			if (col < 1 || col > CHUNK_COLS) continue;
			if (row < 1 || row > CHUNK_ROWS) continue;

			int chunkWorldX = (col - 1) * CHUNK_SIZE;
			int chunkWorldY = (row - 1) * CHUNK_SIZE;
			int screenX = chunkWorldX - (int)cam.x;
			int screenY = chunkWorldY - (int)cam.y;

			if (screenX >= SCREEN_WIDTH) continue;
			if (screenY >= SCREEN_HEIGHT) continue;
			if (screenX + CHUNK_SIZE <= 0) continue;
			if (screenY + CHUNK_SIZE <= 0) continue;

			drawChunk(screenX, screenY, CHUNK_SIZE, CHUNK_SIZE,
				chunkTex[row - 1][col - 1]);
		}
	}
}

// HELPER - squared distance between two world points.
static inline float distSq(float ax, float ay, float bx, float by) {
	float dx = ax - bx;
	float dy = ay - by;
	return dx * dx + dy * dy;
}

// initializeWorldPowerups
// Places all overworld powerups at their fixed map locations.
static void initializeWorldPowerups() {
	// Speed Boost
	worldPowerups[0].x = 800.0f;
	worldPowerups[0].y = 850.0f;
	worldPowerups[0].type = 0;
	worldPowerups[0].collected = false;

	// Damage Boost
	worldPowerups[1].x = 1070.0f;
	worldPowerups[1].y = 2200.0f;
	worldPowerups[1].type = 1;
	worldPowerups[1].collected = false;

	// Defense Boost
	worldPowerups[2].x = 4260.0f;
	worldPowerups[2].y = 3540.0f;
	worldPowerups[2].type = 2;
	worldPowerups[2].collected = false;

	powerupsInitialized = true;
}

// loadPowerupTexturesIfNeeded
// Caches the powerup PNG icons once.
static void loadPowerupTexturesIfNeeded() {
	if (powerupTexturesLoaded) return;

	powerupTextures[0] = loadImage("assets/images/poweritems/speed.png");
	powerupTextures[1] = loadImage("assets/images/poweritems/sword.png");
	powerupTextures[2] = loadImage("assets/images/poweritems/shield.png");
	powerupTexturesLoaded = true;
}

// pushLiveWorldToSaveData
// Copies the live overworld state into GameState, then mirrors
// that into SaveData so the active save slot can be written.
static void pushLiveWorldToSaveData() {
	GameState& state = getGameState();

	if (player) {
		state.playerX = player->x;
		state.playerY = player->y;
	}

	state.powerupSpeed = gPowerups.speedBoost ? 1 : 0;
	state.powerupDamage = gPowerups.damageBoost ? 1 : 0;
	state.powerupDefense = gPowerups.defenseBoost ? 1 : 0;
	state.bossLocationsRevealed = gPowerups.bossLocationsRevealed ? 1 : 0;

	for (int i = 0; i < 3; i++) {
		state.collectedPowerups[i] = worldPowerups[i].collected ? 1 : 0;
	}

	if (npcManager) {
		npcManager->copyProgress(state.npcDialogueCompleted, state.npcCheckpoint);
	}

	syncSaveDataFromGameState();
}

// saveCurrentCheckpoint
// Writes the current live world state into the active save slot.
// This is used for autosaves after NPC talks and powerup pickups.
static void saveCurrentCheckpoint() {
	if (!hasActiveSaveSlot()) return;

	pushLiveWorldToSaveData();
	saveActiveGame();
}

// syncLiveWorldFromSave
// Rebuilds the live overworld state from the active save slot.
// Called after New Game, Load Save, or any requested world sync.
static void syncLiveWorldFromSave() {
	const GameState& state = getGameState();

	if (!powerupsInitialized) {
		initializeWorldPowerups();
	}

	if (player) {
		delete player;
		player = nullptr;
	}
	if (npcManager) {
		delete npcManager;
		npcManager = nullptr;
	}

	player = new Player();
	player->x = state.playerX;
	player->y = state.playerY;

	// Reset live overworld guard encounters when the world is rebuilt
	// from a save, load, or post-boss sync.
	resetAllGuardEncounters(false);

	npcManager = new NPCManager();
	npcManager->applyProgress(state.npcDialogueCompleted, state.npcCheckpoint);

	gPowerups.speedBoost = (state.powerupSpeed != 0);
	gPowerups.damageBoost = (state.powerupDamage != 0);
	gPowerups.defenseBoost = (state.powerupDefense != 0);
	gPowerups.bossLocationsRevealed = (state.bossLocationsRevealed != 0);

	for (int i = 0; i < 3; i++) {
		worldPowerups[i].collected = (state.collectedPowerups[i] != 0);
	}

	if (!fruitsInitialized) initializeFruits();
	if (state.fruitQuestState >= 2) {
		// Already collected before this session
		for (int i = 0; i < 3; i++) fruitItems[i].collected = true;
	}
	else {
		for (int i = 0; i < 3; i++) fruitItems[i].collected = false;
	}
}

static void buildInspectPoint(NPC& npc, float wx, float wy,
	const char* label) {
	npc.x = wx;
	npc.y = wy;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 0; npc.colorG = 0; npc.colorB = 0;
	npc.name = label;           // shows as speaker name in the box
	npc.tex[0] = npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;
	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = true;  // the new flag
	npc.tree.nodeCount = 0;
}

// Helper macro mirrors the ones in NPC.cpp
#define _IP(npc)       (npc).tree.nodes[(npc).tree.nodeCount]
#define END_IP(npc)    (npc).tree.nodeCount++

static void buildAllInspectPoints() {
	// ---- Point 0: old ruined well ----
	buildInspectPoint(inspectPoints[0], 2566.0f, 2624.0f, "Xeralt");
	{
		NPC& n = inspectPoints[0];
		_IP(n).npcLine1 = "An old well... the stonework has crumbled.";
		_IP(n).npcLine2 = "Someone carved a name here once.";
		_IP(n).npcLine3 = "The inscription is too worn to read.";
		_IP(n).responseCount = 1;
		_IP(n).responses[0] = { "[Look away]", -1 };
		END_IP(n);
	}

	// ---- Point 1: village roads ----
	buildInspectPoint(inspectPoints[1], 3220.0f, 2394.0f, "Xeralt");
	{
		NPC& n = inspectPoints[1];
		_IP(n).npcLine1 = "Seems like these roads were carved long ago.";
		_IP(n).npcLine2 = "Perhaps people of this land used it regularly.";
		_IP(n).npcLine3 = "But no usage is observed anymore...";
		_IP(n).responseCount = 1;
		_IP(n).responses[0] = { "[Move on]", -1 };
		END_IP(n);
	}

	// ---- Point 2: guard tower ----
	buildInspectPoint(inspectPoints[2], 1900.0f, 3554.0f, "Xeralt");
	{
		NPC& n = inspectPoints[2];
		_IP(n).npcLine1 = "This looks like some sort of a guard tower.";
		_IP(n).npcLine2 = "They look ancient yet recently used.";
		_IP(n).npcLine3 = "Those vile creatures must have resided in these.";
		_IP(n).responseCount = 1;
		_IP(n).responses[0] = { "[Move on]", -1 };
		END_IP(n);
	}

	// ---- Point 3: graveyard ----
	buildInspectPoint(inspectPoints[3], 4592.0f, 2778.0f, "Xeralt");
	{
		NPC& n = inspectPoints[3];
		_IP(n).npcLine1 = "A graveyard...";
		_IP(n).npcLine2 = "May you all rest in peace...";
		_IP(n).npcLine3 = "I shall absolve this land of the curse that lingers heavily.";
		_IP(n).responseCount = 1;
		_IP(n).responses[0] = { "[Stand up]", -1 };
		END_IP(n);
	}

	// ---- Point 4: forest ----
	buildInspectPoint(inspectPoints[4], 376.0f, 4380.0f, "Xeralt");
	{
		NPC& n = inspectPoints[4];
		_IP(n).npcLine1 = "A beautiful lush forest.";
		_IP(n).npcLine2 = "Even in these dark times, nature finds her way to thrive.";
		_IP(n).npcLine3 = "Wonderful sight, it is...";
		_IP(n).responseCount = 1;
		_IP(n).responses[0] = { "[keep walking]", -1 };
		END_IP(n);
	}

	// ---- Point 5: serpent island ----
	buildInspectPoint(inspectPoints[5], 1180.0f, 2174.0f, "Xeralt");
	{
		NPC& n = inspectPoints[5];
		_IP(n).npcLine1 = "I feel a strong repulsion force...";
		_IP(n).npcLine2 = "The dead beast emits such energy!";
		_IP(n).npcLine3 = "";
		_IP(n).responseCount = 1;
		_IP(n).responses[0] = { "[Move on]", -1 };
		END_IP(n);
	}

	inspectPointsBuilt = true;
}

#undef _IP
#undef END_IP

// ensureWorldReady
// The heavy boot work is now staged by updateWorldLoadStep().
// Once the world has fully loaded, this helper only handles any
// later save-sync requests (for example after boss scenes).
static void ensureWorldReady() {
	if (!worldLoadFinished) return;
	syncWorldFromSaveIfNeeded();
}

// drawOverworldHealthBar
// Draws a parchment-and-iron style health panel that fits the
// overworld UI and leaves room for future creature combat.
static void drawOverworldHealthBar() {
	if (!player) return;

	// Keep the health panel in the upper-left corner so it stays
	// readable during exploration, while world coordinates can live
	// on the opposite side of the screen without crowding the HUD.
	const int panelX = OVERWORLD_VITALITY_PANEL_X;
	const int panelY = OVERWORLD_VITALITY_PANEL_Y;
	const int panelW = OVERWORLD_VITALITY_PANEL_W;
	const int panelH = OVERWORLD_VITALITY_PANEL_H;
	const int barX = panelX + 12;
	const int barY = panelY + 10;
	const int barW = 176;
	const int barH = 14;

	const int hp = player->getHealth();
	const int maxHp = player->getMaxHealth();
	const int fillW = (maxHp > 0) ? (hp * barW) / maxHp : 0;

	// Back plate
	setColor(28, 20, 18);
	fillRect(panelX, panelY, panelW, panelH);
	setColor(122, 94, 42);
	drawRectOutline(panelX, panelY, panelW, panelH);
	setColor(168, 136, 72);
	drawRectOutline(panelX + 1, panelY + 1, panelW - 2, panelH - 2);

	// Inner trough
	setColor(62, 24, 24);
	fillRect(barX, barY, barW, barH);
	setColor(95, 70, 48);
	drawRectOutline(barX, barY, barW, barH);

	// Current health fill
	if (fillW > 0) {
		setColor(166, 34, 34);
		fillRect(barX + 1, barY + 1, fillW - (fillW > 1 ? 1 : 0), barH - 2);

		// Subtle highlight strip for a slightly richer look.
		if (fillW > 6) {
			setColor(224, 110, 84);
			fillRect(barX + 2, barY + barH - 5, fillW - 4, 3);
		}
	}

	drawTextC(panelX + 12, panelY + 25, "VITALITY", 214, 191, 133);

	char hpLine[48];
	sprintf_s(hpLine, sizeof(hpLine), "%d/%d", hp, maxHp);
	drawTextC(panelX + panelW - 70, panelY + 25, hpLine, 214, 191, 133);

	if (player->isRegenerating() && hp < maxHp) {
		drawTextC(panelX + 12, panelY + 4, "Recovery active", 110, 170, 110);
	}
}

// drawQuestScroll
// Displays the parchment-style quest / stats overlay.
static void drawQuestScroll() {
	const GameState& data = getGameState();

	const int panelX = 30;
	const int panelY = 45;
	const int panelW = 330;
	const int panelH = 360;

	setColor(96, 72, 34);
	fillRect(panelX - 4, panelY - 4, panelW + 8, panelH + 8);
	setColor(218, 196, 148);
	fillRect(panelX, panelY, panelW, panelH);
	setColor(140, 108, 52);
	drawRectOutline(panelX, panelY, panelW, panelH);

	drawTextC(panelX + 91, panelY + 326, "QUEST SCROLL", 70, 35, 10);
	drawTextC(panelX + 20, panelY + 300, "Active Save:", 80, 45, 20);
	drawTextC(panelX + 20, panelY + 283, data.displayName, 35, 35, 35);
	drawTextC(panelX + 20, panelY + 266, data.lastSaved, 70, 70, 70);

	drawTextC(panelX + 20, panelY + 235, "KNIGHT STATS", 80, 45, 20);

	char scoreLine[64];
	sprintf_s(scoreLine, sizeof(scoreLine), "Score: %d", getGameState().totalScore);

	char line[128];
	sprintf_s(line, sizeof(line), "World Speed: %.1f", gPowerups.speedBoost ? 2.8f : 2.0f);
	drawTextC(panelX + 20, panelY + 218, line, 20, 20, 20);

	sprintf_s(line, sizeof(line), "Overworld Vitality: %d / %d",
		player ? player->getHealth() : PLAYER_MAX_HP,
		player ? player->getMaxHealth() : PLAYER_MAX_HP);
	drawTextC(panelX + 20, panelY + 201, line, 20, 20, 20);

	sprintf_s(line, sizeof(line), "Battle Hitpoints: %d", PLAYER_MAX_HP);
	drawTextC(panelX + 20, panelY + 184, line, 20, 20, 20);

	sprintf_s(line, sizeof(line), "Battle Damage: %d", gPowerups.damageBoost ? PLAYER_BOOSTED_DAMAGE : PLAYER_BASE_DAMAGE);
	drawTextC(panelX + 20, panelY + 167, line, 20, 20, 20);

	sprintf_s(line, sizeof(line), "Defense Blessing: %s", gPowerups.defenseBoost ? "ACTIVE" : "Locked");
	drawTextC(panelX + 20, panelY + 150, line, 20, 20, 20);

	drawTextC(panelX + 20, panelY + 133, scoreLine, 50, 120, 50);

	drawTextC(panelX + 20, panelY + 116, "CURRENT OBJECTIVES", 80, 45, 20);

	const char* objective1 = "Speak with the Village Elder.";
	if (!data.bossDefeated[0] && gPowerups.bossLocationsRevealed) objective1 = "Castle location marked on the map.";
	if (data.bossDefeated[0]) objective1 = "Castle Knight defeated.";
	if (data.bossDefeated[1]) objective1 = "Sanctum Sorcerer defeated.";
	if (data.bossDefeated[2]) objective1 = "Corrupted King defeated.";

	// Do not show the castle-fight objective until the Village Elder
	// has actually revealed the route on the overworld map.
	const char* objective2 = gPowerups.bossLocationsRevealed ?
		"Defeat the Castle Knight." :
		"";
	if (data.bossDefeated[0] && !data.bossDefeated[1]) objective2 = "The Sanctum now stands revealed.";
	if (data.bossDefeated[1] && !data.bossDefeated[2]) objective2 = "The Throne Room now stands revealed.";
	if (data.bossDefeated[2]) objective2 = "The journey is complete.";

	const char* objective3 = "Speak with Elara, the herb doctor.";
	if (data.fruitQuestState == 1) objective3 = "Collect 3 moonberries for Elara.";
	if (data.fruitQuestState == 2) objective3 = "Return to Elara with the moonberries.";
	if (data.fruitQuestState >= 3) objective3 = "Elara's vitality tonic secured.";

	const char* objective4 = gPowerups.speedBoost ?
		"Maximus's speed blessing is active." :
		"Talk to Maximus for speed blessing.";

	const char* objective5 = gPowerups.damageBoost ?
		"Rosevelt's damage blessing is active." :
		"Talk to Rosevelt for damage blessing.";

	const char* objective6 = gPowerups.defenseBoost ?
		"Melika's defense blessing is active." :
		"Talk to Melika for defense blessing.";

	// New work: pack the visible objectives tightly so empty lines do not
	// waste scroll space. This keeps Rosevelt and Melika on their own lines
	// while still preserving the boss-route objective when it is active.
	const char* objectiveLines[6] = { objective1, objective2, objective3, objective4, objective5, objective6 };
	int objectiveCount = 0;
	for (int i = 0; i < 6; ++i) {
		if (objectiveLines[i] && objectiveLines[i][0] != '\0') {
			drawTextC(panelX + 20, panelY + 96 - objectiveCount * 16, objectiveLines[i], 15, 15, 15);
			objectiveCount++;
		}
	}

	drawTextC(panelX + 20, panelY + 4, "Press I to close the scroll.", 85, 45, 20);
}

// PlayScene::reset
// Clears the static world state so the next PlayScene() rebuilds
// the overworld from scratch.
void PlayScene::reset() {
	delete player;     player = nullptr;
	delete npcManager; npcManager = nullptr;
	chunksLoaded = false;
	minimapTex = -1;
	powerupsInitialized = false;
	powerupTexturesLoaded = false;
	questScrollVisible = false;
	fruitsInitialized = false;
	inspectPointsBuilt = false;
	activeInspectIdx = -1;
	inspectInputCooldown = 0;
	// Reset all overworld guard encounter runtime state.
	for (int i = 0; i < GUARD_ENCOUNTER_COUNT; ++i) {
		guardEncounters[i].active = false;
		guardEncounters[i].cleared = false;
		for (int j = 0; j < OVERWORLD_GUARD_COUNT; ++j) {
			guardEncounters[i].guards[j].active = false;
			guardEncounters[i].guards[j].state = OverworldMinion::SPAWNING;
			guardEncounters[i].guards[j].animFrame = 0;
			guardEncounters[i].guards[j].animTimer = 0;
			guardEncounters[i].guards[j].contactCooldown = 0;
			guardEncounters[i].guards[j].hitCooldown = 0;
		}
	}
	overworldCreatureTexturesLoaded = false;
	skeletonSeekersInitialized = false;
	skeletonSeekerTexturesLoaded = false;
	additionalCreaturesInitialized = false;
	additionalOverworldCreatureTexturesLoaded = false;
	distressedChildRescue.triggered = false;
	distressedChildRescue.active = false;
	distressedChildRescue.cleared = false;
	pendingSkeletonIntroIdx = -1;
	skeletonIntroInputCooldown = 0;
	skeletonIntroDialogue.inConversation = false;
	overworldMonologueDialogue.inConversation = false;
	overworldMonologueInputCooldown = 0;
	overworldMonologuePendingAction = 0;
	overworldMonologuePendingValue = -1;
	worldLoadStarted = false;
	worldLoadFinished = false;
	worldLoadStage = WORLD_LOAD_STAGE_SAVE_SLOT;
	worldLoadChunkCursor = 0;
	worldLoadStartTick = 0;
	worldLoadElapsedMs = 0;
}

// PlayScene::PlayScene
// Scene objects are created lazily inside ensureWorldReady().
PlayScene::PlayScene() {}

// PlayScene::update
// Called every fixed-update frame (~60 fps / 16 ms).
void PlayScene::update() {
	if (!updateWorldLoadStep()) {
		return;
	}

	ensureWorldReady();

	if (getSaveRuntime().bannerTimer > 0) {
		getSaveRuntime().bannerTimer--;
	}

	// Toggle the quest scroll on a fresh I key press.
	if (consumeKey('i') || consumeKey('I')) {
		questScrollVisible = !questScrollVisible;
	}
	if (questScrollVisible && consumeKey(27)) {
		questScrollVisible = false;
	}

	// ---- Auto monologues already in progress ----
	// Freeze the world before any other overworld logic can run.
	if (updateSkeletonIntroDialogue()) return;
	if (updateOverworldMonologue()) return;

	// ---- Distressed child rescue trigger ----
	// The ambush should fire as soon as the player approaches the
	// child, before any dialogue prompt can interrupt the event.
	if (checkDistressedChildRescueTrigger()) return;

	// ---- NPC dialogue ----
	// Returns true while a conversation is active; freeze everything else.
	bool talking = false;
	if (!distressedChildRescue.active) {
		talking = npcManager->update(player->x, player->y);
	}
	if (npcManager->consumeConversationFinished()) {
		saveCurrentCheckpoint();

		// Apply Elara HP bonus if quest just completed
		int expectedBonus = (getGameState().fruitQuestState >= 3) ? 20 : 0;
		int expectedMaxHP = PLAYER_MAX_HP + expectedBonus;
		if (player && player->maxHealth != expectedMaxHP) {
			int diff = expectedMaxHP - player->maxHealth;
			player->maxHealth = expectedMaxHP;
			player->health = player->health + diff;
			if (player->health > player->maxHealth)
				player->health = player->maxHealth;
			setWorldBanner("VITALITY INCREASED!", 120);
		}
	}
	if (talking) return;

	// ---- Horde warning monologues ----
	// Packed overworld pockets call out the danger once before
	// the player pushes into them for the first time.
	if (checkOverworldHordeMonologues()) return;

	// Once the player crosses into a horde pocket, every living
	// member of that group should wake up together.
	triggerHordesInRange();

	// ---- Inspect point dialogue ----
	if (inspectInputCooldown > 0) inspectInputCooldown--;

	if (activeInspectIdx >= 0) {
		NPC& ip = inspectPoints[activeInspectIdx];

		if (!ip.inConversation) {
			activeInspectIdx = -1;
		}
		else if (inspectInputCooldown == 0) {
			// Number keys select responses
			int nodeCount = ip.tree.nodes[ip.currentNode].responseCount;
			for (int i = 0; i < nodeCount; i++) {
				if (consumeKey((unsigned char)('1' + i))) {
					inspectInputCooldown = 15;
					if (!ip.selectResponse(i))
						activeInspectIdx = -1;
					break;
				}
			}
			// ESC closes
			if (consumeKey(27)) {
				ip.endDialogue();
				activeInspectIdx = -1;
			}
		}
		return;   // freeze player while inspecting
	}

	// Check proximity + E press for inspect points (only when not talking to NPC)
	if (!talking) {
		for (int i = 0; i < INSPECT_COUNT; i++) {
			float dx = player->x - inspectPoints[i].x;
			float dy = player->y - inspectPoints[i].y;
			if (dx * dx + dy * dy < 80.0f * 80.0f) {
				if (consumeKey('e') || consumeKey('E')) {
					inspectPoints[i].beginDialogue();
					activeInspectIdx = i;
					inspectInputCooldown = 20;
					return;
				}
				break;   // only one proximity hit needed
			}
		}
	}

	// ---- Player movement + collision + animation ----
	player->update();

	// ---- Buried ambushers + rescue rush + roaming creatures ----
	// Ambient birds were removed, but the free-roam danger now
	// comes from seekers, the child-rescue ambush, and two extra
	// roaming creature families configured from GameConfig.h.
	if (updateDistressedChildRescue()) return;
	if (updateSkeletonSeekers()) return;
	if (updateAdditionalOverworldCreatures()) return;
	if (skeletonIntroDialogue.inConversation) return;

	// ---- Boss-area overworld guard waves ----
	// This runs after player input so sword swings and blocks can
	// immediately interact with the chasing minions.
	if (updateOverworldGuardEncounters()) return;

	// ---- Powerup pickup detection ----
	for (int i = 0; i < 3; i++) {
		if (worldPowerups[i].collected) continue;
		if (!npcManager->isDialogueComplete(POWERUP_UNLOCK_NPC[i])) continue;

		float dx = player->x - worldPowerups[i].x;
		float dy = player->y - worldPowerups[i].y;

		// Simple 20-unit AABB proximity check
		if (std::abs(dx) < 20 && std::abs(dy) < 20) {
			worldPowerups[i].collected = true;

			// Apply the matching global powerup flag (defined in Powerups.cpp)
			if (worldPowerups[i].type == 0) gPowerups.speedBoost = true;
			else if (worldPowerups[i].type == 1) gPowerups.damageBoost = true;
			else if (worldPowerups[i].type == 2) gPowerups.defenseBoost = true;

			setWorldBanner(worldPowerups[i].type == 0 ? "SPEED BLESSING OBTAINED" :
				worldPowerups[i].type == 1 ? "DAMAGE BLESSING OBTAINED" :
				"DEFENSE BLESSING OBTAINED", 160);
			saveCurrentCheckpoint();
		}
	}

	// ---- Moonberry collection (Elara's quest) ----
	{
		GameState& gs = getGameState();
		if (gs.fruitQuestState == 1) {
			int remaining = 0;
			for (int i = 0; i < 3; i++) {
				if (!fruitItems[i].collected) {
					float dx = player->x - fruitItems[i].x;
					float dy = player->y - fruitItems[i].y;
					if (std::abs(dx) < 20 && std::abs(dy) < 20) {
						fruitItems[i].collected = true;
						setWorldBanner("MOONBERRY COLLECTED!", 90);
					}
					else {
						remaining++;
					}
				}
			}
			if (remaining == 0) {
				gs.fruitQuestState = 2;
				setWorldBanner("ALL 3 MOONBERRIES FOUND! Return to Elara.", 180);
				saveCurrentCheckpoint();
			}
		}
	}

	// ---- Camera: follow player, clamped to world edges ----
	cam.x = clampf(player->x - SCREEN_WIDTH / 2.0f, 0.0f,
		static_cast<float>(WORLD_W - SCREEN_WIDTH));
	cam.y = clampf(player->y - SCREEN_HEIGHT / 2.0f, 0.0f,
		static_cast<float>(WORLD_H - SCREEN_HEIGHT));

	// ---- Entrance detection ----
	bool enterPressed = consumeKey('e') || consumeKey('E');
	const GameState& data = getGameState();

	const bool sanctumUnlocked = (data.bossDefeated[0] != 0);
	const bool throneUnlocked = (data.bossDefeated[1] != 0);

	// CHECKPOINT 1: Castle - Village Elder must reveal locations first
	if (enterPressed && !data.bossDefeated[0] && gPowerups.bossLocationsRevealed &&
		distSq(player->x, player->y, (float)CASTLE_X, (float)CASTLE_Y) < 100.0f * 100.0f) {
		if (countActiveGuards(GUARD_ENCOUNTER_CASTLE) > 0) {
			setWorldBanner(getEncounterBlockMessage(GUARD_ENCOUNTER_CASTLE), 90);
			return;
		}
		Engine::changeScene(new BossCutscene1());
		return;
	}

	// CHECKPOINT 2: Sanctum - only opens after boss 1 is defeated
	if (enterPressed && sanctumUnlocked && !data.bossDefeated[1] &&
		distSq(player->x, player->y, (float)SANCTUM_X, (float)SANCTUM_Y) < 100.0f * 100.0f) {
		if (countActiveGuards(GUARD_ENCOUNTER_SANCTUM) > 0) {
			setWorldBanner(getEncounterBlockMessage(GUARD_ENCOUNTER_SANCTUM), 90);
			return;
		}
		Engine::changeScene(new BossCutscene2());
		return;
	}

	// CHECKPOINT 3: Throne Room - only opens after boss 2 is defeated
	if (enterPressed && throneUnlocked && !data.bossDefeated[2] &&
		distSq(player->x, player->y, (float)THRONE_X, (float)THRONE_Y) < 100.0f * 100.0f) {
		if (countActiveGuards(GUARD_ENCOUNTER_THRONE) > 0) {
			setWorldBanner(getEncounterBlockMessage(GUARD_ENCOUNTER_THRONE), 90);
			return;
		}
		Engine::changeScene(new BossCutscene3());
		return;
	}
}

// Draw order (back to front):
// World chunk tiles
// NPC bodies (world space)
// Player sprite (world space)
// NPC "Press E" prompts (world space overlay)
// Powerup icons + labels + dialogue
// Mini-map (screen space)
// World banner / entrance prompts / controls hint
// Quest scroll overlay
void PlayScene::draw() {
	beginWorldLoadIfNeeded();
	if (!worldLoadFinished) {
		drawWorldLoadingScreen();
		return;
	}

	ensureWorldReady();

	// 1. WORLD TILES
	drawVisibleChunks();

	// 2. NPC BODIES
	npcManager->drawWorld(cam.x, cam.y);

	// 3. OVERWORLD CREATURES + PLAYER SPRITE
	drawSkeletonSeekers();
	drawDistressedChildRescue();
	drawAdditionalOverworldCreatures();
	drawOverworldGuards();

	if (player) {
		player->draw();

		// 4. NPC "PRESS E" PROMPTS (near player only)
		npcManager->drawPrompts(player->x, player->y, cam.x, cam.y);
	}

	// 6. POWERUP ICONS + PROXIMITY LABELS + DIALOGUE
	for (int i = 0; i < 3; i++) {
		if (worldPowerups[i].collected) continue;
		if (!npcManager->isDialogueComplete(POWERUP_UNLOCK_NPC[i])) continue;

		int sx = (int)(worldPowerups[i].x - cam.x);
		int sy = (int)(worldPowerups[i].y - cam.y);

		int texId = powerupTextures[worldPowerups[i].type];

		if (texId >= 0) {
			drawImage(sx, sy, 32, 32, texId);
		}
		else {
			if (worldPowerups[i].type == 0) setColor(0, 255, 0);
			else if (worldPowerups[i].type == 1) setColor(255, 0, 0);
			else                                 setColor(0, 0, 255);
			fillRect(sx, sy, 10, 10);
		}

		if (player) {
			float dx = player->x - worldPowerups[i].x;
			float dy = player->y - worldPowerups[i].y;

			if (std::abs(dx) < 80 && std::abs(dy) < 80) {
				const char* labels[] = { "Speed Boost!", "Damage Boost!", "Defense Boost!" };
				const char* label = labels[worldPowerups[i].type];

				drawTextC(sx - 1, sy + 21, label, 0, 0, 0);
				if (worldPowerups[i].type == 0)
					drawTextC(sx - 2, sy + 22, label, 0, 255, 0);
				else if (worldPowerups[i].type == 1)
					drawTextC(sx - 2, sy + 22, label, 255, 80, 80);
				else
					drawTextC(sx - 2, sy + 22, label, 100, 150, 255);
			}
		}
	}

	npcManager->drawUI();

	// 6b. MOONBERRY FRUIT ITEMS
	{
		const GameState& gs = getGameState();
		if (gs.fruitQuestState == 1) {
			for (int i = 0; i < 3; i++) {
				if (fruitItems[i].collected) continue;

				int sx = (int)(fruitItems[i].x - cam.x);
				int sy = (int)(fruitItems[i].y - cam.y);

				// Off-screen cull
				if (sx < -20 || sx > SCREEN_WIDTH + 20) continue;
				if (sy < -20 || sy > SCREEN_HEIGHT + 20) continue;

				// Outer glow (slightly larger, dark red)
				setColor(140, 0, 0);
				fillRect(sx - 1, sy - 1, 14, 14);
				// Berry (bright red)
				setColor(220, 30, 30);
				fillRect(sx, sy, 12, 12);
				// Highlight dot (top-left)
				setColor(255, 130, 130);
				fillRect(sx + 2, sy + 2, 3, 3);

				// Proximity label
				if (player) {
					float dx = player->x - fruitItems[i].x;
					float dy = player->y - fruitItems[i].y;
					if (std::abs(dx) < 80 && std::abs(dy) < 80) {
						drawTextC(sx - 8, sy + 18, "Moonberry", 0, 0, 0);
						drawTextC(sx - 9, sy + 19, "Moonberry", 255, 80, 80);
					}
				}
			}
		}
		// Show a "Return to Elara" reminder when all berries are gathered
		if (gs.fruitQuestState == 2) {
			drawTextC(231, 276, "Return to Elara with the moonberries!", 0, 0, 0);
			drawTextC(230, 275, "Return to Elara with the moonberries!", 255, 160, 100);
		}
		if (distressedChildRescue.active) {
			drawTextC(243, 251, "Save Pruette from the attackers!", 0, 0, 0);
			drawTextC(242, 250, "Save Pruette from the attackers!", 255, 120, 120);
		}
	}

	// ---- Inspect point prompts ----
	if (activeInspectIdx < 0 && player) {
		for (int i = 0; i < INSPECT_COUNT; i++) {
			float dx = player->x - inspectPoints[i].x;
			float dy = player->y - inspectPoints[i].y;
			if (dx * dx + dy * dy < 80.0f * 80.0f) {
				int sx = (int)(inspectPoints[i].x - cam.x) - 30;
				int sy = (int)(inspectPoints[i].y - cam.y) + 36;
				drawTextC(sx + 1, sy - 1, "Press E to inspect", 0, 0, 0);
				drawTextC(sx, sy, "Press E to inspect", 255, 200, 80);
				break;
			}
		}
	}

	// ---- Inspect point dialogue UI ----
	if (activeInspectIdx >= 0) {
		inspectPoints[activeInspectIdx].drawDialogueUI();
	}

	// The first skeleton seeker uses an automatic player-only
	// monologue before it rises from the ground.
	if (skeletonIntroDialogue.inConversation) {
		skeletonIntroDialogue.drawDialogueUI();
	}
	if (overworldMonologueDialogue.inConversation) {
		overworldMonologueDialogue.drawDialogueUI();
	}

	// Show the overworld health panel.
	drawOverworldHealthBar();

	// Show world coordinates on the opposite side of the HUD so the
	// upper-left health panel remains clean and easy to read.
	if (player) {
		char coordBuf[64];
		sprintf_s(coordBuf, sizeof(coordBuf), "X%.0f Y%.0f", player->x, player->y);

		const int coordPanelW = OVERWORLD_COORD_PANEL_W;
		const int coordPanelH = OVERWORLD_COORD_PANEL_H;
		// Keep the coordinate plate clear of the minimap frame while
		// trimming the plate down so it stays lightweight.
		const int coordPanelX = SCREEN_WIDTH - 130 - OVERWORLD_COORD_PANEL_MARGIN -
			coordPanelW - OVERWORLD_COORD_PANEL_MARGIN;
		const int coordPanelY = SCREEN_HEIGHT - coordPanelH - OVERWORLD_COORD_PANEL_MARGIN;

		setColor(18, 22, 28);
		fillRect(coordPanelX, coordPanelY, coordPanelW, coordPanelH);
		setColor(60, 96, 124);
		drawRectOutline(coordPanelX, coordPanelY, coordPanelW, coordPanelH);
		setColor(120, 166, 198);
		drawRectOutline(coordPanelX + 1, coordPanelY + 1, coordPanelW - 2, coordPanelH - 2);

		drawTextC(coordPanelX + 8, coordPanelY + 6, coordBuf, 190, 236, 255);
	}

	// 6. MINI-MAP (top-right corner, screen space)
	{
		const int miniW = 130;
		const int miniH = 130;
		const int miniMargin = 10;
		const int miniX = SCREEN_WIDTH - miniW - miniMargin;
		const int miniY = SCREEN_HEIGHT - miniH - miniMargin;
		const GameState& data = getGameState();

		// Draw the overview map image as the minimap background
		if (minimapTex >= 0) {
			drawImage(miniX, miniY, miniW, miniH, minimapTex);
		}
		else {
			// Fallback: solid black if texture failed to load
			setColor(0, 0, 0);
			fillRect(miniX, miniY, miniW, miniH);
		}

		// Only the currently unlocked destination is shown on the mini-map.
		// The elder can reveal the castle up front, then each defeated boss
		// unlocks the next destination in sequence.
		if (!data.bossDefeated[0] && gPowerups.bossLocationsRevealed) {
			setColor(255, 0, 0);
			int castleMiniX = miniX + (int)((float)CASTLE_X / (float)WORLD_W * miniW);
			int castleMiniY = miniY + (int)((float)CASTLE_Y / (float)WORLD_H * miniH);
			fillRect(castleMiniX, castleMiniY, 5, 5);
		}
		else if (!data.bossDefeated[1] && data.bossDefeated[0]) {
			setColor(200, 0, 200);
			int sanctumMiniX = miniX + (int)((float)SANCTUM_X / (float)WORLD_W * miniW);
			int sanctumMiniY = miniY + (int)((float)SANCTUM_Y / (float)WORLD_H * miniH);
			fillRect(sanctumMiniX, sanctumMiniY, 5, 5);
		}
		else if (!data.bossDefeated[2] && data.bossDefeated[1]) {
			setColor(80, 0, 150);
			int throneMiniX = miniX + (int)((float)THRONE_X / (float)WORLD_W * miniW);
			int throneMiniY = miniY + (int)((float)THRONE_Y / (float)WORLD_H * miniH);
			fillRect(throneMiniX, throneMiniY, 5, 5);
		}

		if (player) {
			setColor(255, 0, 0);
			int playerMiniX = miniX + (int)((float)player->x / (float)WORLD_W * miniW);
			int playerMiniY = miniY + (int)((float)player->y / (float)WORLD_H * miniH);
			fillRect(playerMiniX, playerMiniY, 4, 4);
		}

		// Black border frame around the minimap
		setColor(0, 0, 0);
		drawRectOutline(miniX - 1, miniY - 1, miniW + 2, miniH + 2);
		drawRectOutline(miniX, miniY, miniW, miniH);

		// COMPASS ROSE (static, above the minimap)
		const int compR = 28;                       // radius
		const int compCX = miniX + miniW / 2;        // centred on minimap
		const int compCY = miniY - compR - 10;       // sits just above the minimap

		// Dark background square
		setColor(10, 10, 20);
		fillRect(compCX - compR, compCY - compR, compR * 2, compR * 2);

		// Gold border
		setColor(160, 130, 50);
		drawRectOutline(compCX - compR, compCY - compR, compR * 2, compR * 2);

		// Faint cross-hair lines
		setColor(60, 55, 45);
		fillRect(compCX - 1, compCY - compR + 5, 2, compR * 2 - 10);
		fillRect(compCX - compR + 5, compCY - 1, compR * 2 - 10, 2);

		// Diagonal tick marks at inter-cardinal corners
		setColor(55, 50, 35);
		fillRect(compCX - compR + 5, compCY + compR - 7, 4, 2);
		fillRect(compCX + compR - 9, compCY + compR - 7, 4, 2);
		fillRect(compCX - compR + 5, compCY - compR + 5, 4, 2);
		fillRect(compCX + compR - 9, compCY - compR + 5, 4, 2);

		// N in red, S/E/W in grey  (N = higher world-Y = top of minimap)
		drawTextC(compCX - 3, compCY + compR - 16, "N", 0, 0, 0);
		drawTextC(compCX - 4, compCY + compR - 15, "N", 220, 50, 50);

		drawTextC(compCX - 3, compCY - compR + 4, "S", 0, 0, 0);
		drawTextC(compCX - 4, compCY - compR + 5, "S", 180, 180, 180);

		drawTextC(compCX + compR - 10, compCY - 4, "E", 0, 0, 0);
		drawTextC(compCX + compR - 11, compCY - 3, "E", 180, 180, 180);

		drawTextC(compCX - compR + 3, compCY - 4, "W", 0, 0, 0);
		drawTextC(compCX - compR + 2, compCY - 3, "W", 180, 180, 180);

		// Centre dot
		setColor(200, 160, 60);
		fillRect(compCX - 2, compCY - 2, 4, 4);
	}

	// 7. WORLD BANNER + ENTRANCE PROMPTS + CONTROLS
	if (getSaveRuntime().bannerTimer > 0 && getSaveRuntime().bannerText[0] != '\0') {
		int tx = 400 - (int)std::strlen(getSaveRuntime().bannerText) * 4;
		drawTextC(tx + 1, 321, getSaveRuntime().bannerText, 0, 0, 0);
		drawTextC(tx, 320, getSaveRuntime().bannerText, 255, 215, 0);
	}

	const GameState& data = getGameState();

	if (player && !data.bossDefeated[0]) {
		float dx = player->x - (float)CASTLE_X;
		float dy = player->y - (float)CASTLE_Y;
		if (dx * dx + dy * dy < 100 * 100) {
			if (gPowerups.bossLocationsRevealed) {
				if (countActiveGuards(GUARD_ENCOUNTER_CASTLE) > 0) {
					drawTextC(286, 226, "Defeat the goblin guards first", 0, 0, 0);
					drawTextC(285, 225, "Defeat the goblin guards first", 255, 160, 90);
				}
				else {
					drawTextC(301, 226, "Press E to Enter Castle", 0, 0, 0);
					drawTextC(300, 225, "Press E to Enter Castle", 255, 255, 0);
				}
			}
			else {
				drawTextC(271, 226, "Speak with the Village Elder first", 0, 0, 0);
				drawTextC(270, 225, "Speak with the Village Elder first", 255, 160, 100);
			}
		}
	}

	if (player && !data.bossDefeated[1]) {
		float dx = player->x - (float)SANCTUM_X;
		float dy = player->y - (float)SANCTUM_Y;
		if (dx * dx + dy * dy < 100 * 100) {
			if (data.bossDefeated[0]) {
				if (countActiveGuards(GUARD_ENCOUNTER_SANCTUM) > 0) {
					drawTextC(282, 226, "Defeat the sanctum acolytes first", 0, 0, 0);
					drawTextC(281, 225, "Defeat the sanctum acolytes first", 220, 120, 220);
				}
				else {
					drawTextC(301, 226, "Press E to Enter The Sanctum", 0, 0, 0);
					drawTextC(300, 225, "Press E to Enter The Sanctum", 200, 0, 200);
				}
			}
			else {
				drawTextC(281, 226, "Defeat the Castle Knight first", 0, 0, 0);
				drawTextC(280, 225, "Defeat the Castle Knight first", 255, 160, 100);
			}
		}
	}

	if (player && !data.bossDefeated[2]) {
		float dx = player->x - (float)THRONE_X;
		float dy = player->y - (float)THRONE_Y;
		if (dx * dx + dy * dy < 100 * 100) {
			if (data.bossDefeated[1]) {
				if (countActiveGuards(GUARD_ENCOUNTER_THRONE) > 0) {
					drawTextC(286, 226, "Defeat the throne guards first", 0, 0, 0);
					drawTextC(285, 225, "Defeat the throne guards first", 255, 120, 160);
				}
				else {
					drawTextC(301, 226, "Press E to Enter The Throne Room", 0, 0, 0);
					drawTextC(300, 225, "Press E to Enter The Throne Room", 150, 0, 255);
				}
			}
			else {
				drawTextC(271, 226, "Defeat the Sanctum Sorcerer first", 0, 0, 0);
				drawTextC(270, 225, "Defeat the Sanctum Sorcerer first", 180, 140, 255);
			}
		}
	}

	drawTextC(11, 11, "WASD=Move  SPACE=Attack  X=Block  E=Enter  Esc=Pause  I=Scroll", 0, 0, 0);
	drawTextC(10, 10, "WASD=Move  SPACE=Attack  X=Block  E=Enter  Esc=Pause  I=Scroll", 200, 200, 200);

	// 8. QUEST SCROLL OVERLAY
	if (questScrollVisible) {
		drawQuestScroll();
	}
}