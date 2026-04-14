#ifndef RENDERER_H
#define RENDERER_H

void drawRect(float x, float y, int w, int h);
void drawText(int x, int y, const char* str);

void drawBMP(int x, int y, const char* path);
void drawBMP2(int x, int y, const char* path, int ignoreColor);

int  loadImage(const char* path);
void drawImage(int x, int y, int w, int h, int id);
void drawImageTransparent(int x, int y, int w, int h, int id, int ignoreColor);
void drawTextC(int x, int y, const char* str, int r, int g, int b);
void drawTextBig(int x, int y, const char* str, int r, int g, int b);

// Additional drawing functions for game scenes
void setColor(int r, int g, int b);
void fillRect(int x, int y, int w, int h);
void drawRectOutline(int x, int y, int w, int h);
void drawChunk(int x, int y, int w, int h, int id);

#endif