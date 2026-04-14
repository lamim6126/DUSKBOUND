#ifndef SAVESYSTEM_H
#define SAVESYSTEM_H

#include "GameConfig.h"
#include "GameState.h"

#include <windows.h>
#include <direct.h>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

// SAVE FILE FORMAT
static const int SAVE_FILE_MAGIC = 0x44534244;   // 'DBSD'
static const int SAVE_FILE_VERSION = 4;
static const int SAVE_NAME_CAPACITY = 64;
static const int SAVE_TIME_CAPACITY = 32;

// Serialized data written to disk.
struct SaveData {
    int magic;
    int version;

    char displayName[SAVE_NAME_CAPACITY];
    char lastSaved[SAVE_TIME_CAPACITY];

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

// Lightweight info used by the load-save menu.
struct SaveSlotInfo {
    char filePath[MAX_PATH];
    char fileName[MAX_PATH];
    char displayName[SAVE_NAME_CAPACITY];
    char lastSaved[SAVE_TIME_CAPACITY];
};

// Runtime-only state that should not be written to disk.
struct SaveRuntime {
    char activePath[MAX_PATH];
    int hasActiveSave;
    int pendingWorldSync;

    // One-shot ambience / encounter flags that should survive
    // scene swaps during the current play session.
    int skeletonIntroSeen;
    int oldGuardianIntroSeen;
    int skeletonLighterIntroSeen;
    int childRescueIntroSeen;
    int hordeIntroSeen[OVERWORLD_HORDE_COUNT];

    char bannerText[64];
    int bannerTimer;
};

// Shared singletons
inline SaveData& getSaveData() {
    static SaveData data;
    return data;
}

inline SaveRuntime& getSaveRuntime() {
    static SaveRuntime runtime;
    return runtime;
}

// safeStringCopy
// MSVC-safe fixed-size string copy helper.
inline void safeStringCopy(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    if (!src) src = "";
#if defined(_MSC_VER)
    strncpy_s(dst, cap, src, _TRUNCATE);
#else
    std::strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
#endif
}

// Save/GameState sync helpers
inline void syncGameStateFromSaveData() {
    const SaveData& data = getSaveData();
    GameState& state = getGameState();

    safeStringCopy(state.displayName, sizeof(state.displayName), data.displayName);
    safeStringCopy(state.lastSaved, sizeof(state.lastSaved), data.lastSaved);

    state.playerX = data.playerX;
    state.playerY = data.playerY;

    state.powerupSpeed = data.powerupSpeed;
    state.powerupDamage = data.powerupDamage;
    state.powerupDefense = data.powerupDefense;
    state.bossLocationsRevealed = data.bossLocationsRevealed;

    for (int i = 0; i < 3; i++) {
        state.collectedPowerups[i] = data.collectedPowerups[i];
        state.bossDefeated[i] = data.bossDefeated[i];
    }

    for (int i = 0; i < 8; i++) {
        state.npcDialogueCompleted[i] = data.npcDialogueCompleted[i];
        state.npcCheckpoint[i] = data.npcCheckpoint[i];
    }

	state.fruitQuestState = data.fruitQuestState;
    state.endingSeen = data.endingSeen;
	state.totalScore = data.totalScore;
}

inline void syncSaveDataFromGameState() {
    SaveData& data = getSaveData();
    const GameState& state = getGameState();

    safeStringCopy(data.displayName, sizeof(data.displayName), state.displayName);
    safeStringCopy(data.lastSaved, sizeof(data.lastSaved), state.lastSaved);

    data.playerX = state.playerX;
    data.playerY = state.playerY;

    data.powerupSpeed = state.powerupSpeed;
    data.powerupDamage = state.powerupDamage;
    data.powerupDefense = state.powerupDefense;
    data.bossLocationsRevealed = state.bossLocationsRevealed;

    for (int i = 0; i < 3; i++) {
        data.collectedPowerups[i] = state.collectedPowerups[i];
        data.bossDefeated[i] = state.bossDefeated[i];
    }

    for (int i = 0; i < 8; i++) {
        data.npcDialogueCompleted[i] = state.npcDialogueCompleted[i];
        data.npcCheckpoint[i] = state.npcCheckpoint[i];
    }

	data.fruitQuestState = state.fruitQuestState;
    data.endingSeen = state.endingSeen;
	data.totalScore = state.totalScore;
}

// Directory + timestamp helpers
inline void ensureSaveDirectory() {
    _mkdir("saves");
}

inline void buildNowStrings(char* fileStamp, size_t fileCap,
                            char* displayStamp, size_t displayCap,
                            char* prettyStamp, size_t prettyCap) {
    time_t now = time(NULL);
    struct tm tmNow;
#if defined(_MSC_VER)
    localtime_s(&tmNow, &now);
#else
    tmNow = *localtime(&now);
#endif

    if (fileStamp && fileCap > 0) {
        strftime(fileStamp, fileCap, "%Y%m%d_%H%M%S", &tmNow);
    }
    if (displayStamp && displayCap > 0) {
        strftime(displayStamp, displayCap, "%Y-%m-%d %H:%M:%S", &tmNow);
    }
    if (prettyStamp && prettyCap > 0) {
        strftime(prettyStamp, prettyCap, "Journey %Y-%m-%d %H:%M", &tmNow);
    }
}

inline void resetSaveDataToDefaults(SaveData& data) {
    std::memset(&data, 0, sizeof(SaveData));
    data.magic = SAVE_FILE_MAGIC;
    data.version = SAVE_FILE_VERSION;
    data.playerX = 2500.0f;
    data.playerY = 2000.0f;

    safeStringCopy(data.displayName, sizeof(data.displayName), "New Journey");
    safeStringCopy(data.lastSaved, sizeof(data.lastSaved), "Never");

    resetGameStateToDefaults(getGameState());
    syncGameStateFromSaveData();
}

inline bool writeSaveFileRaw(const char* path, const SaveData& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(&data), sizeof(SaveData));
    return out.good();
}

