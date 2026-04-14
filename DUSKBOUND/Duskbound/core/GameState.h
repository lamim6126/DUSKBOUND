#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "GameConfig.h"
#include <cstring>

// GAME STATE
// Central runtime progression state shared by the overworld,
// boss fights, save system and quest scroll.
struct GameState {
    char displayName[64];
    char lastSaved[32];

    float playerX;
    float playerY;

    int powerupSpeed;
    int powerupDamage;
    int powerupDefense;
    int bossLocationsRevealed;

    int collectedPowerups[3];
	int npcDialogueCompleted[8];
	int npcCheckpoint[8];
	int fruitQuestState;
    int bossDefeated[3];
	int totalScore;

    int endingSeen;
};

// copyFixedString
// Copies a C-string into a fixed-size buffer and always keeps
// the destination null-terminated.
template <size_t N>
inline void copyFixedString(char (&dest)[N], const char* src) {
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t i = 0;
    for (; i + 1 < N && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

// Shared in-memory game state.
inline GameState& getGameState() {
    static GameState state;
    return state;
}

// Reset every progression field to the default new-game values.
inline void resetGameStateToDefaults(GameState& state) {
    std::memset(&state, 0, sizeof(GameState));
    copyFixedString(state.displayName, "New Journey");
    copyFixedString(state.lastSaved, "Never");
	state.playerX = 2500.0f;
	state.playerY = 2000.0f;
	state.totalScore = 0;
}

#endif
