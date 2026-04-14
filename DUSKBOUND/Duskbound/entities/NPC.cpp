#include "NPC.h"
#include "../core/Renderer.h"   
#include "../core/Input.h"      
#include "../core/GameConfig.h" 
#include "../systems/Powerups.h"
#include "../core/GameState.h"
#include <cmath>
#include <cstring>
#include <cstdio>

static const int UI_BOX_X = 0;
static const int UI_BOX_Y = 0;
static const int UI_BOX_W = 800;
static const int UI_BOX_H = 185;

static const int PORTRAIT_X = 10;
static const int PORTRAIT_Y = 75;
static const int PORTRAIT_W = 80;
static const int PORTRAIT_H = 100;

static const int TEXT_X = 105;   // right of portrait
static const int NAME_Y = 170;
static const int LINE1_Y = 152;
static const int LINE2_Y = 135;
static const int LINE3_Y = 118;
static const int DIVIDER_Y = 108;

static const int RESP_START_Y = 96;    // first response row
static const int RESP_GAP = 18;    // pixels between response rows

// Prompt drawn above NPC in world space
static const int PROMPT_OFFSET_Y = 10; // pixels above NPC top

// Cycles animFrame between 0 and 1 every NPC_ANIM_PERIOD frames.
// Call every frame from NPCManager::update().
void NPC::updateAnim() {
	animTimer++;
	if (animTimer >= NPC_ANIM_PERIOD) {
		animTimer = 0;
		animFrame = 1 - animFrame;  // toggle 0 <-> 1
	}
}

// NPC::isNearPlayer
bool NPC::isNearPlayer(float px, float py, float radius) const {
	float cx = x + w * 0.5f;
	float cy = y + h * 0.5f;
	float dx = px - cx;
	float dy = py - cy;
	return (dx * dx + dy * dy) < (radius * radius);
}

// NPC::beginDialogue / endDialogue
void NPC::beginDialogue() {
	inConversation = true;
	currentNode = checkpointNode;   // resume from last checkpoint (node 0 on first talk)
}

void NPC::endDialogue() {
	inConversation = false;
	checkpointNode = currentNode;
	currentNode = 0;
}

// Returns false when the conversation is over.
// idx is 0-based (key '1' -> idx 0, etc.)
bool NPC::selectResponse(int idx) {
	if (currentNode < 0 || currentNode >= tree.nodeCount) {
		endDialogue();
		return false;
	}

	DialogueNode& node = tree.nodes[currentNode];

	if (idx < 0 || idx >= node.responseCount) return true; // invalid key, stay

	int next = node.responses[idx].nextNode;

	if (next < 0 || next >= tree.nodeCount) {
		// -1 or out of range -> end conversation
		endDialogue();
		return false;
	}

	currentNode = next;
	return true; // still talking
}

// Draws the NPC sprite at their world position using the current
// idle animation frame.  Falls back to a coloured rectangle if
// the texture failed to load (tex[x] == -1).
void NPC::drawInWorld(float camX, float camY) const {
	int sx = (int)(x - camX);
	int sy = (int)(y - camY);

	// Off-screen cull using display size, not collision size
	if (sx + NPC_SPRITE_W < 0 || sx > SCREEN_WIDTH)  return;
	if (sy + NPC_SPRITE_H < 0 || sy > SCREEN_HEIGHT) return;

	int texId = tex[animFrame];

	if (texId >= 0) {
		// Draw PNG sprite - centred horizontally on the collision box
		int offsetX = (NPC_SPRITE_W - w) / 2;
		drawImage(sx - offsetX, sy, NPC_SPRITE_W, NPC_SPRITE_H, texId);
	}
	else {
		// Fallback: coloured rectangle (original placeholder rendering)
		// Outline (slightly darker shade)
		setColor(colorR / 2, colorG / 2, colorB / 2);
		fillRect(sx, sy, w, h);

		// Main body colour
		setColor(colorR, colorG, colorB);
		fillRect(sx + 2, sy + 2, w - 4, h - 4);

		// Simple "head" circle approximation - two rectangles
		int headW = w / 3;
		int headH = h / 4;
		int headX = sx + (w - headW) / 2;
		int headY = sy + h - headH - 2;
		setColor(230, 200, 170);   // skin tone placeholder
		fillRect(headX, headY, headW, headH);
	}
}

// "Press E to talk" hint above the NPC in world space.
void NPC::drawPrompt(float camX, float camY) const {
	int sx = (int)(x - camX) + w / 2 - 60;
	int sy = (int)(y - camY) + NPC_SPRITE_H + PROMPT_OFFSET_Y;

	if (sy > SCREEN_HEIGHT + 20) return; // off top of screen

	// Shadow
	drawTextC(sx + 1, sy - 1, "Press E to talk", 0, 0, 0);
	// Text
	drawTextC(sx, sy, "Press E to talk", 255, 255, 100);
}