inline bool readSaveFileRaw(const char* path, SaveData& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    SaveData temp;
    std::memset(&temp, 0, sizeof(SaveData));
    in.read(reinterpret_cast<char*>(&temp), sizeof(SaveData));
    if (!in.good() && !in.eof()) return false;
    if ((int)in.gcount() != (int)sizeof(SaveData)) return false;
    if (temp.magic != SAVE_FILE_MAGIC) return false;
    if (temp.version != SAVE_FILE_VERSION) return false;

    data = temp;
    return true;
}

inline void stampSaveData(SaveData& data) {
    char fileStamp[32] = { 0 };
    char displayStamp[32] = { 0 };
    char prettyStamp[64] = { 0 };
    buildNowStrings(fileStamp, sizeof(fileStamp), displayStamp, sizeof(displayStamp), prettyStamp, sizeof(prettyStamp));
    safeStringCopy(data.lastSaved, sizeof(data.lastSaved), displayStamp);
}

// Runtime helpers
inline void clearWorldBanner() {
    SaveRuntime& runtime = getSaveRuntime();
    runtime.bannerText[0] = '\0';
    runtime.bannerTimer = 0;
}

inline void setWorldBanner(const char* text, int timer) {
    SaveRuntime& runtime = getSaveRuntime();
    safeStringCopy(runtime.bannerText, sizeof(runtime.bannerText), text);
    runtime.bannerTimer = timer;
}

inline void resetSaveRuntime() {
    SaveRuntime& runtime = getSaveRuntime();
    std::memset(&runtime, 0, sizeof(SaveRuntime));
}

inline int hasActiveSaveSlot() {
    return getSaveRuntime().hasActiveSave;
}

inline const char* getActiveSavePath() {
    return getSaveRuntime().activePath;
}

inline bool consumePendingWorldSync() {
    SaveRuntime& runtime = getSaveRuntime();
    if (!runtime.pendingWorldSync) return false;
    runtime.pendingWorldSync = 0;
    return true;
}

// Public save-slot API
inline bool saveActiveGame() {
    SaveRuntime& runtime = getSaveRuntime();
    SaveData& data = getSaveData();

    if (!runtime.hasActiveSave || runtime.activePath[0] == '\0') return false;

    syncSaveDataFromGameState();
    stampSaveData(data);
    syncGameStateFromSaveData();
    return writeSaveFileRaw(runtime.activePath, data);
}

