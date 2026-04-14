#include "PauseScene.h"
#include "../core/Engine.h"
#include "../core/Input.h"
#include "../core/Renderer.h"
#include "../core/Sound.h"
#include "../core/SaveSystem.h"
#include "PlayScene.h"
#include "MainMenuScene.h"
#include <cstdlib>
#include <cmath>

// Extern mouse state (defined in iMain.cpp)
extern int g_mouseX;
extern int g_mouseY;
extern bool g_mouseLDown;

// Menu audio
static const char* PAUSE_MENU_MUSIC = "assets/audio/pause.wav";
static bool gPauseMenuMusicPlaying = false;

static void UpdatePauseMenuMusic(bool audioOn) {
    if (audioOn) {
        if (!gPauseMenuMusicPlaying) {
            playSound(PAUSE_MENU_MUSIC, true);
            gPauseMenuMusicPlaying = true;
        }
    } else {
        if (gPauseMenuMusicPlaying) {
            stopSound();
            gPauseMenuMusicPlaying = false;
        }
    }
}

// Button layout – mirrors main menu exactly, but 6 buttons
static const int PAUSE_BTN_COUNT   = 6;
static const int PAUSE_BTN_W       = 200;
static const int PAUSE_BTN_H       = 50;
static const int PAUSE_BTN_GAP     = 58;
static const int PAUSE_BTN_CX      = 410;
static const int PAUSE_BTN_START_Y = 339; // shifted up to fit 6 buttons

// Button images: [button index][0=normal, 1=hover]
static const char* PAUSE_BTN_IMG[6][2] = {
    { "assets/buttons/btn_start_nm.png",  "assets/buttons/btn_start_hv.png"  }, // 0 Start
    { "assets/buttons/btn_resume_nm.png", "assets/buttons/btn_resume_hv.png" }, // 1 Resume
    { "assets/buttons/btn_load_nm.png",   "assets/buttons/btn_load_hv.png"   }, // 2 Load Save
    { "assets/buttons/btn_option_nm.png", "assets/buttons/btn_option_hv.png" }, // 3 Options
    { "assets/buttons/btn_credit_nm.png", "assets/buttons/btn_credit_hv.png" }, // 4 Credits
    { "assets/buttons/btn_exit_nm.png",   "assets/buttons/btn_exit_hv.png"   }, // 5 Exit
};

// Helper – returns main-button index (0-5) under mouse, or -1
int PauseScene::getHoveredItem() {
    for (int i = 0; i < PAUSE_BTN_COUNT; i++) {
        int bx = PAUSE_BTN_CX - PAUSE_BTN_W / 2;
        int by = PAUSE_BTN_START_Y - i * PAUSE_BTN_GAP;
        if (g_mouseX >= bx && g_mouseX <= bx + PAUSE_BTN_W &&
            g_mouseY >= by && g_mouseY <= by + PAUSE_BTN_H)
            return i;
    }
    return -1;
}

// Helper – returns options-item index (0-2) under mouse, or -1
// (exact copy of MainMenuScene::getHoveredOption geometry)
int PauseScene::getHoveredOption() {
    static const int OPT_COUNT = 3;
    // Centered at x=400, items spaced 70px apart starting at y=260
    int cx = 400, y = 260, gap = 70;
    for (int i = 0; i < OPT_COUNT; i++) {
        int cy    = y - i * gap;
        int left  = cx - 160;
        int right = cx + 160;
        int bot   = cy - 22;
        int top   = cy + 22;
        if (g_mouseX >= left && g_mouseX <= right &&
            g_mouseY >= bot  && g_mouseY <= top)
            return i;
    }
    return -1;
}

