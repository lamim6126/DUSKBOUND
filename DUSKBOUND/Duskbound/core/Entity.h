#ifndef ENTITY_H
#define ENTITY_H

class Entity {
public:
	float x, y;
	int w, h;

	virtual void update() = 0;
	virtual void draw() = 0;
	virtual ~Entity() {}
};

#endif
