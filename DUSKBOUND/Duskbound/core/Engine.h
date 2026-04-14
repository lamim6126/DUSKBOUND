#ifndef ENGINE_H
#define ENGINE_H

class Scene;

// Camera structure definition 
struct Camera {
	float x, y;
	Camera() : x(0.0f), y(0.0f) {}
};

// Constants for world dimensions
extern const int WORLD_W;
extern const int WORLD_H;

// Camera object to manage the view
extern Camera cam;

class Engine {
public:
	static Scene* currentScene;

	static void init(Scene* startScene);
	static void changeScene(Scene* scene);
	static void update();
	static void draw();
};

#endif
