#ifndef COLLISIONMAP_H
#define COLLISIONMAP_H

// COLLISION MAP SYSTEM

// Resolution of each chunk mask kept in RAM.
// 256 gives ~4-world-unit precision on a 1024-unit chunk.
static const int MASK_SIZE = 256;

// ---- Public API -----

// Load all chunk masks from disk.
// Safe to call every frame – is a no-op after the first call.
void loadCollisionMasks();

// Returns true if the single world-space point (worldX, worldY) is walkable.
bool isWalkable(float worldX, float worldY);

// Returns true if a bounding box (playerW x playerH) placed at world
// position (newX, newY) is entirely within walkable space.
// Checks 8 points so the player slides along walls instead of stopping dead.
bool canPlayerMoveTo(float newX, float newY, int playerW, int playerH);

#endif // COLLISIONMAP_H