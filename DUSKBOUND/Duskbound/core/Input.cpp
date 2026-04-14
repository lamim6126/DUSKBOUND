#include "Input.h"
#include <cstring>

// Declare iGraphics' keyPressed array (it is external)
extern unsigned int keyPressed[512];

// Current key state copied from iGraphics each frame.
bool keys[256] = { false };

// Previous-frame state used for edge detection.
static bool prevKeys[256] = { false };

void syncKeys() {
    for (int i = 0; i < 256; i++) {
        bool downNow = (keyPressed[i] != 0);
        keys[i] = downNow;

        // When the key is released, clear the previous-state latch
        // so the next press can trigger consumeKey() again.
        if (!downNow) {
            prevKeys[i] = false;
        }
    }
}

bool consumeKey(unsigned char k) {
    if (!keys[k]) {
        prevKeys[k] = false;
        return false;
    }

    if (!prevKeys[k]) {
        prevKeys[k] = true;
        return true;
    }

    return false;
}