// Bottom-of-screen Fallout-style dialogue panel.
// Portrait slot uses tex[0] (the first idle frame PNG).
// Falls back to coloured rectangle if texture not loaded.
void NPC::drawDialogueUI() const {
	if (!inConversation) return;
	if (currentNode < 0 || currentNode >= tree.nodeCount) return;

	const DialogueNode& node = tree.nodes[currentNode];

	// ---- Dark background panel ----
	setColor(8, 8, 18);
	fillRect(UI_BOX_X, UI_BOX_Y, UI_BOX_W, UI_BOX_H);

	// Panel border (gold tint matches the boss UI style)
	setColor(160, 130, 60);
	drawRectOutline(UI_BOX_X, UI_BOX_Y, UI_BOX_W, UI_BOX_H);

	// ---- NPC Portrait ----
	// Use tex[0] (idle frame 0) as the portrait if loaded
	if (!hidePortrait) {
		if (tex[0] >= 0) {
			drawImage(PORTRAIT_X, PORTRAIT_Y, PORTRAIT_W, PORTRAIT_H, tex[0]);
			// Gold border over the portrait
			setColor(160, 130, 60);
			drawRectOutline(PORTRAIT_X, PORTRAIT_Y, PORTRAIT_W, PORTRAIT_H);
		}
		else {
			// Fallback: coloured rectangle portrait
			setColor(colorR / 2, colorG / 2, colorB / 2);
			fillRect(PORTRAIT_X, PORTRAIT_Y, PORTRAIT_W, PORTRAIT_H);
			setColor(colorR, colorG, colorB);
			fillRect(PORTRAIT_X + 2, PORTRAIT_Y + 2, PORTRAIT_W - 4, PORTRAIT_H - 4);
			// Portrait border
			setColor(160, 130, 60);
			drawRectOutline(PORTRAIT_X, PORTRAIT_Y, PORTRAIT_W, PORTRAIT_H);
		}
	}

	// ---- NPC Name ----
	int nameX = hidePortrait ? 15 : TEXT_X;
	int textX = nameX;   // reuse for lines + responses below
	drawTextC(nameX + 1, NAME_Y - 1, name, 0, 0, 0);
	drawTextC(nameX, NAME_Y, name, 255, 210, 80);

	// ---- NPC Speech lines ----
	if (node.npcLine1 && node.npcLine1[0] != '\0') {
		drawTextC(textX + 1, LINE1_Y - 1, node.npcLine1, 0, 0, 0);
		drawTextC(textX, LINE1_Y, node.npcLine1, 220, 220, 220);
	}
	if (node.npcLine2 && node.npcLine2[0] != '\0') {
		drawTextC(textX + 1, LINE2_Y - 1, node.npcLine2, 0, 0, 0);
		drawTextC(textX, LINE2_Y, node.npcLine2, 200, 200, 200);
	}
	if (node.npcLine3 && node.npcLine3[0] != '\0') {
		drawTextC(textX + 1, LINE3_Y - 1, node.npcLine3, 0, 0, 0);
		drawTextC(textX, LINE3_Y, node.npcLine3, 180, 180, 180);
	}

	// ---- Divider line ----
	setColor(80, 65, 30);
	fillRect(textX, DIVIDER_Y - 1, UI_BOX_W - textX - 10, 1);

	// ---- Player Response Options ----
	for (int i = 0; i < node.responseCount; i++) {
		int ry = RESP_START_Y - i * RESP_GAP;

		// Build label "[1] text..."
		char label[128];
		sprintf_s(label, sizeof(label), "[%d] %s", i + 1, node.responses[i].text);

		// Shadow
		drawTextC(textX + 1, ry - 1, label, 0, 0, 0);
		// Dim yellow for selectable options
		drawTextC(textX, ry, label, 180, 180, 80);
	}

	// ---- Hint ----
	drawTextC(UI_BOX_W - 175, 5, "Press number to reply", 60, 60, 60);
}


// NPCManager - constructor
// Builds all 4 NPCs then loads their PNG sprites.
NPCManager::NPCManager() : activeIdx(-1), inputCooldown(0), conversationFinishedThisFrame(false) {
	buildNPC1(npcs[0]);
	buildNPC2(npcs[1]);
	buildNPC3(npcs[2]);
	buildNPC4(npcs[3]);
	buildNPC5(npcs[4]);
	buildNPC6(npcs[5]);
	buildNPC7(npcs[6]);
	buildNPC8(npcs[7]);
	

	// Load PNG sprites - names match assets/images/npc/{name}_0.png
	// NPC1 = oldman, NPC2 = lumberjack, NPC3 = slave, NPC4 = retiredknight
	loadNPCSprites(npcs[0], "oldman");
	loadNPCSprites(npcs[1], "lumberjack");
	loadNPCSprites(npcs[2], "slave");
	loadNPCSprites(npcs[3], "retiredknight");
	loadNPCSprites(npcs[4], "herbdoctor");
	loadNPCSprites(npcs[5], "hurtknight");
	loadNPCSprites(npcs[6], "madman");
	loadNPCSprites(npcs[7], "distressedchild");
}