// Constructor
PauseScene::PauseScene()
    : selected(1),        // default highlight on Resume
      moveCooldown(10),   // small delay so Esc key doesn't instantly fire
      page(0),
      optionSelected(0),
      audioOn(true),
      msgTimer(0),
      mouseX(0), mouseY(0),
      mouseLWasDown(false)
{
    for (int i = 0; i < PAUSE_BTN_COUNT; i++) {
        btnTex[i][0] = loadImage(PAUSE_BTN_IMG[i][0]);
        btnTex[i][1] = loadImage(PAUSE_BTN_IMG[i][1]);
    }
    creditsTexId = loadImage("assets/credits/credis.png");
}

// Update
void PauseScene::update() {

    UpdatePauseMenuMusic(audioOn);

    // Snapshot mouse
    mouseX = g_mouseX;
    mouseY = g_mouseY;
    bool lDown  = g_mouseLDown;
    bool clicked = (!lDown && mouseLWasDown);
    mouseLWasDown = lDown;

    // ---- OPTIONS PAGE (page == 1) ----
    if (page == 1) {
        if (moveCooldown > 0) moveCooldown--;

        // Mouse hover
        int hov = getHoveredOption();
        if (hov >= 0) optionSelected = hov;

        // Keyboard nav
        if (moveCooldown == 0) {
            if (keys['w'] || keys['W']) {
                optionSelected--;
                if (optionSelected < 0) optionSelected = 2;
                moveCooldown = 10;
            } else if (keys['s'] || keys['S']) {
                optionSelected++;
                if (optionSelected > 2) optionSelected = 0;
                moveCooldown = 10;
            }
        }

        // Confirm: keyboard ENTER/SPACE or mouse click
        bool confirm = (moveCooldown == 0 && (consumeKey(13) || consumeKey(32))) ||
                       (clicked && getHoveredOption() == optionSelected);
        if (confirm) {
            if (optionSelected == 0) {          // Controls sub-page
                page = 3;
                moveCooldown = 10;
                return;
            }
            if (optionSelected == 1) {          // Toggle audio
                audioOn = !audioOn;
                stopSound();
                gPauseMenuMusicPlaying = false;
                if (audioOn) {
                    playSound(PAUSE_MENU_MUSIC, true);
                    gPauseMenuMusicPlaying = true;
                }
                moveCooldown = 10;
                return;
            }
            if (optionSelected == 2) {          // Back
                page = 0;
                moveCooldown = 10;
                return;
            }
        }

        // B = back
        if (consumeKey('b') || consumeKey('B')) {
            page = 0;
            moveCooldown = 10;
            return;
        }
        // Esc on sub-pages = back to main pause menu (NOT resume)
        if (consumeKey(27)) {
            page = 0;
            moveCooldown = 10;
            return;
        }
        return;
    }

    // ---- CREDITS PAGE (page == 2) ----
    if (page == 2) {
        if (moveCooldown > 0) moveCooldown--;
        msgTimer++;
        if (moveCooldown == 0 && (consumeKey('b') || consumeKey('B'))) {
            page = 0;
            msgTimer = 0;
            moveCooldown = 10;
        }
        // Esc on sub-pages = back to main pause menu
        if (consumeKey(27)) {
            page = 0;
            moveCooldown = 10;
        }
        return;
    }

    // ---- CONTROLS PAGE (page == 3) ----
    if (page == 3) {
        if (consumeKey('b') || consumeKey('B')) {
            page = 1;   // back to options
            moveCooldown = 10;
            return;
        }
        // Esc on sub-pages = back to main pause menu
        if (consumeKey(27)) {
            page = 0;
            moveCooldown = 10;
            return;
        }
        return;
    }

    // ---- MAIN PAUSE MENU (page == 0) ----
    if (moveCooldown > 0) moveCooldown--;

    // Mouse hover updates selection
    int hov = getHoveredItem();
    if (hov >= 0) selected = hov;

    // Keyboard navigation (W/S)
    if (moveCooldown == 0) {
        if (keys['w'] || keys['W']) {
            selected--;
            if (selected < 0) selected = PAUSE_BTN_COUNT - 1;
            moveCooldown = 10;
        } else if (keys['s'] || keys['S']) {
            selected++;
            if (selected >= PAUSE_BTN_COUNT) selected = 0;
            moveCooldown = 10;
        }
    }

    // Esc on main page = Resume
    if (moveCooldown == 0 && consumeKey(27)) {
        selected = 1;
        stopSound();
        gPauseMenuMusicPlaying = false;
        Engine::changeScene(new PlayScene());
        return;
    }

    // Confirm: ENTER / SPACE / mouse click on hovered item
    bool confirm = (moveCooldown == 0 && (consumeKey(13) || consumeKey(32))) ||
                   (clicked && getHoveredItem() == selected && hov >= 0);

    if (confirm) {
        switch (selected) {
        case 0: // Start – full restart
            stopSound();
            gPauseMenuMusicPlaying = false;
            beginNewSaveSlot();
            PlayScene::reset();
            Engine::changeScene(new PlayScene());
            break;
        case 1: // Resume – reuses static PlayScene state (position intact)
            stopSound();
            gPauseMenuMusicPlaying = false;
            Engine::changeScene(new PlayScene());
            break;
        case 2: // Load Save
            stopSound();
            gPauseMenuMusicPlaying = false;
            Engine::changeScene(new MainMenuScene(4));
            break;
        case 3: // Options
            page = 1;
            moveCooldown = 10;
            break;
        case 4: // Credits
            page = 2;
            msgTimer = 0;
            moveCooldown = 10;
            break;
        case 5: // Exit
            exit(0);
            break;
        }
    }
}

