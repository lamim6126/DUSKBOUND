#include "CollisionMap.h"
#include "../core/GameConfig.h"  
#include <cstdio>
#include <cstring>
#include "../vendor/stb_image.h"

static bool  s_mask[CHUNK_ROWS][CHUNK_COLS][MASK_SIZE][MASK_SIZE];
static bool  s_loaded = false;

// World units represented by one mask pixel (1024 / 256 = 4.0 units)
static const float MASK_SCALE = (float)CHUNK_SIZE / (float)MASK_SIZE;

// loadCollisionMasks
// Call once from PlayScene::update() - no-op on subsequent calls.
void loadCollisionMasks() {

	if (s_loaded) return;   // already done

	// Default: every cell walkable.
	// Chunks without a mask file keep this default.
	memset(s_mask, 1, sizeof(s_mask));

	printf("=== LOADING COLLISION MASKS ===\n");

	for (int row = 1; row <= CHUNK_ROWS; row++) {
		for (int col = 1; col <= CHUNK_COLS; col++) {

			// ---- Build the file path ----
			// Naming convention: coll{ROW}{COL}.bmp
			// e.g. coll21.bmp = row 2, col 1
			char path[256];
			sprintf_s(path, sizeof(path),
				"assets/images/collision/coll%d%d.bmp", row, col);

			// ---- Load with stb_image (supports BMP, PNG, JPG) ----
			int imgW = 0, imgH = 0, channels = 0;
			unsigned char* data = stbi_load(path, &imgW, &imgH, &channels, 3);

			if (!data) {
				// Missing file is NOT an error - chunk stays fully walkable
				printf("  coll%d%d.bmp : not found (fully walkable)\n", row, col);
				continue;
			}

			// ---- Resample the loaded image into MASK_SIZE x MASK_SIZE ----
			for (int my = 0; my < MASK_SIZE; my++) {
				for (int mx = 0; mx < MASK_SIZE; mx++) {

					// Nearest-neighbour sample from source image
					int imgX = mx          * imgW / MASK_SIZE;
					int imgY = my          * imgH / MASK_SIZE;
					int flipY = (imgH - 1) - imgY;  // flip for Y-up world

					int pixelIndex = (flipY * imgW + imgX) * 3; // stride = 3 (RGB)
					int r = data[pixelIndex + 0];
					int g = data[pixelIndex + 1];
					int b = data[pixelIndex + 2];

					// Threshold: bright (>128) = walkable, dark (<=128) = solid.
					// Averaging all three channels handles grey brushwork too.
					int brightness = (r + g + b) / 3;
					s_mask[row - 1][col - 1][my][mx] = (brightness > 128);
				}
			}

			stbi_image_free(data);
			printf("  coll%d%d.bmp : loaded  (src %dx%d -> mask %dx%d)\n",
				row, col, imgW, imgH, MASK_SIZE, MASK_SIZE);
		}
	}

	s_loaded = true;
	printf("=== COLLISION MASKS READY ===\n\n");
}


// isWalkable
// Returns true if world-space point (worldX, worldY) is open ground.
bool isWalkable(float worldX, float worldY) {

	if (!s_loaded) return true;   // masks not loaded yet - don't block movement

	// Out-of-world -> solid (keeps player inside map boundaries)
	if (worldX < 0.0f || worldX >= (float)WORLD_WIDTH)  return false;
	if (worldY < 0.0f || worldY >= (float)WORLD_HEIGHT) return false;

	// Which chunk (0-indexed)?
	int col = (int)(worldX / (float)CHUNK_SIZE);
	int row = (int)(worldY / (float)CHUNK_SIZE);

	// Clamp to valid range (handles exact edge of world)
	if (col < 0) col = 0;  if (col >= CHUNK_COLS) col = CHUNK_COLS - 1;
	if (row < 0) row = 0;  if (row >= CHUNK_ROWS) row = CHUNK_ROWS - 1;

	// Position inside the chunk (local coordinates)
	float localX = worldX - col * (float)CHUNK_SIZE;
	float localY = worldY - row * (float)CHUNK_SIZE;

	// Convert to mask grid index
	int mx = (int)(localX / MASK_SCALE);
	int my = (int)(localY / MASK_SCALE);

	if (mx < 0) mx = 0;  if (mx >= MASK_SIZE) mx = MASK_SIZE - 1;
	if (my < 0) my = 0;  if (my >= MASK_SIZE) my = MASK_SIZE - 1;

	return s_mask[row][col][my][mx];
}


// canPlayerMoveTo
//
// Checks 8 points around the player's bounding box.
// Axis-separated calls in Player::update() mean the player
// slides smoothly along walls instead of getting hard-stuck.
bool canPlayerMoveTo(float newX, float newY, int playerW, int playerH) {

	const float in = 4.0f;          // pixel inset from sprite corners

	float l = newX + in;                    // left edge
	float r = newX + playerW - in;        // right edge
	float b = newY + in;                    // bottom edge
	float t = newY + playerH - in;        // top edge
	float cx = newX + playerW  * 0.5f;      // horizontal centre
	float cy = newY + playerH  * 0.5f;      // vertical centre

	// Bottom row
	if (!isWalkable(l, b))  return false;   // BL
	if (!isWalkable(cx, b))  return false;   // BM
	if (!isWalkable(r, b))  return false;   // BR

	// Middle row
	if (!isWalkable(l, cy)) return false;   // ML
	if (!isWalkable(r, cy)) return false;   // MR

	// Top row
	if (!isWalkable(l, t))  return false;   // TL
	if (!isWalkable(cx, t))  return false;   // TM
	if (!isWalkable(r, t))  return false;   // TR

	return true;   // all 8 points are in walkable space
}