// Loads {spriteName}_0.png and {spriteName}_1.png from
// assets/images/npc/ into npc.tex[0] and npc.tex[1].
// Missing files produce -1; drawInWorld() falls back to rects.
static int tryLoadNPCSpriteFrame(const char* spriteName, int frameIndex) {
	char path[256];

	// Prefer the new 00 / 01 placeholder naming.
	sprintf_s(path, sizeof(path),
		"assets/images/npc/%s_%02d.png", spriteName, frameIndex);
	int tex = loadImage(path);
	if (tex >= 0) return tex;

	// Fall back to the older 0 / 1 naming used by the original NPCs.
	sprintf_s(path, sizeof(path),
		"assets/images/npc/%s_%d.png", spriteName, frameIndex);
	return loadImage(path);
}

void NPCManager::loadNPCSprites(NPC& npc, const char* spriteName) {
	npc.tex[0] = tryLoadNPCSpriteFrame(spriteName, 0);
	npc.tex[1] = tryLoadNPCSpriteFrame(spriteName, 1);

	printf("  NPC '%s' sprites: frame0=%d, frame1=%d\n",
		spriteName, npc.tex[0], npc.tex[1]);
}

// Call every frame from PlayScene::update().
// Returns true while a dialogue is active (so caller can lock player).
bool NPCManager::update(float playerX, float playerY) {

	// Reset the one-frame conversation-finished flag at the start
	// of each update. PlayScene can consume it after update() runs.
	conversationFinishedThisFrame = false;

	// ---- Always advance idle animations ----
	for (int i = 0; i < NPC_COUNT; i++) {
		npcs[i].updateAnim();
	}

	// ---- Active dialogue: handle response input ----
	if (activeIdx >= 0) {
		NPC& npc = npcs[activeIdx];

		if (!npc.inConversation) {
			conversationFinishedThisFrame = true;
			activeIdx = -1;
			return false;
		}

		if (inputCooldown > 0) {
			inputCooldown--;
			return true;
		}

		for (int i = 0; i < MAX_RESPONSES; i++) {
			unsigned char key = (unsigned char)('1' + i);
			if (consumeKey(key)) {
				inputCooldown = 15;
				bool still = npc.selectResponse(i);
				if (!still) {
					if (activeIdx == 0 && npc.checkpointNode == 10) {
						gPowerups.bossLocationsRevealed = true;
					}
					if (activeIdx == 1 && npc.checkpointNode == 9) {
						npc.dialogueCompleted = true;
					}
					if (activeIdx == 2 && npc.checkpointNode == 6) {
						npc.dialogueCompleted = true;
					}
					if (activeIdx == 3 && npc.checkpointNode == 10) {
						npc.dialogueCompleted = true;
					}
					// Elara (npcs[4]): node 1 end -> quest accepted, spawn fruits
					if (activeIdx == 4 && npc.checkpointNode == 1) {
						GameState& gs = getGameState();
						if (gs.fruitQuestState == 0) gs.fruitQuestState = 1;
					}
					// Elara (npcs[4]): node 3 end -> quest complete, grant HP bonus
					if (activeIdx == 4 && npc.checkpointNode == 3) {
						GameState& gs = getGameState();
						if (gs.fruitQuestState == 2) {
							gs.fruitQuestState = 3;
							npc.dialogueCompleted = true;
						}
					}
					conversationFinishedThisFrame = true;
					activeIdx = -1;
				}
				return still;
			}
		}

		// ESC closes dialogue - no reveal since this is always an early exit
		if (consumeKey(27)) {
			npc.endDialogue();
			conversationFinishedThisFrame = true;
			activeIdx = -1;
			return false;
		}

		return true;
	}

	// ---- No active dialogue: check for proximity + E press ----
	// Check proximity WITHOUT consuming the key first
	int nearbyIdx = -1;
	for (int i = 0; i < NPC_COUNT; i++) {
		if (npcs[i].isNearPlayer(playerX, playerY)) {
			nearbyIdx = i;
			break;
		}
	}

	// Only consume E if there is actually someone nearby to talk to
	if (nearbyIdx >= 0 && (consumeKey('e') || consumeKey('E'))) {
		// Elara (npcs[4]): if quest was accepted AND fruits collected,
		// redirect to the reward node before opening dialogue.
		if (nearbyIdx == 4) {
			GameState& gs = getGameState();
			if (gs.fruitQuestState == 2 && npcs[4].checkpointNode == 1) {
				npcs[4].checkpointNode = 2;   // jump straight to reward node
			}
		}
		npcs[nearbyIdx].beginDialogue();
		activeIdx = nearbyIdx;
		return true;
	}

	return false;
}


// Restores the saved dialogue completion flags and checkpoint
// nodes for all NPCs after a save is loaded.
void NPCManager::applyProgress(const int completed[NPC_COUNT], const int checkpoints[NPC_COUNT]) {
	for (int i = 0; i < NPC_COUNT; i++) {
		npcs[i].dialogueCompleted = (completed[i] != 0);
		npcs[i].checkpointNode = checkpoints[i];
		npcs[i].inConversation = false;
		npcs[i].currentNode = 0;
	}
}

