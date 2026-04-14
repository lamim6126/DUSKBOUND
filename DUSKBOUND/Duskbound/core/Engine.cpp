#include "Engine.h"
#include "Scene.h"
#include "Input.h"
#include "GameConfig.h"  
#include <cstring>

// Static member initialization
Scene* Engine::currentScene = nullptr;

// Global world constants definition (from GameConfig.h)
const int WORLD_W = WORLD_WIDTH;
const int WORLD_H = WORLD_HEIGHT;

// Global camera instance definition
Camera cam;

// Declare iGraphics' keyPressed array (it's external)
extern unsigned int keyPressed[512];

void Engine::init(Scene* s) {
	if (currentScene) delete currentScene;
	currentScene = s;
	
	// Clear both key arrays
	memset(keys, 0, sizeof(keys));
	memset(keyPressed, 0, sizeof(keyPressed));
}

void Engine::changeScene(Scene* s) {
	if (currentScene) delete currentScene;
	currentScene = s;
	
	// Clear both key arrays
	memset(keys, 0, sizeof(keys));
	memset(keyPressed, 0, sizeof(keyPressed));
}

void Engine::update() {
	if (currentScene) currentScene->update();
}

void Engine::draw() {
	if (currentScene) currentScene->draw();
}