// Draw
void PauseScene::draw() {
    drawBMP(0, 0, "assets/images/menu_bg.bmp");

    // ---- OPTIONS PAGE ----
    if (page == 1) {
        // Centered "OPTIONS" title at top
        const char* title = "OPTIONS";
        int titleX = 400 - (int)(strlen(title) * 13) / 2;
        drawTextBig(titleX + 1, 391, title, 0, 0, 0);
        drawTextBig(titleX,     390, title, 255, 220, 100);

        // Decorative divider under title
        setColor(180, 150, 60);
        fillRect(200, 375, 400, 2);

        const char* options[3] = { "Controls", audioOn ? "Audio: ON" : "Audio: OFF", "Back" };

        // Three menu items centered at x=400, spaced 70px apart from y=260
        int cx = 400, y = 260, gap = 70;
        for (int i = 0; i < 3; i++) {
            int yy   = y - i * gap;
            bool hov = (i == optionSelected);
            int len   = (int)strlen(options[i]);
            int textW = len * 13;          // ~13px per char in Times Roman 24
            int textX = cx - textW / 2;

            // Highlight / background box (wider, centered)
            if (hov) {
                setColor(100, 75, 15);
                fillRect(cx - 162, yy - 20, 324, 46);
                setColor(70, 50, 5);
                fillRect(cx - 160, yy - 18, 320, 42);
            } else {
                setColor(15, 12, 30);
                fillRect(cx - 160, yy - 18, 320, 42);
            }

            // Arrow indicator for selected item
            if (hov) {
                int arrX = cx - 155;
                drawTextBig(arrX + 1, yy - 8, ">", 0, 0, 0);
                drawTextBig(arrX,     yy - 8, ">", 255, 220, 0);
                drawTextBig(textX + 1, yy - 8, options[i], 0, 0, 0);
                drawTextBig(textX,     yy - 8, options[i], 255, 220, 0);
            } else {
                drawTextBig(textX + 1, yy - 8, options[i], 0, 0, 0);
                drawTextBig(textX,     yy - 8, options[i], 200, 200, 200);
            }
        }

        // Decorative divider above hint
        setColor(180, 150, 60);
        fillRect(200, 78, 400, 2);

        drawTextC(221, 61, "W/S = move   ENTER = select   B = back", 0, 0, 0);
        drawTextC(220, 60, "W/S = move   ENTER = select   B = back", 200, 200, 200);
        return;
    }

    // ---- CREDITS PAGE ----
	if (page == 2) {
		float time = msgTimer * 0.05f;

		// ---- Scroll background (parchment, same style as quest scroll) ----
		const int scrollX = 140;
		const int scrollY = 40;
		const int scrollW = 520;
		const int scrollH = 370;

		// Outer border (dark brown)
		setColor(96, 72, 34);
		fillRect(scrollX - 4, scrollY - 4, scrollW + 8, scrollH + 8);

		// Parchment face
		setColor(218, 196, 148);
		fillRect(scrollX, scrollY, scrollW, scrollH);

		// Inner border accent
		setColor(140, 108, 52);
		drawRectOutline(scrollX, scrollY, scrollW, scrollH);

		// Thin inner rule 8px inside the border
		setColor(160, 128, 60);
		drawRectOutline(scrollX + 8, scrollY + 8, scrollW - 16, scrollH - 16);

		// Divider under title
		setColor(140, 108, 52);
		fillRect(scrollX + 20, scrollY + scrollH - 30, scrollW - 40, 1);

		// ---- Section header ----
		const char* secHeader = "Developed by";
		int shX = 400 - (int)(strlen(secHeader) * 8) / 2;
		drawTextC(shX + 1, scrollY + scrollH - 56, secHeader, 80, 50, 15);
		drawTextC(shX, scrollY + scrollH - 57, secHeader, 80, 50, 15);

		// ---- Divider above first entry ----
		setColor(160, 130, 70);
		fillRect(scrollX + 40, scrollY + scrollH - 72, scrollW - 80, 1);

		// ---- Entry helper layout ----
		// Three entries, evenly spaced.
		// Each entry: Name (gold) + ID (muted dark brown) + role (dark)
		struct CreditEntry {
			const char* name;
			const char* id;
			const char* role;
		};

		static const CreditEntry entries[3] = {
			{ "RESHAD AHMED LAMIM", "ID: 00724205101076", "Game Systems & World Design" },
			{ "MUNTASIR RABBI SHOSCHA", "ID: 00724205101079", "Boss Design & Cutscene Systems" },
			{ "MD. RAIYAN HUSSAIN CHOUDHURY",
			"ID: 00724205101067", "Player Controller & UI Systems" },
		};

		// Row Y positions (from bottom of scroll, descending)
		const int rowY[3] = {
			scrollY + scrollH - 108,
			scrollY + scrollH - 186,
			scrollY + scrollH - 264,
		};

		for (int i = 0; i < 3; i++) {
			// Entry background stripe (subtle warm tint)
			setColor(200, 176, 120);
			fillRect(scrollX + 20, rowY[i] - 44, scrollW - 40, 50);
			setColor(160, 128, 60);
			drawRectOutline(scrollX + 20, rowY[i] - 44, scrollW - 40, 50);

			// Number badge (left side)
			setColor(96, 72, 34);
			fillRect(scrollX + 28, rowY[i] - 36, 22, 22);
			char badge[4];
			sprintf_s(badge, sizeof(badge), "%d", i + 1);
			drawTextC(scrollX + 32, rowY[i] - 22, badge, 218, 196, 148);

			// Name (dark brown, bold via drawTextBig)
			int nameX = scrollX + 60;
			drawTextBig(nameX + 1, rowY[i] - 18, entries[i].name, 60, 30, 5);
			drawTextBig(nameX, rowY[i] - 19, entries[i].name, 60, 30, 5);

			// ID (muted)
			drawTextC(nameX + 1, rowY[i] - 37, entries[i].id, 80, 60, 30);
			drawTextC(nameX, rowY[i] - 38, entries[i].id, 80, 60, 30);
		}

		// ---- Decorative bottom rule ----
		setColor(140, 108, 52);
		fillRect(scrollX + 20, scrollY + 22, scrollW - 40, 1);

		// ---- Course / project line ----
		const char* proj = "CSE 1104  -  Structured Programming Lab  -  AUST";
		int projX = 400 - (int)(strlen(proj) * 8) / 2;
		drawTextC(projX + 1, scrollY + 11, proj, 80, 55, 20);
		drawTextC(projX, scrollY + 12, proj, 80, 55, 20);

		// ---- Pulsing "Press B to go back" hint (below scroll) ----
		int pulse_back = (int)(200 + 55 * sin(time * 3));
		const char* hint = "Press B to go back";
		int hintX = 400 - (int)(strlen(hint) * 8) / 2;
		drawTextC(hintX + 1, 21, hint, 0, 0, 0);
		drawTextC(hintX, 20, hint, pulse_back, pulse_back, 0);

		return;
	}

    // ---- CONTROLS PAGE ----
    if (page == 3) {
        // Title (centered, Times Roman 24pt gold)
        const char* ctitle = "CONTROLS";
        int ctX = 400 - (int)(strlen(ctitle) * 13) / 2;
        drawTextBig(ctX + 1, 421, ctitle, 0, 0, 0);
        drawTextBig(ctX,     420, ctitle, 255, 220, 100);

        // Gold divider under title
        setColor(180, 150, 60);
        fillRect(80, 405, 640, 2);

        // 7 control entries — same layout as Main Menu
        // 3 rows of 2 at left (x=200) / right (x=600), then B centred
        const char* ckeys[]  = { "W",       "A",        "S",        "D",         "I",            "ESC",       "B"    };
        const char* cdescs[] = { "Move Up","Move Left","Move Down","Move Right","Quest Scroll","Pause Game","Back"  };
        int ccxs[] = { 200, 600, 200, 600, 200, 600, 400 };
        int ccys[] = { 330, 330, 248, 248, 166, 166,  84 };
        int ccount = 7;


        for (int i = 0; i < ccount; i++) {
            int cx = ccxs[i];
            int cy = ccys[i];
            const char* key  = ckeys[i];
            const char* desc = cdescs[i];

            // Outer gold border frame
            setColor(130, 100, 20);
            fillRect(cx - 141, cy - 27, 282, 56);
            // Dark card background
            setColor(14, 11, 28);
            fillRect(cx - 140, cy - 26, 280, 54);
            // Amber key box background
            setColor(55, 40, 8);
            fillRect(cx - 133, cy - 20, 56, 42);
            // Key box inner gold border accent (top + left highlight)
            setColor(160, 120, 30);
            fillRect(cx - 133, cy - 20, 56, 2);   // top edge
            fillRect(cx - 133, cy - 20, 2, 42);   // left edge

            // Key text – gold bold, centered inside key box (56px wide)
            int klen = (int)strlen(key);
            int kw   = klen * 13;
            int kx   = cx - 133 + (56 - kw) / 2;
            drawTextBig(kx + 1, cy - 13, key, 0, 0, 0);
            drawTextBig(kx,     cy - 13, key, 255, 205, 40);

            // Description text, starts 10px right of key box
            int dx = cx - 133 + 56 + 10;
            drawTextBig(dx + 1, cy - 13, desc, 0, 0, 0);
            drawTextBig(dx,     cy - 13, desc, 185, 185, 185);
        }

        // Bottom gold divider
        setColor(180, 150, 60);
        fillRect(80, 54, 640, 2);


        return;
    }


    // ---- MAIN PAUSE MENU ----
    for (int i = 0; i < PAUSE_BTN_COUNT; i++) {
        int bx = PAUSE_BTN_CX - PAUSE_BTN_W / 2;
        int by = PAUSE_BTN_START_Y - i * PAUSE_BTN_GAP;
        bool hov = (i == selected);
        int texId = btnTex[i][hov ? 1 : 0];
        if (texId >= 0)
            drawChunk(bx, by, PAUSE_BTN_W, PAUSE_BTN_H, texId);
    }

    drawTextC(221, 21, "W/S = move   ENTER/SPACE = select   or click   Esc = Resume", 0, 0, 0);
    drawTextC(220, 20, "W/S = move   ENTER/SPACE = select   or click   Esc = Resume", 200, 200, 200);
}