// Copies the live NPC progression data into plain integer arrays
// so the save system can serialize them easily.
void NPCManager::copyProgress(int completed[NPC_COUNT], int checkpoints[NPC_COUNT]) const {
	for (int i = 0; i < NPC_COUNT; i++) {
		completed[i] = npcs[i].dialogueCompleted ? 1 : 0;
		checkpoints[i] = npcs[i].checkpointNode;
	}
}

// Returns true only once after a conversation has just ended.
bool NPCManager::consumeConversationFinished() {
	if (!conversationFinishedThisFrame) return false;
	conversationFinishedThisFrame = false;
	return true;
}

// Draw NPC sprites in world space (called from PlayScene::draw).
// Prompts are drawn separately via drawPrompts() so player
// position can be passed in for proximity checks.
void NPCManager::drawWorld(float camX, float camY) const {
	for (int i = 0; i < NPC_COUNT; i++) {
		npcs[i].drawInWorld(camX, camY);
	}
}

// Draw dialogue overlay on top of everything (call last).
void NPCManager::drawUI() const {
	if (activeIdx < 0) return;
	npcs[activeIdx].drawDialogueUI();
}

// Draw "Press E to talk" prompts near the player only.
// Call from PlayScene::draw() after drawWorld():
// npcManager->drawPrompts(player->x, player->y, cam.x, cam.y);
void NPCManager::drawPrompts(float playerX, float playerY, float camX, float camY) const {
	if (activeIdx >= 0) return;  // hide prompt while already in dialogue

	for (int i = 0; i < NPC_COUNT; i++) {
		if (npcs[i].isNearPlayer(playerX, playerY)) {
			npcs[i].drawPrompt(camX, camY);
		}
	}
}


// Edit the builders below to customise each NPC.
// Node 0 is ALWAYS the opening greeting.
// Use nextNode == -1 for any response that ends the conversation.
// HELPER MACRO - reduces boilerplate when adding a node.
#define _N(tree)       (tree).nodes[(tree).nodeCount]
#define END_NODE(tree) (tree).nodeCount++

