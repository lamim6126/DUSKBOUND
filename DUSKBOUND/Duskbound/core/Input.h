#ifndef INPUT_H
#define INPUT_H

// Global key state array (true = key is currently pressed)
extern bool keys[256];

// Call this every frame from fixedUpdate() BEFORE any game logic.
// This copies iGraphics keyboard state to keys[] and also resets
// the edge-detect state when a key is released.
void syncKeys();

// Returns true only once per physical key press.
// Holding the key down will not repeatedly trigger this function.
bool consumeKey(unsigned char k);

#endif
