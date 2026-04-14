#include "vendor/iGraphics.h"
#include "core/Engine.h"
#include "core/Input.h"
#include "core/Renderer.h"
#include "scenes/MenuScene.h"

#include <cstdio>
#include <direct.h>

// Global mouse state (read by menu scenes)
int g_mouseX = 0;
int g_mouseY = 0;
bool g_mouseLDown = false;

// Basic drawing wrappers
void drawRect(float x, float y, int w, int h) {
  iSetColor(0, 0, 0);
  iRectangle(x, y, w, h);
}

void drawText(int x, int y, const char *str) {
  iSetColor(0, 0, 0);
  iText(x, y, (char *)str);
}

void drawTextC(int x, int y, const char *str, int r, int g, int b) {
  iSetColor(r, g, b);
  iText(x, y, (char *)str);
}

// Draws larger, bold-simulated text using GLUT_BITMAP_TIMES_ROMAN_24.
// Bold effect achieved by rendering text at several 1-pixel offsets.
// Approximate char width for Times Roman 24 is ~13px for centering.
void drawTextBig(int x, int y, const char *str, int r, int g, int b) {
  iSetColor(r, g, b);
  // Bold simulation: draw at multiple offsets
  int offsets[5][2] = {{0,0},{1,0},{-1,0},{0,1},{0,-1}};
  for (int o = 0; o < 5; o++) {
    iText(x + offsets[o][0], y + offsets[o][1], (char *)str,
          GLUT_BITMAP_TIMES_ROMAN_24);
  }
}

// drawBMP / drawBMP2 – kept for small sprites (player etc.)
// These still use glRasterPos + glDrawPixels so they ONLY
// work reliably when (x, y) is inside the viewport.
// For world chunks use drawChunk() instead.
void drawBMP(int x, int y, const char *path) {
	//only draw if the file actually exists and can be opened
	FILE *f = NULL;
  fopen_s(&f, path, "rb");
  if (!f)
    return;
  fclose(f);
  iShowBMP(x, y, (char *)path);
}

void drawBMP2(int x, int y, const char *path, int ignoreColor) {
	//only draw if the file actually exists and can be opened
	FILE *f = NULL;
  fopen_s(&f, path, "rb");
  if (!f)
    return;
  fclose(f);
  iShowBMP2(x, y, (char *)path, ignoreColor);
}

int loadImage(const char *path) {
  // iLoadImage uses stbi_load → returns texture id (unsigned int cast to int)
  FILE *f = NULL;
  fopen_s(&f, path, "rb");
  if (!f)
    return -1; // file doesn't exist – skip silently
  fclose(f);
  return (int)iLoadImage((char *)path);
}

void drawImage(int x, int y, int w, int h, int id) {
  if (id >= 0)
    iShowImage(x, y, w, h, (unsigned int)id);
}

void drawImageTransparent(int x, int y, int w, int h, int id, int ignoreColor) {
  if (id >= 0)
    iShowImage(x, y, w, h, (unsigned int)id);
}

void setColor(int r, int g, int b) {
	iSetColor(r, g, b);
}

void fillRect(int x, int y, int w, int h) {
	iFilledRectangle(x, y, w, h);
}

void drawRectOutline(int x, int y, int w, int h) {
	iRectangle(x, y, w, h);
}

// drawChunk  –  GL_QUADS based tile renderer
void drawChunk(int x, int y, int w, int h, int id) {
  if (id < 0)
    return;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, (unsigned int)id);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  float fx = (float)x;
  float fy = (float)y;
  float fw = (float)w;
  float fh = (float)h;

  glBegin(GL_QUADS);
  // tc_y = 1 at the bottom of the quad (screen-bottom)  → image bottom
  // tc_y = 0 at the top   of the quad (screen-top)      → image top
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(fx, fy); // bottom-left
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(fx + fw, fy); // bottom-right
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(fx + fw, fy + fh); // top-right
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(fx, fy + fh); // top-left
  glEnd();

  glDisable(GL_TEXTURE_2D);
}

// iGraphics callbacks
void iDraw() {
  iClear();

  // White background (world tiles will cover this)
  iSetColor(255, 255, 255);
  iFilledRectangle(0, 0, 800, 450);

  Engine::draw();
}

void fixedUpdate() {
  syncKeys();
  Engine::update();
}

void iKeyboard(unsigned char k) {}
void iKeyboardUp(unsigned char k) {}
void iMouseMove(int mx, int my) {
  g_mouseX = mx;
  g_mouseY = my;
}
void iPassiveMouseMove(int mx, int my) {
  g_mouseX = mx;
  g_mouseY = my;
}
void iMouse(int button, int state, int mx, int my) {
  g_mouseX = mx;
  g_mouseY = my;
  if (button == GLUT_LEFT_BUTTON)
    g_mouseLDown = (state == GLUT_DOWN);
}

int main() {
  Engine::init(new MenuScene());
  iInitialize(800, 450, (char *)"Duskbound");
  iSetTimer(16, fixedUpdate);
  iStart();
  return 0;
}