// NPC 1 - Village Elder (oldman)
// Location: central village
// Colour:   steel blue
void NPCManager::buildNPC1(NPC& npc) {
	npc.x = 3082.0f;   // world X
	npc.y = 2150.0f;   // world Y
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 60;
	npc.colorG = 100;
	npc.colorB = 180;
	npc.name = "Village Elder";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	// Sprite fields - textures loaded separately in constructor
	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 : ----
	_N(t).npcLine1 = "Ahoy! Y-you!! What a-are you d-doing here?";
	_N(t).npcLine2 = "W-wait...I sense s-some royalty in you!";
	_N(t).npcLine3 = "Who are y-you?";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Your guess would be correct. I am Prince Xeralt, son of Odeseous.", 1 };
	_N(t).responses[1] = { "I must be going. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 1 : ----
	_N(t).npcLine1 = "My m-my! You truly a-are of noble f-family!!";
	_N(t).npcLine2 = "You h-have shining armor and t-the mighty sword o-of the kings!";
	_N(t).npcLine3 = "What b-brings you here, your h-highness?";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I have come here to fight the vile creatures I heard of.", 2 };
	_N(t).responses[1] = { "I must be going. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 2 : ----
	_N(t).npcLine1 = "A p-prince who f-fights his own b-battles! ";
	_N(t).npcLine2 = "W-what a sight it r-really is!";
	_N(t).npcLine3 = "I am v-very honoured to w-witness this!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I did not expect a village in these lands.", 3 };
	_N(t).responses[1] = { "I have heard enough. Goodbye.", -1 };
	END_NODE(t);

	// ---- NODE 3 : ----
	_N(t).npcLine1 = "O-oh our beloved M-mudsville!";
	_N(t).npcLine2 = "It is c-considered a s-safe place from t-the outside d-dangers.";
	_N(t).npcLine3 = "W-we don not stray f-far away from it.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 4 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 4 : ----
	_N(t).npcLine1 = "Although I s-sometimes think o-f leaving here f-for a fresh view...";
	_N(t).npcLine2 = "B-but an old f-folk like m-me! H-heh heh!!";
	_N(t).npcLine3 = "I c-can barely m-move...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I heard that the creatures are causing havoc here.", 5 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 5 : ----
	_N(t).npcLine1 = "Oh m-my! You would be correct!!";
	_N(t).npcLine2 = "They c-came out of n-nowhere one d-day and c-committed atrocities!.";
	_N(t).npcLine3 = "No o-one knows who t-they are, w-where they came f-from.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 6 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 6 : ----
	_N(t).npcLine1 = "People w-who fought back w-were no match f-for them!";
	_N(t).npcLine2 = "Many g-got killed...the rest r-ran with their l-lives.";
	_N(t).npcLine3 = "Those c-creatures have not f-found us here y-yet...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Do you know where these creatures are?", 7 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 7 : ----
	_N(t).npcLine1 = "O-oh your h-highness! You t-truly are a b-brave one!!";
	_N(t).npcLine2 = "I must say t-that even t-though you are v-very brave,";
	_N(t).npcLine3 = "you have t-to be very c-cautious...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 8 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 8 : ----
	_N(t).npcLine1 = "But if y-you persist, I will t-tell you this.";
	_N(t).npcLine2 = "Rumors have it, a goblin-like b-beast resides i-in a";
	_N(t).npcLine3 = "castle, t-to the north o-of this village.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 9 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 9 : ----
	_N(t).npcLine1 = "There a-are two m-more vile beasts in t-the south and";
	_N(t).npcLine2 = "the west.";
	_N(t).npcLine3 = "I d-do not know a-anything more...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Thank you for letting me know. I shall defeat them.", 10 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 10 ----
	_N(t).npcLine1 = "";
	_N(t).npcLine2 = "May the f-father of h-heavens be with you, P-prince!!";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Farewell.", -1 };
	END_NODE(t);
}


// NPC 2 - Lumberjack (lumberjack)
// Location: northwest forest
// Colour:   burnt orange / guard armour
void NPCManager::buildNPC2(NPC& npc) {
	npc.x = 300.0f;
	npc.y = 4800.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 200;
	npc.colorG = 100;
	npc.colorB = 20;
	npc.name = "Rosevelt";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	// Sprite fields - textures loaded separately in constructor
	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 : ----
	_N(t).npcLine1 = "Oi! Stay back!!";
	_N(t).npcLine2 = "What are you doing here?";
	_N(t).npcLine3 = "Answer me!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I mean no harm. I am Prince Xeralt. Yourself?", 1 };
	_N(t).responses[1] = { "Do not be rash. I will leave.", -1 };
	END_NODE(t);

	// ---- NODE 1 : ----
	_N(t).npcLine1 = "Prince? Heh! You're funny. I have one for you too.";
	_N(t).npcLine2 = "I am Rosevelt, the King of this forest!";
	_N(t).npcLine3 = "Bow down before me!!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I would rather not.", 2 };
	_N(t).responses[1] = { "I will take my leave.", -1 };
	END_NODE(t);

	// ---- NODE 2 : ----
	_N(t).npcLine1 = "Oh you were not joking...not the humorous type, eh?";
	_N(t).npcLine2 = "Well well, you are in my turf, so tell me this.";
	_N(t).npcLine3 = "Why would a prince wander about these woods?";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I am here to defeat the so called 'creatures' roaming about here.", 3 };
	_N(t).responses[1] = { "I will leave now.", -1 };
	END_NODE(t);

	// ---- NODE 3 : ----
	_N(t).npcLine1 = "Defeat the creatures? Ha ha!!";
	_N(t).npcLine2 = "Maybe you are homurous!";
	_N(t).npcLine3 = "Or you have a death wish.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Why do you say that?", 4 };
	_N(t).responses[1] = { "I will leave now.", -1 };
	END_NODE(t);

	// ---- NODE 4 : ----
	_N(t).npcLine1 = "There is no chance of defeating them.";
	_N(t).npcLine2 = "Even with my axe, I dare not fight them!";
	_N(t).npcLine3 = "They are very strong.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Do you know where they came from or what they want?", 5 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 5 : ----
	_N(t).npcLine1 = "Nobody knows for sure. It is all hearsay.";
	_N(t).npcLine2 = "Some say they came from a realm different from ours.";
	_N(t).npcLine3 = "Others say they were created by some powerful diety";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 6 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 6 : ----
	_N(t).npcLine1 = "Their purpose? Could be many.";
	_N(t).npcLine2 = "But one thing is for sure. They are vicious and lethal.";
	_N(t).npcLine3 = "They will tear through anything that crosses their path.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I cannot go back after coming all this way. I must fight.", 7 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 7 : ----
	_N(t).npcLine1 = "Eh, why would I care if you die.";
	_N(t).npcLine2 = "I will tell you this though.";
	_N(t).npcLine3 = "If you must fight, try taking the katari Sword.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "katari Sword?", 8 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 8 : ----
	_N(t).npcLine1 = "It is the magical weapon of an ancient warrior, Tukora.";
	_N(t).npcLine2 = "He defeated the legendary seprent with that weapon.";
	_N(t).npcLine3 = "It has been wedged in the soil for hundreds of years.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 9 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 9 ----
	_N(t).npcLine1 = "Nobody has been able to lift it, as it requires incredible strength.";
	_N(t).npcLine2 = "Go south by following the road. Once you see the island, go to the";
	_N(t).npcLine3 = "centre. Muster all your strength and try to lift the sword.";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Thank you for letting me know. Farewell.", -1 };
	END_NODE(t);
}

