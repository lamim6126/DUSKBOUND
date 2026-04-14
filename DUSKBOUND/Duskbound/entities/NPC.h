#ifndef NPC_H
#define NPC_H

#include "../systems/DialogueSystem.h"

// NPC  -  A single non-player character

// ---- Display size of NPC sprites in world space ----
// Tweak these to match your actual PNG dimensions.
static const int NPC_SPRITE_W = 32;
static const int NPC_SPRITE_H = 32;

struct NPC {

	// World position & collision dimensions
	float x, y;   // Bottom-left world corner (matches player convention)
	int   w, h;   // Collision width / height (32x32)

	// Fallback colour (shown if PNG fails to load)
	int colorR, colorG, colorB;

	// Identity
	const char* name;   // Displayed as NPC title in dialogue box

	// Dialogue data
	DialogueTree tree;

	// Runtime state  (reset each time a conversation starts)
	bool inConversation;
	int  currentNode;
	int  checkpointNode;
	bool dialogueCompleted;
	bool hidePortrait;

	// PNG sprite animation (2-frame idle cycle)
	// tex[0] and tex[1] are loaded once by NPCManager.
	// animFrame toggles 0/1 every NPC_ANIM_PERIOD frames.
	static const int NPC_ANIM_PERIOD = 30;  // frames per idle frame

	int tex[2];       // Texture IDs: -1 = not loaded (use fallback rect)
	int animFrame;    // 0 or 1 - current idle frame
	int animTimer;    // counts up to NPC_ANIM_PERIOD then flips animFrame

	// Advance the idle animation - call every frame from NPCManager::update()
	void updateAnim();

	// ------ helpers ------

	// True when player world position is within radius
	bool isNearPlayer(float px, float py, float radius = 80.0f) const;

	// Start / end helpers called by NPCManager
	void beginDialogue();
	void endDialogue();

	// Returns false when the conversation is over
	// idx is 0-based (key '1' -> idx 0, etc.)
	bool selectResponse(int idx);

	// ------ rendering ------

	// Draw NPC sprite in world space.
	// Uses tex[animFrame] if loaded, else coloured rectangle fallback.
	void drawInWorld(float camX, float camY) const;

	// Draw "Press E to talk" prompt above NPC (world-space)
	void drawPrompt(float camX, float camY) const;

	// Draw the dialogue overlay (bottom of screen)
	// Only call when inConversation == true
	void drawDialogueUI() const;
};

// NPCManager  -  owns all 4 NPCs, drives interaction
class NPCManager {
public:
	static const int NPC_COUNT = 8;

	NPCManager();

	// Call every frame.
	// Returns true while a dialogue is active (so caller can lock player).
	bool update(float playerX, float playerY);

	// Draw NPC sprites + prompts in world space
	void drawWorld(float camX, float camY) const;

	// Draw dialogue overlay (call last, on top of everything)
	void drawUI() const;

	bool isInDialogue() const { return activeIdx >= 0; }

	void drawPrompts(float playerX, float playerY, float camX, float camY) const;

	bool isDialogueComplete(int idx) const {
		if (idx < 0 || idx >= NPC_COUNT) return false;
		return npcs[idx].dialogueCompleted;
	}

    void applyProgress(const int completed[NPC_COUNT], const int checkpoints[NPC_COUNT]);
    void copyProgress(int completed[NPC_COUNT], int checkpoints[NPC_COUNT]) const;
    bool consumeConversationFinished();

private:
	NPC npcs[NPC_COUNT];
	int activeIdx;      // index into npcs[], or -1
	int inputCooldown;
	bool conversationFinishedThisFrame;

	// Load 2 PNG frames for one NPC into npc.tex[0] and npc.tex[1]
	// spriteName must match the file prefix e.g. "oldman" -> oldman_0.png
	void loadNPCSprites(NPC& npc, const char* spriteName);

	// One builder per NPC - fill in position, name, dialogue tree
	void buildNPC1(NPC& npc);   // Village Elder  - central village
	void buildNPC2(NPC& npc);   // Lumberjack     - northwest forest
	void buildNPC3(NPC& npc);   // Escaped Slave  - near castle
	void buildNPC4(NPC& npc);   // Retired Knight - south of map
	void buildNPC5(NPC& npc);   // Herb Doctor 
	void buildNPC6(NPC& npc);   // Hurt Fighter
	void buildNPC7(NPC& npc);   // Mad Man
	void buildNPC8(NPC& npc);	// Distressed Child
};

#endif // NPC_H