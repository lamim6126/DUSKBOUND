#ifndef DIALOGUESYSTEM_H
#define DIALOGUESYSTEM_H

// DIALOGUE SYSTEM - Fallout-style branching dialogue trees

#define MAX_RESPONSES      4   // Player choices per node
#define MAX_DIALOGUE_NODES 30  // Nodes per NPC tree

// One player response option
struct PlayerResponse {
	const char* text;    // What the player says   (shown in UI)
	int         nextNode; // Node to jump to, or -1 to end dialogue
};

// One beat of dialogue: NPC speaks, player
// chooses one of up to MAX_RESPONSES replies
struct DialogueNode {
	const char*    npcLine1;                      // First  line of NPC text
	const char*    npcLine2;                      // Second line ("" = unused)
	const char*    npcLine3;
	PlayerResponse responses[MAX_RESPONSES];
	int            responseCount;
};

// A complete dialogue tree for one NPC.
// nodes[0] is ALWAYS the opening/greeting node.
struct DialogueTree {
	DialogueNode nodes[MAX_DIALOGUE_NODES];
	int          nodeCount;
};

#endif // DIALOGUESYSTEM_H