// NPC 3 - Escaped Slave (slave)
// Location: near castle
// Colour:   teal / arcane robes
void NPCManager::buildNPC3(NPC& npc) {
	npc.x = 3140.0f;
	npc.y = 4220.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 20;
	npc.colorG = 180;
	npc.colorB = 160;
	npc.name = "Melika";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	// Sprite fields - textures loaded separately in constructor
	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 : ----
	_N(t).npcLine1 = "AH! Please do not hurt me!!";
	_N(t).npcLine2 = "I beg you!";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Calm ypurself. I will not hurt you.", 1 };
	_N(t).responses[1] = { "I do not have time for you.", -1 };
	END_NODE(t);

	// ---- NODE 1 : ----
	_N(t).npcLine1 = "";
	_N(t).npcLine2 = "Oh thank you mister! I really thought this was my end!";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "who are you and why are you in such distress?", 2 };
	_N(t).responses[1] = { "Go away now. I do not have time for you.", -1 };
	END_NODE(t);

	// ---- NODE 2 : ----
	_N(t).npcLine1 = "My name is Melika...";
	_N(t).npcLine2 = "I was trapped in that castle as a slave...";
	_N(t).npcLine3 = "I could not take the burden! Hence I escaped!!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "By whom did you get enslaved?", 3 };
	_N(t).responses[1] = { "I have heard enough. I must move on.", -1 };
	END_NODE(t);

	// ---- NODE 3 : ----
	_N(t).npcLine1 = "The hideous looking goblin monster!";
	_N(t).npcLine2 = "He calls himself the king of the northern lands.";
	_N(t).npcLine3 = "He enslaves and tortures many like me!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Where are you running to?", 4 };
	_N(t).responses[1] = { "I have heard enough. I must move on.", -1 };
	END_NODE(t);

	// ---- NODE 4 : ----
	_N(t).npcLine1 = "I heard that there is a shield with casted spells!";
	_N(t).npcLine2 = "It is near and old outpost of this castle.";
	_N(t).npcLine3 = "If anyone tries to attack me again, I will defend myself with that!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "You are in no shape to fight. I shall get it instead.", 5 };
	_N(t).responses[1] = { "I have heard enough. I must move on.", -1 };
	END_NODE(t);

	// ---- NODE 5 : ----
	_N(t).npcLine1 = "Are you going to fight him?";
	_N(t).npcLine2 = "With all this armor and weapon it seems you are well suited.";
	_N(t).npcLine3 = "I will tell you the location if you promise to defeat him!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I will try with my fullest strenght.", 6 };
	_N(t).responses[1] = { "I do not make promises. I must move on.", -1 };
	END_NODE(t);

	// ---- NODE 6 ----
	_N(t).npcLine1 = "Alright! If you walk east, you will come upon an outpost.";
	_N(t).npcLine2 = "Near it, a glowing heavy shield is rumored to be there.";
	_N(t).npcLine3 = "If you have the strength, you shall have it!";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Thank you. I will try my fullest.", -1 };
	END_NODE(t);
}

// NPC 4 - Retired Knight (retiredknight)
// Location: south of the map
// Colour:   deep violet / ancient
void NPCManager::buildNPC4(NPC& npc) {
	npc.x = 2400.0f;
	npc.y = 670.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 130;
	npc.colorG = 20;
	npc.colorB = 200;
	npc.name = "Maximus";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	// Sprite fields - textures loaded separately in constructor
	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 : ----
	_N(t).npcLine1 = "Prince Xeralt? What an honor!";
	_N(t).npcLine2 = "I was not expecting your presence at all!";
	_N(t).npcLine3 = "The rumors were true indeed!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I am surprised at the fact that you recognized me. Who are you?", 1 };
	_N(t).responses[1] = { "I must move on. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 1 : ----
	_N(t).npcLine1 = "My name is Maximus, your highness. I served King Odeseous for many decades.";
	_N(t).npcLine2 = "I was the 7th Commanding Knight of the king's army.";
	_N(t).npcLine3 = "I have fought many battles for our King...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I have heard of your tales, Maximus. What made you reside here?", 2 };
	_N(t).responses[1] = { "I must move on. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 2 : ----
	_N(t).npcLine1 = "Even a knight needs to know when to stop. So I took my retirement.";
	_N(t).npcLine2 = "I wanted to get away from the chaos and lead a peaceful life";
	_N(t).npcLine3 = "for my remaining days. That is why I chose to stay here.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Are you aware of the dangers lurking upon these lands?", 3 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 3 : ----
	_N(t).npcLine1 = "Oh yes. I am aware, your highness.";
	_N(t).npcLine2 = "In fact, in this very premise, there was one of them.";
	_N(t).npcLine3 = "With all my strength and a bit of luck, I managed to defeat it.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Impressive! What was the creature like?", 4 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 4 : ----
	_N(t).npcLine1 = "It was utterly horrid looking beast! Nothing like I have witnessed before!!";
	_N(t).npcLine2 = "It had an appearance of a werewolf with sharp teeth and claws.";
	_N(t).npcLine3 = "It moved with great speed and precision.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "How had you managed to defeat it?", 5 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 5 : ----
	_N(t).npcLine1 = "I had a magical totem of speed with me, your highness.";
	_N(t).npcLine2 = "With the use of it, I fought hard.";
	_N(t).npcLine3 = "Ultimately, I successfully bested the beast with swiftness.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Do you have the totem with you at this moment?", 6 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 6 : ----
	_N(t).npcLine1 = "It is not with me currently, your highness.";
	_N(t).npcLine2 = "I have heard that you chose the path of a knight in attaining pure strength...";
	_N(t).npcLine3 = "No prince or king did so before.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 7 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 7 : ----
	_N(t).npcLine1 = "In your noble endeavour, I must assist you.";
	_N(t).npcLine2 = "I have have hidden the totem near an outpost in the west direction.";
	_N(t).npcLine3 = "Follow the rocky path and you shall find it.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Do have any advice on how to defeat the creatures?", 8 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 8 : ----
	_N(t).npcLine1 = "I have only fought one of them, your highness!";
	_N(t).npcLine2 = "So my knowledge is very limited.";
	_N(t).npcLine3 = "But I will share it with you.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 9 };
	_N(t).responses[1] = { "I have heard enough. Farewell.", -1 };
	END_NODE(t);

	// ---- NODE 9 : ----
	_N(t).npcLine1 = "Do not get too close to them all the time during battle.";
	_N(t).npcLine2 = "Keep your distance and get close only when ready to strike.";
	_N(t).npcLine3 = "Do not forget to block their strikes with your armor when you must.";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Thank you, Maximus. I must move on now.", 10 };
	END_NODE(t);

	// ---- NODE 10  ----
	_N(t).npcLine1 = "Farewell, your highness!";
	_N(t).npcLine2 = "May the Prince live long and strong!!";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "[Leave]", -1 };
	END_NODE(t);
}