inline bool beginNewSaveSlot() {
    ensureSaveDirectory();

    SaveData& data = getSaveData();
    SaveRuntime& runtime = getSaveRuntime();

    resetSaveDataToDefaults(data);

    char fileStamp[32] = { 0 };
    char displayStamp[32] = { 0 };
    char prettyStamp[64] = { 0 };
    buildNowStrings(fileStamp, sizeof(fileStamp), displayStamp, sizeof(displayStamp), prettyStamp, sizeof(prettyStamp));

    char path[MAX_PATH] = { 0 };
    sprintf_s(path, sizeof(path), "saves\\save_%s.dsv", fileStamp);

    safeStringCopy(data.displayName, sizeof(data.displayName), prettyStamp);
    safeStringCopy(data.lastSaved, sizeof(data.lastSaved), displayStamp);

    safeStringCopy(runtime.activePath, sizeof(runtime.activePath), path);
    runtime.hasActiveSave = 1;
    runtime.pendingWorldSync = 1;
    runtime.skeletonIntroSeen = 0;
    runtime.oldGuardianIntroSeen = 0;
    runtime.skeletonLighterIntroSeen = 0;
    runtime.childRescueIntroSeen = 0;
    std::memset(runtime.hordeIntroSeen, 0, sizeof(runtime.hordeIntroSeen));
    clearWorldBanner();

    syncGameStateFromSaveData();
    return writeSaveFileRaw(path, data);
}

inline bool loadSaveSlot(const char* path) {
    SaveData temp;
    if (!readSaveFileRaw(path, temp)) return false;

    SaveData& data = getSaveData();
    SaveRuntime& runtime = getSaveRuntime();
    data = temp;

    safeStringCopy(runtime.activePath, sizeof(runtime.activePath), path);
    runtime.hasActiveSave = 1;
    runtime.pendingWorldSync = 1;
    runtime.skeletonIntroSeen = 0;
    runtime.oldGuardianIntroSeen = 0;
    runtime.skeletonLighterIntroSeen = 0;
    runtime.childRescueIntroSeen = 0;
    std::memset(runtime.hordeIntroSeen, 0, sizeof(runtime.hordeIntroSeen));
    clearWorldBanner();

    syncGameStateFromSaveData();
    return true;
}

inline bool renameSaveDisplayName(const char* path, const char* newName) {
    SaveData temp;
    if (!readSaveFileRaw(path, temp)) return false;

    safeStringCopy(temp.displayName, sizeof(temp.displayName), newName);
    stampSaveData(temp);
    if (!writeSaveFileRaw(path, temp)) return false;

    SaveRuntime& runtime = getSaveRuntime();
    SaveData& active = getSaveData();
    if (runtime.hasActiveSave && std::strcmp(runtime.activePath, path) == 0) {
        active = temp;
        syncGameStateFromSaveData();
    }
    return true;
}


inline bool deleteSaveSlot(const char* path) {
    if (!path || !path[0]) return false;

    SaveRuntime& runtime = getSaveRuntime();

    // If the deleted slot is the currently active one, clear the runtime
    // binding so the next "New Game" starts from a fresh slot.
    bool deletingActive = runtime.hasActiveSave &&
                          (std::strcmp(runtime.activePath, path) == 0);

    if (!DeleteFileA(path)) {
        return false;
    }

    if (deletingActive) {
        resetSaveRuntime();
        resetSaveDataToDefaults(getSaveData());
        resetGameStateToDefaults(getGameState());
    }

    return true;
}

inline bool listSaveSlots(std::vector<SaveSlotInfo>& outSlots) {
    ensureSaveDirectory();
    outSlots.clear();

    WIN32_FIND_DATAA findData;
    HANDLE handle = FindFirstFileA("saves\\*.dsv", &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        char fullPath[MAX_PATH] = { 0 };
        sprintf_s(fullPath, sizeof(fullPath), "saves\\%s", findData.cFileName);

        SaveData temp;
        if (!readSaveFileRaw(fullPath, temp)) {
            continue;
        }

        SaveSlotInfo info;
        std::memset(&info, 0, sizeof(SaveSlotInfo));
        safeStringCopy(info.filePath, sizeof(info.filePath), fullPath);
        safeStringCopy(info.fileName, sizeof(info.fileName), findData.cFileName);
        safeStringCopy(info.displayName, sizeof(info.displayName), temp.displayName);
        safeStringCopy(info.lastSaved, sizeof(info.lastSaved), temp.lastSaved);
        outSlots.push_back(info);
    } while (FindNextFileA(handle, &findData));

    FindClose(handle);

    std::sort(outSlots.begin(), outSlots.end(),
        [](const SaveSlotInfo& a, const SaveSlotInfo& b) {
            return std::strcmp(a.lastSaved, b.lastSaved) > 0;
        });

    return !outSlots.empty();
}

#endif