// NPC 5 - Herb Doctor (herbdoctor)
// Location: North of village
// Quest:    collect 3 moonberries (red fruits), return for HP bonus
void NPCManager::buildNPC5(NPC& npc) {
	npc.x = 4100.0f;
	npc.y = 3050.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 60;
	npc.colorG = 160;
	npc.colorB = 80;
	npc.name = "Elara";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 : ----
	_N(t).npcLine1 = "Oh! Excuse me, Mr. Knight! My name is Elara.";
	_N(t).npcLine2 = "I brew healing tonics for the sick and wounded. I ran out of moonberries!";
	_N(t).npcLine3 = "Could you fetch some for me? They glow bright red.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I can lend assistance. Where do I find these moonberries?", 1 };
	_N(t).responses[1] = { "I am too busy for errands right now.", -1 };
	END_NODE(t);

	// ---- NODE 1 : ----
	_N(t).npcLine1 = "They grow near the lake to the west of here!";
	_N(t).npcLine2 = "You will recognise them by their vivid red glow!";
	_N(t).npcLine3 = "Bring me 3 and I shall brew you a lasting vitality tonic!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I am on my way.", -1 };
	_N(t).responses[1] = { "I have not found them yet. I will keep looking.", -1 };
	END_NODE(t);

	// ---- NODE 2 : ----
	_N(t).npcLine1 = "You found them! These moonberries are perfect specimens!";
	_N(t).npcLine2 = "Let me brew this quickly... there you go!";
	_N(t).npcLine3 = "Drink this tonic. Your vitality will grow permanently!";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "I can feel the warmth already.", 3 };
	END_NODE(t);

	// ---- NODE 3 : ----
	_N(t).npcLine1 = "That tonic will serve you through many battles to come.";
	_N(t).npcLine2 = "You have my deepest gratitude, brave Knight. You have saved a lot of lives!";
	_N(t).npcLine3 = "You have saved a lot of lives!";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Farewell, Elara. I must go now.", -1 };
	END_NODE(t);
}

// NPC 5 - Theador (hurt fighter)
// Location: near final boss entrance
void NPCManager::buildNPC6(NPC& npc) {
	npc.x = 1236.0f;
	npc.y = 1446.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 100;
	npc.colorG = 150;
	npc.colorB = 90;
	npc.name = "Theador";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 : ----
	_N(t).npcLine1 = "";
	_N(t).npcLine2 = "Hey! Slow down!! There are creatures nearby...";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "You look hurt. What happened to you?", 1 };
	_N(t).responses[1] = { "I have no time for you.", -1 };
	END_NODE(t);

	// ---- NODE 1 : ----
	_N(t).npcLine1 = "I tried to enter the dungeon through that entrance...";
	_N(t).npcLine2 = "Then some creatures came out of there and attacked me!";
	_N(t).npcLine3 = "I barely managed to kill them but they have injured me...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "In this condition, you are unable to fight. You should rest.", 2 };
	_N(t).responses[1] = { "I must go", -1 };
	END_NODE(t);

	// ---- NODE 2 : ----
	_N(t).npcLine1 = "Yeah...you are right, fellow knight.";
	_N(t).npcLine2 = "You must keep on fighting. Do not let them get the best of you.";
	_N(t).npcLine3 = "Do not stop until they are all gone...";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 3 };
	_N(t).responses[1] = { "I have heard enough.", -1 };
	END_NODE(t);

	// ---- NODE 3 : ----
	_N(t).npcLine1 = "That dungeon must be holding some dark being...";
	_N(t).npcLine2 = "Some call him the Corrupted King...";
	_N(t).npcLine3 = "You must put an end to this!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 4 };
	_N(t).responses[1] = { "I have heard enough.", -1 };
	END_NODE(t);

	// ---- NODE 4 : ----
	_N(t).npcLine1 = "Promise me that you will defeat him!";
	_N(t).npcLine2 = "You must not lose!";
	_N(t).npcLine3 = "You must prevail!!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "I shall finish what you have started. You have my word", 5 };
	_N(t).responses[1] = { "I do not make promises.", -1 };
	END_NODE(t);

	// ---- NODE 5 : ----
	_N(t).npcLine1 = "";
	_N(t).npcLine2 = "May thy mighty sword chip and shatter! Farewell!!";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Farewell.", -1 };
	END_NODE(t);
}

// NPC 5 - ??? (mad man)
// Location: near upper castle tower
void NPCManager::buildNPC7(NPC& npc) {
	npc.x = 4290.0f;
	npc.y = 4706.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 50;
	npc.colorG = 170;
	npc.colorB = 70;
	npc.name = "???";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 :----
	_N(t).npcLine1 = "They crawl where lightno, nodont look there,";
	_N(t).npcLine2 = "The dark remembers, it stares and stares,";
	_N(t).npcLine3 = "I told it not to learn your name.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "You seem lost. Do you need assistance?", 1 };
	_N(t).responses[1] = { "[Leave]", -1 };
	END_NODE(t);

	// ---- NODE 1 :----
	_N(t).npcLine1 = "I saw their teeth (too many teeth),";
	_N(t).npcLine2 = "They whispered secrets underneath,";
	_N(t).npcLine3 = "My skin still humsthey stay the same.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 2 };
	_N(t).responses[1] = { "[Leave]", -1 };
	END_NODE(t);

	// ---- NODE 2 :----
	_N(t).npcLine1 = "I hear them knock inside my head,";
	_N(t).npcLine2 = "Not dead, not dead, not dead, they said,";
	_N(t).npcLine3 = "But somethings dripping through the floors.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 3 };
	_N(t).responses[1] = { "[Leave]", -1 };
	END_NODE(t);

	// ---- NODE 3 :----
	_N(t).npcLine1 = "The trees bend closerclosercloser,";
	_N(t).npcLine2 = "Each branch a finger on your shoulder,";
	_N(t).npcLine3 = "Dont turndont turndont turn";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 4 };
	_N(t).responses[1] = { "[Leave]", -1 };
	END_NODE(t);

	// ---- NODE 4 :----
	_N(t).npcLine1 = "I ran...I think I did?...I tried to pray,";
	_N(t).npcLine2 = "The words fell out the wrong waywrong way,";
	_N(t).npcLine3 = "They laughed and wore them like a face.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 5 };
	_N(t).responses[1] = { "[Leave]", -1 };
	END_NODE(t);

	// ---- NODE 5 :----
	_N(t).npcLine1 = "Dont make a soundit hears you think,";
	_N(t).npcLine2 = "Your thoughts are louder than you blink,";
	_N(t).npcLine3 = "It drinks them slow, it drinks them deep.";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "[Continue]", 6 };
	_N(t).responses[1] = { "[Leave]", -1 };
	END_NODE(t);

	// ---- NODE 6 :----
	_N(t).npcLine1 = "Too latetoo lateI said that first,";
	_N(t).npcLine2 = "It knows your voice, it knows your worst,";
	_N(t).npcLine3 = "It sings you gently into sleep.";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "[Leave]", -1 };
	END_NODE(t);
}

// NPC 5 - Pruette (child)
// Location: bottom of village
void NPCManager::buildNPC8(NPC& npc) {
	npc.x = 3342.0f;
	npc.y = 940.0f;
	npc.w = 32;
	npc.h = 32;
	npc.colorR = 120;
	npc.colorG = 80;
	npc.colorB = 150;
	npc.name = "Pruette";

	npc.inConversation = false;
	npc.currentNode = 0;
	npc.checkpointNode = 0;
	npc.dialogueCompleted = false;
	npc.hidePortrait = false;

	npc.tex[0] = -1;
	npc.tex[1] = -1;
	npc.animFrame = 0;
	npc.animTimer = 0;

	DialogueTree& t = npc.tree;
	t.nodeCount = 0;

	// ---- NODE 0 :----
	_N(t).npcLine1 = "Oh my God!! You saved me!!";
	_N(t).npcLine2 = "They were trying to kill me!!";
	_N(t).npcLine3 = "You saved me!!";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "Do not worry anymore, child. You are safe now.", 1 };
	_N(t).responses[1] = { "I must not waste time.", -1 };
	END_NODE(t);

	// ---- NODE 1 :----
	_N(t).npcLine1 = "I am eternally grateful to you, sire!";
	_N(t).npcLine2 = "I will tell everyone of your bravery!!";
	_N(t).npcLine3 = "";
	_N(t).responseCount = 2;
	_N(t).responses[0] = { "No need for that. Do not wander alone in these lands.", 2 };
	_N(t).responses[1] = { "I shall go now.", -1 };
	END_NODE(t);

	// ---- NODE 2 :----
	_N(t).npcLine1 = "Yes, sire!";
	_N(t).npcLine2 = "I must not go on my on anywhere!";
	_N(t).npcLine3 = "I promise you.";
	_N(t).responseCount = 1;
	_N(t).responses[0] = { "Go home, child. Be careful.", -1 };
	END_NODE(t);
}

#undef _N
#undef END_NODE