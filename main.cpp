// =============================================================================
//  MINI RPG: LEGENDS EDITION  —  Raylib GUI Version
//  Visual Studio 2022 + Raylib 5.x
// =============================================================================

#include "raylib.h"
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
using namespace std;

// ---------------------------------------------------------------------------
// CONSTANTS & TUNABLES
// ---------------------------------------------------------------------------
static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;
static const int FPS      = 60;

static const float ENEMY_HP_MULT  = 1.35f;
static const float ENEMY_DMG_MULT = 1.25f;

// ---------------------------------------------------------------------------
// COLOR PALETTE
// ---------------------------------------------------------------------------
static const Color C_BG       = { 12,  12,  20, 255 };
static const Color C_PANEL    = { 20,  22,  38, 255 };
static const Color C_BORDER   = { 60,  80, 140, 255 };
static const Color C_WHITE    = { 230, 230, 240, 255 };
static const Color C_GOLD     = { 255, 200,  60, 255 };
static const Color C_RED      = { 220,  60,  60, 255 };
static const Color C_GREEN    = { 60,  200,  80, 255 };
static const Color C_CYAN     = { 60,  200, 220, 255 };
static const Color C_PURPLE   = { 180,  80, 220, 255 };
static const Color C_ORANGE   = { 255, 140,  30, 255 };
static const Color C_GREY     = { 80,   80,  90, 255 };
static const Color C_DARKGREY = { 30,   30,  40, 255 };
static const Color C_YELLOW   = { 255, 240,  80, 255 };

// ---------------------------------------------------------------------------
// STRUCTS
// ---------------------------------------------------------------------------
struct Hero {
    string name, cls;
    int hp, maxHp, atk, def, lvl, xp, xpMax, gold, potions;
    int weapon, armor;   // 0=Fists 1=Dagger 2=Sword 3=Axe | 0=None 1=Leather 2=Chain 3=Plate
    int dungeon;         // 0=Greenwood 1=Crypt 2=DragonPeak
    bool poisoned; int pTurns;
    bool q1, q2, q3; int gKills;
    long long reactSumMs; int reactCount;
};

struct Enemy {
    string name, rank;
    int hp, maxHp, atk, def, xpR, goldR;
    bool canPoison;
};

struct Particle {
    Vector2 pos, vel;
    float life, maxLife, size;
    Color color;
};

// ---------------------------------------------------------------------------
// ITEM DATA
// ---------------------------------------------------------------------------
static const char* wName[4] = { "Fists","Dagger","Iron Sword","Battle Axe" };
static const int   wAtk[4]  = { 0, 3, 7, 12 };
static const int   wCrit[4] = { 0,10,15,  8 };
static const int   wCost[4] = { 0,20,60,100 };

static const char* aName[4] = { "No Armor","Leather","Chain Mail","Plate" };
static const int   aDef[4]  = { 0, 3, 6, 11 };
static const int   aCost[4] = { 0,30,70,130 };

static const char* dName[3] = { "Greenwood Forest","Cursed Crypt","Dragon Peak" };
static const int   dMinLv[3]= { 1, 4, 7 };

// ---------------------------------------------------------------------------
// GAME SCREENS
// ---------------------------------------------------------------------------
enum class Screen {
    MAIN_MENU,
    CLASS_SELECT,
    NAME_INPUT,
    HUB,
    EXPLORE,
    BATTLE,
    SHOP,
    STATS,
    QUESTS,
    DUNGEON_SEL,
    GAME_OVER,
    VICTORY,
    LEVEL_UP,
};

// ---------------------------------------------------------------------------
// BATTLE SUB-STATES
// ---------------------------------------------------------------------------
enum class BattlePhase {
    PLAYER_TURN,
    ANIM_PLAYER_ATK,
    ANIM_ENEMY_ATK,
    REFLEX_PROMPT,
    REFLEX_WAITING,
    ENEMY_DEAD,
    HERO_DEAD,
    RUN,
};

// ---------------------------------------------------------------------------
// GLOBAL STATE
// ---------------------------------------------------------------------------
static Screen       gScreen          = Screen::MAIN_MENU;
static Hero         gHero            = {};
static Enemy        gEnemy           = {};
static bool         mimicUp          = false;
static bool         rareUp           = false;

// Battle state
static BattlePhase  gBPhase          = BattlePhase::PLAYER_TURN;
static int          gBTurn           = 0;
static float        gAnimTimer       = 0.f;
static string       gFlashMsg        = "";
static Color        gFlashColor      = C_WHITE;
static int          gHeroXPos        = 200;
static int          gEnemyXPos       = 900;
static int          gHeroTargetX     = 200;
static int          gEnemyTargetX    = 900;
static float        gShakeX          = 0.f;
static float        gShakeTimer      = 0.f;
static int          gBossPhase       = 1;
static bool         gMimicBattle     = false;
static string       gEventMsg        = "";
static bool         gEventPending    = false;

// Reflex block
static float        gReflexWindow    = 0.65f;
static float        gReflexTimer     = 0.f;
static int          gPendingEnemyDmg = 0;

// XP/Gold tally animation
static int          gXpGain          = 0;
static int          gGoldGain        = 0;
static int          gShownXp         = 0;
static int          gShownGold       = 0;

// Particles
static vector<Particle> gParticles;

// UI Text input & selections
static char gInputBuf[64] = {};
static int  gInputLen     = 0;
static int  gSelectedClass= 0;

// Level-up messages queue
static vector<string> gLevelUpMsgs;
static int gLevelUpIdx = 0;

// Notification banner
static string gNotifMsg   = "";
static float  gNotifTimer = 0.f;

// ---------------------------------------------------------------------------
// UTILITY FUNCTIONS
// ---------------------------------------------------------------------------
int rnd(int a, int b) { return a + rand() % (b - a + 1); }

string intStr(int v) { return to_string(v); }

float myLerp(float a, float b, float t) { return a + (b - a) * t; }

int decayDmg(int base, int round) {
    return max(1, (int)(base * exp(-0.05 * round)));
}

int scaleEnemyDamage(int dmg) {
    return max(1, (int)(dmg * ENEMY_DMG_MULT));
}

// ---------------------------------------------------------------------------
// PARTICLES
// ---------------------------------------------------------------------------
void spawnParticles(Vector2 origin, int count, Color color, float speed = 120.f, float life = 0.6f) {
    for (int i = 0; i < count; i++) {
        float angle = (float)rnd(0, 360) * DEG2RAD;
        float v = (float)rnd(40, (int)speed);
        gParticles.push_back({
            origin,
            { cosf(angle) * v, sinf(angle) * v },
            life, life,
            (float)rnd(3, 7),
            color
        });
    }
}

void updateParticles(float dt) {
    for (auto& p : gParticles) {
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.y += 150.f * dt;
        p.life  -= dt;
    }
    gParticles.erase(
        remove_if(gParticles.begin(), gParticles.end(), [](const Particle& p){ return p.life <= 0; }),
        gParticles.end()
    );
}

void drawParticles() {
    for (auto& p : gParticles) {
        float alpha = p.life / p.maxLife;
        Color c = { p.color.r, p.color.g, p.color.b, (unsigned char)(alpha * 255) };
        DrawCircle((int)p.pos.x, (int)p.pos.y, p.size * alpha, c);
    }
}

// ---------------------------------------------------------------------------
// UI DRAWING HELPERS
// ---------------------------------------------------------------------------
void drawShadowText(const char* text, int x, int y, int size, Color color) {
    DrawText(text, x + 2, y + 2, size, { 0, 0, 0, 160 });
    DrawText(text, x, y, size, color);
}

void drawBar(int x, int y, int w, int h, int val, int maxVal, Color fill, Color bg) {
    DrawRectangle(x, y, w, h, bg);
    if (maxVal > 0) {
        int fw = max(0, (int)((float)val / maxVal * w));
        DrawRectangle(x, y, fw, h, fill);
    }
    DrawRectangleLines(x, y, w, h, C_BORDER);
}

void drawPanel(int x, int y, int w, int h, Color border = C_BORDER) {
    DrawRectangle(x, y, w, h, C_PANEL);
    DrawRectangleLines(x, y, w, h, border);
}

void showNotif(const string& msg, float duration = 2.5f) {
    gNotifMsg   = msg;
    gNotifTimer = duration;
}

void drawNotif() {
    if (gNotifTimer <= 0.f) return;
    float alpha = min(1.f, gNotifTimer / 0.4f);
    Color c = { 255, 220, 60, (unsigned char)(alpha * 230) };
    int tw = MeasureText(gNotifMsg.c_str(), 20);
    int nx = SCREEN_W / 2 - tw / 2;
    int ny = 130;
    DrawRectangle(nx - 16, ny, tw + 32, 38, { 20, 20, 30, (unsigned char)(alpha * 210) });
    DrawRectangleLines(nx - 16, ny, tw + 32, 38, c);
    DrawText(gNotifMsg.c_str(), nx, ny + 9, 20, c);
}

void drawBackground() {
    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, { 12, 12, 20, 255 }, { 20, 16, 26, 255 });
}

// ---------------------------------------------------------------------------
// CHARACTER RENDERING
// ---------------------------------------------------------------------------
void drawHeroChar(int x, int y, Color tint) {
    DrawCircle(x, y - 40, 14, tint);
    DrawRectangle(x - 8, y - 26, 16, 30, tint);
    DrawRectangle(x - 8, y + 4, 7, 22, tint);
    DrawRectangle(x + 1, y + 4, 7, 22, tint);
    DrawRectangle(x - 18, y - 24, 10, 22, tint);
    DrawRectangle(x + 8, y - 24, 10, 22, tint);
    DrawCircle(x - 5, y - 43, 3, WHITE);
    DrawCircle(x + 5, y - 43, 3, WHITE);
    DrawCircle(x - 4, y - 43, 1, DARKBLUE);
    DrawCircle(x + 6, y - 43, 1, DARKBLUE);
}

void drawEnemyChar(int x, int y, const string& rank, Color tint) {
    if (rank == "Boss") {
        DrawCircle(x, y - 56, 20, tint);
        DrawRectangle(x - 14, y - 36, 28, 40, tint);
        DrawRectangle(x - 14, y + 4, 12, 28, tint);
        DrawRectangle(x + 2, y + 4, 12, 28, tint);
        DrawRectangle(x - 28, y - 34, 14, 30, tint);
        DrawRectangle(x + 14, y - 34, 14, 30, tint);
        DrawCircle(x - 7, y - 60, 4, RED);
        DrawCircle(x + 7, y - 60, 4, RED);
        DrawCircle(x - 7, y - 60, 2, YELLOW);
        DrawCircle(x + 7, y - 60, 2, YELLOW);
        DrawTriangle({ (float)x - 14, (float)y - 76 }, { (float)x - 8, (float)y - 56 }, { (float)x - 4, (float)y - 76 }, tint);
        DrawTriangle({ (float)x - 2,  (float)y - 82 }, { (float)x + 2, (float)y - 56 }, { (float)x + 4, (float)y - 82 }, YELLOW);
        DrawTriangle({ (float)x + 4,  (float)y - 76 }, { (float)x + 8, (float)y - 56 }, { (float)x + 14, (float)y - 76 }, tint);
    } else {
        DrawCircle(x, y - 40, 14, tint);
        DrawRectangle(x - 9, y - 26, 18, 28, tint);
        DrawRectangle(x - 9, y + 2, 8, 20, tint);
        DrawRectangle(x + 1, y + 2, 8, 20, tint);
        DrawRectangle(x - 20, y - 24, 11, 20, tint);
        DrawRectangle(x + 9, y - 24, 11, 20, tint);
        DrawCircle(x - 5, y - 43, 3, RED);
        DrawCircle(x + 5, y - 43, 3, RED);
    }
}

Color enemyColor(const string& rank) {
    if (rank == "Boss")    return { 220,  60,  80, 255 };
    if (rank == "Strong")  return { 200,  80,  60, 255 };
    if (rank == "UNIQUE")  return { 180,  60, 220, 255 };
    if (rank == "JACKPOT") return { 255, 200,  40, 255 };
    return { 160, 120, 80, 255 };
}

// ---------------------------------------------------------------------------
// ENEMY SPAWNING & HEALTH
// ---------------------------------------------------------------------------
Enemy boostEnemyHealth(Enemy e) {
    e.maxHp = max(1, (int)(e.maxHp * ENEMY_HP_MULT));
    e.hp = e.maxHp;
    return e;
}

Enemy spawnEnemy(int lvl, int dng) {
    if (mimicUp) {
        mimicUp = false;
        return boostEnemyHealth({ "Golden Mimic", "JACKPOT", 40 + lvl * 4, 40 + lvl * 4, 0, 1, 5, 300, false });
    }
    if (rareUp) {
        rareUp = false;
        return boostEnemyHealth({ "Shadow Assassin", "UNIQUE", 35 + lvl * 5, 35 + lvl * 5, 14 + lvl, 2, 90, 120, true });
    }
    Enemy g[3][4] = {
        { { "Slime", "Weak", 18 + lvl * 3, 18 + lvl * 3, 6 + lvl, 0, 12, 6, false },
          { "Goblin", "Normal", 27 + lvl * 4, 27 + lvl * 4, 10 + lvl, 1, 24, 12, false },
          { "Orc", "Strong", 45 + lvl * 6, 45 + lvl * 6, 17 + lvl, 3, 42, 18, false },
          { "Troll", "Strong", 50 + lvl * 6, 50 + lvl * 6, 15 + lvl, 4, 36, 22, false } },
        { { "Skeleton", "Normal", 33 + lvl * 4, 33 + lvl * 4, 13 + lvl, 2, 30, 14, true },
          { "Zombie", "Normal", 40 + lvl * 4, 40 + lvl * 4, 12 + lvl, 1, 34, 17, false },
          { "Wraith", "Strong", 50 + lvl * 6, 50 + lvl * 6, 19 + lvl, 3, 48, 24, true },
          { "Lich", "Boss", 80 + lvl * 7, 80 + lvl * 7, 25 + lvl, 5, 85, 42, true } },
        { { "Fire Imp", "Normal", 37 + lvl * 4, 37 + lvl * 4, 17 + lvl, 2, 36, 18, false },
          { "Wyvern", "Strong", 58 + lvl * 6, 58 + lvl * 6, 22 + lvl, 4, 60, 30, false },
          { "Dragon", "Boss", 90 + lvl * 7, 90 + lvl * 7, 28 + lvl, 6, 96, 48, false },
          { "Demon Lord", "Boss", 115 + lvl * 8, 115 + lvl * 8, 34 + lvl, 7, 120, 60, true } }
    };
    return boostEnemyHealth(g[dng][rnd(0, min(3, lvl / 2))]);
}

// ---------------------------------------------------------------------------
// SAVE / LOAD SYSTEM
// ---------------------------------------------------------------------------
void saveGame(const Hero& h) {
    ofstream f("save.txt");
    f << h.name  << "\n" << h.cls << "\n"
      << h.hp    << " " << h.maxHp << " " << h.atk << " " << h.def << "\n"
      << h.lvl   << " " << h.xp   << " " << h.xpMax << "\n"
      << h.gold  << " " << h.potions << "\n"
      << h.weapon<< " " << h.armor   << " " << h.dungeon << "\n"
      << h.poisoned << " " << h.pTurns << "\n"
      << h.q1 << " " << h.q2 << " " << h.q3 << " " << h.gKills << "\n"
      << h.reactSumMs << " " << h.reactCount << "\n";
}

bool loadGame(Hero& h) {
    ifstream f("save.txt");
    if (!f) return false;
    if (!getline(f, h.name) || !getline(f, h.cls)) return false;
    if (!(f >> h.hp >> h.maxHp >> h.atk >> h.def))       return false;
    if (!(f >> h.lvl >> h.xp >> h.xpMax))                return false;
    if (!(f >> h.gold >> h.potions))                      return false;
    if (!(f >> h.weapon >> h.armor >> h.dungeon))         return false;
    if (!(f >> h.poisoned >> h.pTurns))                   return false;
    if (!(f >> h.q1 >> h.q2 >> h.q3 >> h.gKills))        return false;
    if (!(f >> h.reactSumMs >> h.reactCount)) {
        h.reactSumMs = 0;
        h.reactCount = 0;
    }
    return true;
}

void applyClassBonus(Hero& h) {
    if      (h.cls == "Warrior") { h.maxHp += 20; h.hp = h.maxHp; h.def += 3; }
    else if (h.cls == "Mage")    { h.atk += 6; h.maxHp -= 10; h.hp = h.maxHp; }
    else                         { h.atk += 3; }
}

void lvlUp(Hero& h) {
    while (h.xp >= h.xpMax) {
        h.xp -= h.xpMax;
        h.lvl++;
        h.xpMax = h.lvl * 15;
        h.maxHp += 10;
        h.hp = h.maxHp;
        h.atk += 3;
        h.def += 2;
        gLevelUpMsgs.push_back("LEVEL UP!  Now Lv." + intStr(h.lvl) +
            "  MaxHP+10  ATK+3  DEF+2");
    }
}

// ---------------------------------------------------------------------------
// BATTLE INITIALIZATION
// ---------------------------------------------------------------------------
void startBattle(Hero& h, Enemy e) {
    gEnemy           = e;
    gBPhase          = BattlePhase::PLAYER_TURN;
    gBTurn           = 0;
    gAnimTimer       = 0.f;
    gFlashMsg        = "";
    gHeroXPos        = 200;
    gEnemyXPos       = 900;
    gHeroTargetX     = 200;
    gEnemyTargetX    = 900;
    gShakeX          = 0.f;
    gShakeTimer      = 0.f;
    gBossPhase       = 1;
    gMimicBattle     = (e.name == "Golden Mimic");
    gReflexTimer     = 0.f;
    gPendingEnemyDmg = 0;
    gXpGain          = e.xpR;
    gGoldGain        = e.goldR;
    gShownXp         = 0;
    gShownGold       = 0;
    gScreen          = Screen::BATTLE;
}

// ---------------------------------------------------------------------------
// RANDOM EVENTS
// ---------------------------------------------------------------------------
void triggerRandomEvent(Hero& h) {
    if (rnd(1, 10) > 4) {
        gEventPending = false;
        gEventMsg = "";
        return;
    }
    int e = rnd(1, 6);
    if      (e == 1) { int g = rnd(10, 20); h.gold += g; gEventMsg = "Found a coin pouch! +" + intStr(g) + "G"; }
    else if (e == 2) { h.hp = h.maxHp; gEventMsg = "Glowing spring! HP fully restored!"; }
    else if (e == 3) { int d = rnd(3, 8); h.hp = max(1, h.hp - d); gEventMsg = "Triggered a trap! -" + intStr(d) + " HP"; }
    else if (e == 4) { h.atk += 2; gEventMsg = "A sage blesses your weapon! +2 ATK"; }
    else if (e == 5) { rareUp = true;  gEventMsg = "Cursed zone! A dark presence lingers..."; }
    else             { mimicUp = true; gEventMsg = "A chest begins to shake... MIMIC!"; }
    gEventPending = true;
}

// ---------------------------------------------------------------------------
// SCREEN DRAWING FUNCTIONS
// ---------------------------------------------------------------------------
void drawMainMenu() {
    drawBackground();
    DrawText("MINI RPG", SCREEN_W / 2 - MeasureText("MINI RPG", 90) / 2 + 3, 103, 90, { 40, 20, 80, 200 });
    DrawText("MINI RPG", SCREEN_W / 2 - MeasureText("MINI RPG", 90) / 2, 100, 90, C_PURPLE);
    drawShadowText("LEGENDS EDITION", SCREEN_W / 2 - MeasureText("LEGENDS EDITION", 36) / 2, 200, 36, C_GOLD);
    DrawLine(SCREEN_W / 2 - 200, 250, SCREEN_W / 2 + 200, 250, C_BORDER);

    struct Btn { const char* label; int y; Color col; };
    Btn btns[] = {
        { "[ 1 ]  New Game",  300, C_GREEN },
        { "[ 2 ]  Load Game", 380, C_CYAN  },
        { "[ ESC ]  Quit",    460, C_GREY  }
    };
    for (auto& b : btns) {
        int tw = MeasureText(b.label, 30);
        DrawRectangle(SCREEN_W / 2 - 160, b.y - 10, 320, 50, C_PANEL);
        DrawRectangleLines(SCREEN_W / 2 - 160, b.y - 10, 320, 50, b.col);
        drawShadowText(b.label, SCREEN_W / 2 - tw / 2, b.y, 30, b.col);
    }

    const char* footer = "Press 1 for New Game  |  2 for Load Game  |  ESC to Quit";
    DrawText(footer, SCREEN_W / 2 - MeasureText(footer, 16) / 2, SCREEN_H - 40, 16, C_GREY);
}

void drawClassSelect() {
    drawBackground();
    drawShadowText("CHOOSE YOUR CLASS", SCREEN_W / 2 - MeasureText("CHOOSE YOUR CLASS", 40) / 2, 60, 40, C_GOLD);

    struct ClassInfo { const char* name; const char* desc1; const char* desc2; Color col; };
    ClassInfo classes[3] = {
        { "WARRIOR", "+20 HP  +3 DEF",  "Steady frontline fighter", C_RED    },
        { "MAGE",    "+6 ATK  -10 HP",  "Magic scales with level",  C_PURPLE },
        { "ARCHER",  "+3 ATK  +30% Crit","Fast and precise",         C_GREEN  },
    };

    for (int i = 0; i < 3; i++) {
        int cx = 180 + i * 330;
        int cy = 260;
        bool sel = (gSelectedClass == i + 1);
        Color border = sel ? classes[i].col : C_BORDER;
        DrawRectangle(cx - 120, cy - 20, 240, 220, sel ? C_DARKGREY : C_PANEL);
        DrawRectangleLines(cx - 120, cy - 20, 240, 220, border);
        if (sel) DrawRectangle(cx - 120, cy - 20, 240, 4, border);

        drawHeroChar(cx, cy + 80, sel ? classes[i].col : C_GREY);

        int tw = MeasureText(classes[i].name, 24);
        drawShadowText(classes[i].name, cx - tw / 2, cy, 24, sel ? classes[i].col : C_WHITE);
        DrawText(classes[i].desc1, cx - MeasureText(classes[i].desc1, 16) / 2, cy + 140, 16, sel ? C_WHITE : C_GREY);
        DrawText(classes[i].desc2, cx - MeasureText(classes[i].desc2, 14) / 2, cy + 162, 14, C_GREY);
        DrawText(sel ? "SELECTED" : ("[ " + to_string(i + 1) + " ]").c_str(),
                 cx - MeasureText(sel ? "SELECTED" : ("[ " + to_string(i + 1) + " ]").c_str(), 18) / 2,
                 cy + 188, 18, sel ? classes[i].col : C_GREY);
    }

    DrawText("Press 1 / 2 / 3 to select, then ENTER to confirm",
             SCREEN_W / 2 - MeasureText("Press 1 / 2 / 3 to select, then ENTER to confirm", 18) / 2,
             SCREEN_H - 60, 18, C_GREY);
}

void drawNameInput() {
    drawBackground();
    drawShadowText("NAME YOUR HERO", SCREEN_W / 2 - MeasureText("NAME YOUR HERO", 40) / 2, 200, 40, C_GOLD);
    DrawText("Letters only. Press ENTER to confirm.", SCREEN_W / 2 - MeasureText("Letters only. Press ENTER to confirm.", 20) / 2, 260, 20, C_GREY);

    int bx = SCREEN_W / 2 - 200, by = 320;
    DrawRectangle(bx, by, 400, 55, C_PANEL);
    DrawRectangleLines(bx, by, 400, 55, C_CYAN);
    string display = string(gInputBuf) + "_";
    DrawText(display.c_str(), bx + 12, by + 13, 30, C_WHITE);
}

void drawHub() {
    drawBackground();
    Hero& h = gHero;

    // Header
    DrawRectangle(0, 0, SCREEN_W, 54, C_DARKGREY);
    DrawLine(0, 54, SCREEN_W, 54, C_BORDER);
    drawShadowText("MINI RPG: LEGENDS EDITION", 24, 14, 28, C_GOLD);
    const char* dun = dName[h.dungeon];
    DrawText(dun, SCREEN_W - MeasureText(dun, 22) - 24, 18, 22, C_CYAN);

    // Hero stats panel (left)
    drawPanel(20, 70, 360, 340);
    drawShadowText(h.name.c_str(), 36, 84, 26, C_CYAN);
    string clslvl = h.cls + "  Lv." + intStr(h.lvl);
    DrawText(clslvl.c_str(), 36, 114, 20, C_WHITE);

    // HP Bar
    DrawText("HP", 36, 150, 18, C_WHITE);
    Color hpCol = h.hp <= h.maxHp / 4 ? C_RED : h.hp <= h.maxHp / 2 ? C_ORANGE : C_GREEN;
    drawBar(80, 153, 240, 18, h.hp, h.maxHp, hpCol, C_DARKGREY);
    string hpTxt = intStr(h.hp) + "/" + intStr(h.maxHp);
    DrawText(hpTxt.c_str(), 326, 150, 16, hpCol);

    // XP Bar
    DrawText("XP", 36, 182, 18, C_WHITE);
    drawBar(80, 185, 240, 14, h.xp, h.xpMax, C_PURPLE, C_DARKGREY);
    string xpTxt = intStr(h.xp) + "/" + intStr(h.xpMax);
    DrawText(xpTxt.c_str(), 326, 182, 14, C_PURPLE);

    // Attributes
    auto stat = [&](const char* lbl, int val, int x, int y, Color c){
        DrawText(lbl, x, y, 16, C_GREY);
        drawShadowText(intStr(val).c_str(), x + MeasureText(lbl, 16) + 6, y, 16, c);
    };
    stat("ATK:", h.atk,  36, 215, C_RED);
    stat("DEF:", h.def, 140, 215, C_CYAN);
    stat("Gold:", h.gold, 36, 240, C_GOLD);
    stat("Potions:", h.potions, 180, 240, C_GREEN);

    DrawText(("Weapon: " + string(wName[h.weapon])).c_str(), 36, 272, 16, C_WHITE);
    DrawText(("Armor:  " + string(aName[h.armor])).c_str(),  36, 294, 16, C_WHITE);

    if (h.poisoned) DrawText("*** POISONED ***", 36, 326, 18, C_PURPLE);
    else            DrawText("Status: Healthy",  36, 326, 16, C_GREEN);

    drawHeroChar(320, 290, C_CYAN);

    // Navigation Menu (right)
    struct MenuBtn { int key; const char* label; const char* hint; Color col; };
    MenuBtn btns[] = {
        { 1, "Explore", "Fight enemies and encounter fate events", C_GREEN  },
        { 2, "Shop",    "Buy weapons, armor, and potions",         C_GOLD   },
        { 3, "Stats",   "View character build and reflex log",     C_CYAN   },
        { 4, "Quests",  "Track goals and claim rewards",           C_PURPLE },
        { 5, "Dungeon", "Switch challenge dungeon",                C_ORANGE },
        { 6, "Save",    "Save current game progress",              C_WHITE  },
        { 0, "Quit",    "Exit to desktop",                         C_RED    },
    };
    int bx = 420, by0 = 80;
    for (int i = 0; i < 7; i++) {
        auto& b = btns[i];
        int bby = by0 + i * 78;
        DrawRectangle(bx, bby, 820, 66, C_PANEL);
        DrawRectangleLines(bx, bby, 820, 66, b.col);
        string keyLbl = "[ " + (b.key == 0 ? string("0") : intStr(b.key)) + " ]  " + b.label;
        drawShadowText(keyLbl.c_str(), bx + 20, bby + 10, 22, b.col);
        DrawText(b.hint, bx + 20, bby + 38, 16, C_GREY);
    }
}

void drawBattle() {
    Hero& h  = gHero;
    Enemy& e = gEnemy;

    Color topCol    = (h.dungeon == 0) ? Color{ 10, 30, 10, 255 } :
                      (h.dungeon == 1) ? Color{ 20, 10, 30, 255 } :
                                         Color{ 30, 10, 10, 255 };
    Color bottomCol = (h.dungeon == 0) ? Color{ 20, 40, 20, 255 } :
                      (h.dungeon == 1) ? Color{ 35, 15, 50, 255 } :
                                         Color{ 50, 15, 15, 255 };
    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, topCol, bottomCol);

    DrawRectangle(0, 500, SCREEN_W, 4, C_BORDER);
    DrawRectangle(0, 504, SCREEN_W, SCREEN_H - 504, { 20, 16, 14, 255 });

    int sx = (gShakeTimer > 0) ? rnd(-(int)gShakeX, (int)gShakeX) : 0;

    // Hero HUD
    drawPanel(10, 10, 380, 110);
    drawShadowText(h.name.c_str(), 24, 22, 22, C_CYAN);
    DrawText(("Lv." + intStr(h.lvl) + "  " + h.cls).c_str(), 24, 48, 16, C_WHITE);
    DrawText("HP", 24, 72, 16, C_WHITE);
    Color hpCol = h.hp <= h.maxHp / 4 ? C_RED : h.hp <= h.maxHp / 2 ? C_ORANGE : C_GREEN;
    drawBar(60, 75, 280, 20, h.hp, h.maxHp, hpCol, C_DARKGREY);
    DrawText((intStr(h.hp) + "/" + intStr(h.maxHp)).c_str(), 346, 72, 15, hpCol);
    if (h.poisoned) DrawText(("PSN (" + intStr(h.pTurns) + " turns left)").c_str(), 24, 96, 14, C_PURPLE);

    // Enemy HUD
    drawPanel(SCREEN_W - 390, 10, 380, 110, enemyColor(e.rank));
    drawShadowText(e.name.c_str(), SCREEN_W - 376, 22, 22, enemyColor(e.rank));
    DrawText(("[" + e.rank + "]").c_str(), SCREEN_W - 376, 48, 16, C_WHITE);
    DrawText("HP", SCREEN_W - 376, 72, 16, C_WHITE);
    Color eHpCol = e.hp <= e.maxHp / 4 ? C_RED : e.hp <= e.maxHp / 2 ? C_ORANGE : C_GREEN;
    drawBar(SCREEN_W - 340, 75, 280, 20, e.hp, e.maxHp, eHpCol, C_DARKGREY);
    DrawText((intStr(e.hp) + "/" + intStr(e.maxHp)).c_str(), SCREEN_W - 56, 72, 15, eHpCol);

    // Fighters
    drawHeroChar(gHeroXPos + sx, 460, C_CYAN);
    drawEnemyChar(gEnemyXPos + sx, 460, e.rank, enemyColor(e.rank));

    // Weapon glyph
    const char* wGlyphs[4] = { "o", "/>", "/|", "/X" };
    DrawText(wGlyphs[h.weapon], gHeroXPos + 20 + sx, 430, 22, C_GOLD);

    // Combat floating text
    if (!gFlashMsg.empty()) {
        int tw = MeasureText(gFlashMsg.c_str(), 32);
        int fx = SCREEN_W / 2 - tw / 2 + sx;
        DrawRectangle(fx - 20, 200, tw + 40, 54, { 0, 0, 0, 180 });
        DrawRectangleLines(fx - 20, 200, tw + 40, 54, gFlashColor);
        drawShadowText(gFlashMsg.c_str(), fx, 211, 32, gFlashColor);
    }

    // Mimic countdown status
    if (gMimicBattle) {
        DrawText(("MIMIC ESCAPES IN: " + intStr(max(0, 5 - gBTurn)) + " TURNS").c_str(), 20, 130, 18, C_GOLD);
    }

    // Battle menu
    if (gBPhase == BattlePhase::PLAYER_TURN) {
        drawPanel(400, 530, 480, 170, C_BORDER);
        drawShadowText("YOUR TURN", 420, 542, 18, C_GOLD);
        string potLbl = "[ 2 ]  Potion (" + intStr(h.potions) + ")";
        DrawText("[ 1 ]  Attack",   420, 568, 22, C_RED);
        DrawText(potLbl.c_str(),    420, 596, 22, h.potions > 0 ? C_GREEN : C_GREY);
        DrawText("[ 3 ]  Run Away", 420, 624, 22, C_GREY);
        DrawText(("WPN: " + string(wName[h.weapon]) + "  ARM: " + aName[h.armor]).c_str(), 420, 655, 15, C_GREY);
        DrawText(("Turn " + intStr(gBTurn + 1)).c_str(), 700, 655, 15, C_GREY);
    }

    // Reflex blocking prompt
    if (gBPhase == BattlePhase::REFLEX_PROMPT || gBPhase == BattlePhase::REFLEX_WAITING) {
        DrawRectangle(300, 520, 680, 140, { 20, 0, 10, 230 });
        DrawRectangleLines(300, 520, 680, 140, C_RED);
        drawShadowText("!!! INCOMING ATTACK !!!", SCREEN_W / 2 - MeasureText("!!! INCOMING ATTACK !!!", 28) / 2, 536, 28, C_RED);
        if (gBPhase == BattlePhase::REFLEX_WAITING) {
            drawShadowText(">>> PRESS SPACE TO BLOCK <<<", SCREEN_W / 2 - MeasureText(">>> PRESS SPACE TO BLOCK <<<", 24) / 2, 572, 24, C_YELLOW);
            float pct = max(0.f, gReflexTimer / gReflexWindow);
            DrawRectangle(320, 620, 640, 22, C_DARKGREY);
            DrawRectangle(320, 620, (int)(640 * pct), 22, pct > 0.6f ? C_GREEN : pct > 0.3f ? C_ORANGE : C_RED);
            DrawRectangleLines(320, 620, 640, 22, C_BORDER);
        }
    }

    drawParticles();
}

void drawShop() {
    drawBackground();
    Hero& h = gHero;
    drawShadowText("SHOP", SCREEN_W / 2 - MeasureText("SHOP", 48) / 2, 30, 48, C_GOLD);
    DrawText(("Gold: " + intStr(h.gold)).c_str(), 50, 32, 26, C_GOLD);

    // Weapons
    drawPanel(50, 110, 560, 480, C_BORDER);
    drawShadowText("WEAPONS", 70, 126, 22, C_RED);
    for (int i = 1; i < 4; i++) {
        int cy = 166 + (i - 1) * 110;
        bool canBuy = h.gold >= wCost[i];
        bool owned  = h.weapon == i;
        Color bc = owned ? C_GOLD : (canBuy ? C_RED : C_GREY);
        DrawRectangle(70, cy, 520, 94, owned ? Color{ 40, 30, 10, 255 } : C_DARKGREY);
        DrawRectangleLines(70, cy, 520, 94, bc);
        drawShadowText((string("[") + intStr(i) + "]  " + wName[i]).c_str(), 90, cy + 8, 22, bc);
        DrawText(("ATK +" + intStr(wAtk[i]) + "   Crit " + intStr(wCrit[i]) + "%").c_str(), 90, cy + 38, 16, C_WHITE);
        DrawText((intStr(wCost[i]) + "G").c_str(), 90, cy + 62, 16, canBuy ? C_GOLD : C_GREY);
        if (owned) DrawText("EQUIPPED", 400, cy + 62, 16, C_GOLD);
    }

    // Armor
    drawPanel(650, 110, 580, 480, C_BORDER);
    drawShadowText("ARMOR", 670, 126, 22, C_CYAN);
    for (int i = 1; i < 4; i++) {
        int cy = 166 + (i - 1) * 110;
        bool canBuy = h.gold >= aCost[i];
        bool owned  = h.armor == i;
        Color bc = owned ? C_GOLD : (canBuy ? C_CYAN : C_GREY);
        DrawRectangle(670, cy, 540, 94, owned ? Color{ 10, 30, 40, 255 } : C_DARKGREY);
        DrawRectangleLines(670, cy, 540, 94, bc);
        drawShadowText((string("[") + intStr(i + 3) + "]  " + aName[i]).c_str(), 690, cy + 8, 22, bc);
        DrawText(("DEF +" + intStr(aDef[i])).c_str(), 690, cy + 38, 16, C_WHITE);
        DrawText((intStr(aCost[i]) + "G").c_str(), 690, cy + 62, 16, canBuy ? C_GOLD : C_GREY);
        if (owned) DrawText("EQUIPPED", 1090, cy + 62, 16, C_GOLD);
    }

    // Potions
    int py = 620;
    DrawRectangle(50, py, 560, 64, C_DARKGREY);
    DrawRectangleLines(50, py, 560, 64, h.gold >= 15 ? C_GREEN : C_GREY);
    DrawText("[ 7 ]  Potion — Restore 20-35 HP  |  15G", 70, py + 12, 18, h.gold >= 15 ? C_GREEN : C_GREY);
    DrawText(("You have " + intStr(h.potions) + " potions").c_str(), 70, py + 38, 14, C_GREY);

    DrawText("[ 0 ]  Back to Hub", 50, SCREEN_H - 44, 20, C_GREY);
}

void drawStats() {
    drawBackground();
    drawShadowText("HERO STATS", SCREEN_W / 2 - MeasureText("HERO STATS", 42) / 2, 30, 42, C_GOLD);

    Hero& h = gHero;
    drawPanel(60, 100, 500, 520, C_BORDER);
    auto row = [&](const char* label, const string& val, int y, Color vc = C_WHITE) {
        DrawText(label, 80, y, 20, C_GREY);
        DrawText(val.c_str(), 280, y, 20, vc);
    };

    row("Name:",    h.name,            110, C_CYAN);
    row("Class:",   h.cls,             140, C_WHITE);
    row("Level:",   intStr(h.lvl),     170, C_GOLD);
    row("HP:",      intStr(h.hp) + "/" + intStr(h.maxHp), 200, h.hp <= h.maxHp / 4 ? C_RED : C_GREEN);
    row("ATK:",     intStr(h.atk),     230, C_RED);
    row("DEF:",     intStr(h.def),     260, C_CYAN);
    row("XP:",      intStr(h.xp) + "/" + intStr(h.xpMax), 290, C_PURPLE);
    row("Gold:",    intStr(h.gold),    320, C_GOLD);
    row("Potions:", intStr(h.potions), 350, C_GREEN);
    row("Weapon:",  wName[h.weapon],   380, C_ORANGE);
    row("Armor:",   aName[h.armor],    410, C_ORANGE);
    row("Dungeon:", dName[h.dungeon],  440, C_CYAN);
    row("Status:",  h.poisoned ? "POISONED" : "Healthy", 470, h.poisoned ? C_PURPLE : C_GREEN);

    DrawLine(80, 506, 540, 506, C_BORDER);
    DrawText("QUESTS", 80, 512, 16, C_GOLD);
    DrawText(("Q1 Goblin Slayer: " + string(h.q1 ? "DONE" : intStr(h.gKills) + "/3 Goblins")).c_str(), 80, 532, 16, h.q1 ? C_GOLD : C_GREY);
    DrawText(("Q2 Rising Hero:  " + string(h.q2 ? "DONE" : "Need Lv5")).c_str(), 80, 552, 16, h.q2 ? C_GOLD : C_GREY);
    DrawText(("Q3 Dragon Slayer: " + string(h.q3 ? "DONE" : "Slay the Dragon")).c_str(), 80, 572, 16, h.q3 ? C_GOLD : C_GREY);

    // Reflex records
    drawPanel(600, 100, 620, 220, C_BORDER);
    drawShadowText("REFLEX PERFORMANCE", 620, 116, 22, C_CYAN);
    if (h.reactCount > 0) {
        double avg = (double)h.reactSumMs / h.reactCount;
        string tier = avg < 200 ? "Lightning Reflexes!" : avg < 350 ? "Sharp Reflexes" : avg < 500 ? "Steady Guard" : "Room for Practice";
        DrawText(("Average: " + intStr((int)avg) + " ms").c_str(), 620, 152, 20, C_WHITE);
        DrawText(("Successful Blocks: " + intStr(h.reactCount)).c_str(), 620, 178, 18, C_GREY);
        drawShadowText(tier.c_str(), 620, 210, 22, C_GOLD);
    } else {
        DrawText("No blocks recorded yet.", 620, 152, 18, C_GREY);
        DrawText("Block incoming enemy attacks\nto record reflex telemetry!", 620, 180, 16, C_GREY);
    }

    drawHeroChar(1100, 420, C_CYAN);
    DrawText("[ ENTER / ESC ]  Back", 60, SCREEN_H - 44, 20, C_GREY);
}

void drawQuests() {
    drawBackground();
    drawShadowText("QUEST BOARD", SCREEN_W / 2 - MeasureText("QUEST BOARD", 44) / 2, 30, 44, C_GOLD);
    Hero& h = gHero;

    struct QuestRow { const char* title; string progress; bool done; const char* reward; };
    QuestRow qs[] = {
        { "Goblin Slayer", intStr(h.gKills) + "/3 Goblins killed", h.q1, "+80 Gold, +2 Potions" },
        { "Rising Hero",   "Reach Level 5 (Lv." + intStr(h.lvl) + ")", h.q2, "+4 ATK, +2 DEF" },
        { "Dragon Slayer", "Defeat the Dragon in Dragon Peak",   h.q3, "+15 MaxHP" },
    };
    for (int i = 0; i < 3; i++) {
        auto& q = qs[i];
        int qy = 130 + i * 160;
        Color bc = q.done ? C_GOLD : C_BORDER;
        DrawRectangle(60, qy, SCREEN_W - 120, 140, C_PANEL);
        DrawRectangleLines(60, qy, SCREEN_W - 120, 140, bc);
        if (q.done) DrawRectangle(60, qy, SCREEN_W - 120, 6, C_GOLD);

        drawShadowText((string("[Q") + intStr(i + 1) + "]  " + q.title).c_str(), 86, qy + 14, 26, q.done ? C_GOLD : C_WHITE);
        DrawText(q.progress.c_str(), 86, qy + 52, 20, q.done ? C_GOLD : C_GREY);
        DrawText(("Reward: " + string(q.reward)).c_str(), 86, qy + 80, 18, C_CYAN);
        if (q.done) DrawText("COMPLETED!", SCREEN_W - 260, qy + 60, 24, C_GOLD);
    }

    DrawText("[ ENTER / ESC ]  Back", 60, SCREEN_H - 44, 20, C_GREY);
}

void drawDungeonSelect() {
    drawBackground();
    drawShadowText("CHOOSE DUNGEON", SCREEN_W / 2 - MeasureText("CHOOSE DUNGEON", 44) / 2, 30, 44, C_GOLD);
    Hero& h = gHero;
    Color dCols[3] = { C_GREEN, C_PURPLE, C_RED };
    for (int i = 0; i < 3; i++) {
        int dy = 130 + i * 160;
        bool unlocked = h.lvl >= dMinLv[i];
        bool current  = h.dungeon == i;
        Color bc = current ? C_GOLD : (unlocked ? dCols[i] : C_GREY);
        DrawRectangle(80, dy, SCREEN_W - 160, 140, C_PANEL);
        DrawRectangleLines(80, dy, SCREEN_W - 160, 140, bc);
        if (current) DrawRectangle(80, dy, SCREEN_W - 160, 6, C_GOLD);

        drawShadowText((string("[") + intStr(i + 1) + "]  " + dName[i]).c_str(), 110, dy + 14, 28, unlocked ? bc : C_GREY);
        DrawText(("Min Level: " + intStr(dMinLv[i])).c_str(), 110, dy + 54, 18, unlocked ? C_WHITE : C_GREY);
        if (!unlocked) DrawText("LOCKED — Level up first!", SCREEN_W - 400, dy + 54, 18, C_GREY);
        if (current)   DrawText("CURRENT", SCREEN_W - 260, dy + 54, 22, C_GOLD);
    }
    DrawText("[ ESC ]  Back", 80, SCREEN_H - 44, 20, C_GREY);
}

void drawExplore() {
    drawBackground();
    drawPanel(SCREEN_W / 2 - 350, SCREEN_H / 2 - 130, 700, 260, C_GOLD);
    drawShadowText("FATE ENCOUNTER", SCREEN_W / 2 - MeasureText("FATE ENCOUNTER", 32) / 2, SCREEN_H / 2 - 112, 32, C_GOLD);
    DrawLine(SCREEN_W / 2 - 310, SCREEN_H / 2 - 68, SCREEN_W / 2 + 310, SCREEN_H / 2 - 68, C_BORDER);
    DrawText(gEventMsg.c_str(), SCREEN_W / 2 - MeasureText(gEventMsg.c_str(), 22) / 2, SCREEN_H / 2 - 48, 22, C_WHITE);
    DrawText("[ ENTER / SPACE ]  Continue to Battle", SCREEN_W / 2 - MeasureText("[ ENTER / SPACE ]  Continue to Battle", 18) / 2, SCREEN_H / 2 + 60, 18, C_GREY);
}

void drawGameOver() {
    drawBackground();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, { 0, 0, 0, 160 });
    drawShadowText("DEFEATED", SCREEN_W / 2 - MeasureText("DEFEATED", 60) / 2, 180, 60, C_RED);
    DrawText(("You have fallen, " + gHero.name + "...").c_str(),
             SCREEN_W / 2 - MeasureText(("You have fallen, " + gHero.name + "...").c_str(), 24) / 2, 280, 24, C_WHITE);
    DrawText("The village healer drags you back to safety.", SCREEN_W / 2 - MeasureText("The village healer drags you back to safety.", 20) / 2, 320, 20, C_GREY);
    DrawText(("HP restored to " + intStr(gHero.hp) + "/" + intStr(gHero.maxHp)).c_str(),
             SCREEN_W / 2 - MeasureText(("HP restored to " + intStr(gHero.hp) + "/" + intStr(gHero.maxHp)).c_str(), 20) / 2, 356, 20, C_GREEN);
    DrawText("[ ENTER ]  Return to Hub", SCREEN_W / 2 - MeasureText("[ ENTER ]  Return to Hub", 22) / 2, 460, 22, C_GOLD);
}

void drawVictory() {
    drawBackground();
    drawShadowText("V I C T O R Y !", SCREEN_W / 2 - MeasureText("V I C T O R Y !", 52) / 2, 140, 52, C_GOLD);
    drawHeroChar(SCREEN_W / 2, 350, C_CYAN);
    DrawText((gHero.name + " STANDS TRIUMPHANT!").c_str(),
             SCREEN_W / 2 - MeasureText((gHero.name + " STANDS TRIUMPHANT!").c_str(), 26) / 2, 420, 26, C_WHITE);
    DrawText(("+" + intStr(gShownXp) + " XP").c_str(), SCREEN_W / 2 - 120, 470, 28, C_PURPLE);
    DrawText(("+" + intStr(gShownGold) + " Gold").c_str(), SCREEN_W / 2 + 20, 470, 28, C_GOLD);

    if (!gLevelUpMsgs.empty() && gLevelUpIdx < (int)gLevelUpMsgs.size()) {
        drawShadowText(gLevelUpMsgs[gLevelUpIdx].c_str(),
                       SCREEN_W / 2 - MeasureText(gLevelUpMsgs[gLevelUpIdx].c_str(), 28) / 2,
                       530, 28, C_YELLOW);
    }
    if (gShownXp >= gXpGain && gShownGold >= gGoldGain) {
        DrawText("[ ENTER ]  Continue", SCREEN_W / 2 - MeasureText("[ ENTER ]  Continue", 22) / 2, 600, 22, C_GREY);
    }
}

void drawLevelUp() {
    drawBackground();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, { 0, 0, 0, 120 });
    drawShadowText("LEVEL UP!", SCREEN_W / 2 - MeasureText("LEVEL UP!", 64) / 2, 200, 64, C_GOLD);
    if (!gLevelUpMsgs.empty() && gLevelUpIdx < (int)gLevelUpMsgs.size()) {
        DrawText(gLevelUpMsgs[gLevelUpIdx].c_str(),
                 SCREEN_W / 2 - MeasureText(gLevelUpMsgs[gLevelUpIdx].c_str(), 26) / 2,
                 310, 26, C_WHITE);
    }
    drawHeroChar(SCREEN_W / 2, 450, C_YELLOW);
    spawnParticles({ (float)SCREEN_W / 2, 400.f }, 3, C_GOLD, 80.f, 0.8f);
    DrawText("[ ENTER ]  OK", SCREEN_W / 2 - MeasureText("[ ENTER ]  OK", 22) / 2, 540, 22, C_GREY);
}

// ---------------------------------------------------------------------------
// UPDATE FUNCTIONS
// ---------------------------------------------------------------------------
void updateMainMenu() {
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) {
        gSelectedClass = 0;
        gScreen = Screen::CLASS_SELECT;
    }
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) {
        Hero tmp{};
        if (loadGame(tmp)) {
            gHero = tmp;
            showNotif("Welcome back, " + gHero.name + "!");
            gScreen = Screen::HUB;
        } else {
            showNotif("No save file found. Starting new game.");
            gSelectedClass = 0;
            gScreen = Screen::CLASS_SELECT;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) CloseWindow();
}

void updateClassSelect() {
    if (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) gSelectedClass = 1;
    if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) gSelectedClass = 2;
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) gSelectedClass = 3;
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && gSelectedClass != 0) {
        gInputLen = 0;
        gInputBuf[0] = '\0';
        gScreen = Screen::NAME_INPUT;
    }
    if (IsKeyPressed(KEY_ESCAPE)) gScreen = Screen::MAIN_MENU;
}

void updateNameInput() {
    int key = GetCharPressed();
    while (key > 0) {
        if (isalpha(key) || key == ' ') {
            if (gInputLen < 20) {
                gInputBuf[gInputLen++] = (char)key;
                gInputBuf[gInputLen]   = '\0';
            }
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && gInputLen > 0) {
        gInputBuf[--gInputLen] = '\0';
    }
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && gInputLen > 0) {
        string cls = (gSelectedClass == 2 ? "Mage" : gSelectedClass == 3 ? "Archer" : "Warrior");
        gHero         = {};
        gHero.name    = string(gInputBuf);
        gHero.cls     = cls;
        gHero.hp      = gHero.maxHp = 50;
        gHero.atk     = 10;
        gHero.def     = 2;
        gHero.lvl     = 1;
        gHero.xpMax   = 15;
        gHero.gold    = 0;
        gHero.potions = 2;
        applyClassBonus(gHero);
        showNotif("Welcome, " + gHero.name + " the " + cls + "!");
        gScreen = Screen::HUB;
    }
    if (IsKeyPressed(KEY_ESCAPE)) gScreen = Screen::CLASS_SELECT;
}

void updateHub() {
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) {
        triggerRandomEvent(gHero);
        if (gEventPending) {
            gScreen = Screen::EXPLORE;
        } else {
            Enemy e = spawnEnemy(gHero.lvl, gHero.dungeon);
            startBattle(gHero, e);
        }
    }
    if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) gScreen = Screen::SHOP;
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) gScreen = Screen::STATS;
    if (IsKeyPressed(KEY_FOUR)  || IsKeyPressed(KEY_KP_4)) {
        Hero& h = gHero;
        if (!h.q1 && h.gKills >= 3) { h.q1 = true; h.gold += 80; h.potions += 2; showNotif("Quest 1 Complete! +80G +2 Potions"); }
        if (!h.q2 && h.lvl >= 5)    { h.q2 = true; h.atk  += 4;  h.def     += 2; showNotif("Quest 2 Complete! +4ATK +2DEF"); }
        gScreen = Screen::QUESTS;
    }
    if (IsKeyPressed(KEY_FIVE)  || IsKeyPressed(KEY_KP_5)) gScreen = Screen::DUNGEON_SEL;
    if (IsKeyPressed(KEY_SIX)   || IsKeyPressed(KEY_KP_6)) {
        saveGame(gHero);
        showNotif("Game saved successfully!");
    }
    if (IsKeyPressed(KEY_ZERO)  || IsKeyPressed(KEY_ESCAPE)) CloseWindow();
}

void updateExplore() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE)) {
        gEventPending = false;
        Enemy e = spawnEnemy(gHero.lvl, gHero.dungeon);
        startBattle(gHero, e);
    }
}

// ---------------------------------------------------------------------------
// BATTLE SYSTEM
// ---------------------------------------------------------------------------
void updateBattle(float dt) {
    Hero& h  = gHero;
    Enemy& e = gEnemy;

    if (gShakeTimer > 0) {
        gShakeTimer -= dt;
        if (gShakeTimer <= 0) gShakeX = 0.f;
    }

    if (gAnimTimer > 0) {
        gAnimTimer -= dt;
        return;
    }

    switch (gBPhase) {
    case BattlePhase::PLAYER_TURN: {
        break;
    }

    case BattlePhase::ANIM_PLAYER_ATK: {
        gHeroTargetX = 200; // Recoil hero back to start
        gBTurn++;

        // Poison tick on hero
        if (h.poisoned && h.pTurns > 0) {
            h.hp -= 3;
            h.pTurns--;
            spawnParticles({ (float)gHeroXPos, 430.f }, 10, C_PURPLE);
            if (!h.pTurns) h.poisoned = false;
            if (h.hp <= 0) {
                h.hp = 0;
                gBPhase = BattlePhase::HERO_DEAD;
                break;
            }
        }

        // Mimic escape check
        if (gMimicBattle && gBTurn >= 5) {
            gFlashMsg   = "MIMIC ESCAPED!";
            gFlashColor = C_GOLD;
            gAnimTimer  = 1.2f;
            gBPhase     = BattlePhase::RUN;
            break;
        }

        bool dodged = (e.name != "Golden Mimic") && rnd(1, 100) <= 8;
        if (dodged) {
            gFlashMsg   = e.name + " DODGED!";
            gFlashColor = C_CYAN;
            gAnimTimer  = 0.7f;
            spawnParticles({ (float)gEnemyXPos, 440.f }, 8, C_CYAN, 60.f, 0.5f);
            gBPhase     = BattlePhase::ANIM_ENEMY_ATK;
            break;
        }

        // Player damage calculation with exponential decay (fatigue)
        int base = max(2, h.atk + wAtk[h.weapon] - e.def + rnd(-1, 2));
        int cc   = wCrit[h.weapon] + (h.cls == "Archer" ? 30 : 0);
        bool crit= rnd(1, 100) <= cc;
        if (crit) base = (int)(base * 1.5f);
        if (h.cls == "Mage") base += h.lvl * 2;
        int dmg = decayDmg(base, gBTurn);

        bool blk = !crit && rnd(1, 100) <= min(35, e.def * 3);
        if (blk) dmg = max(1, dmg / 2);

        e.hp -= dmg;

        Color sparkCol = crit ? C_YELLOW : C_ORANGE;
        spawnParticles({ (float)gEnemyXPos, 430.f }, crit ? 20 : 10, sparkCol, crit ? 180.f : 100.f, 0.7f);
        gShakeX     = crit ? 6.f : 2.f;
        gShakeTimer = crit ? 0.4f : 0.2f;

        gFlashMsg   = (crit ? "CRITICAL! -" : "-") + intStr(dmg) + " HP";
        gFlashColor = crit ? C_YELLOW : C_RED;
        gAnimTimer  = crit ? 0.9f : 0.6f;

        // Boss phase transformations
        if (e.rank == "Boss" && e.hp > 0) {
            double pct = (double)e.hp / e.maxHp * 100.0;
            if (pct <= 60 && pct > 30 && gBossPhase == 1) {
                gBossPhase = 2;
                e.atk += 3;
                showNotif(e.name + " ENRAGES! ATK+3");
                spawnParticles({ (float)gEnemyXPos, 440.f }, 20, C_RED);
            } else if (pct <= 30 && gBossPhase == 2) {
                gBossPhase = 3;
                e.atk += 5;
                showNotif(e.name + " DESPERATE MODE! ATK+5");
                spawnParticles({ (float)gEnemyXPos, 440.f }, 30, C_RED);
            }
        }

        if (e.hp <= 0) {
            e.hp    = 0;
            gBPhase = BattlePhase::ENEMY_DEAD;
        } else {
            gBPhase = BattlePhase::ANIM_ENEMY_ATK;
        }
        break;
    }

    case BattlePhase::ANIM_ENEMY_ATK: {
        gEnemyTargetX = 900;

        if (e.atk <= 0) {
            gBPhase = BattlePhase::PLAYER_TURN;
            break;
        }

        bool heroDodged = rnd(1, 100) <= (8 + (h.cls == "Archer" ? 7 : 0));
        if (heroDodged) {
            gFlashMsg   = "YOU DODGED!";
            gFlashColor = C_CYAN;
            gAnimTimer  = 0.6f;
            spawnParticles({ (float)gHeroXPos, 430.f }, 8, C_CYAN, 60.f, 0.5f);
            gBPhase     = BattlePhase::PLAYER_TURN;
            break;
        }

        int ed = max(1, e.atk - h.def - aDef[h.armor] + rnd(-2, 3));
        ed = scaleEnemyDamage(ed);
        bool ult = (e.rank == "Boss" && gBossPhase == 3 && rnd(1, 3) == 1);
        if (ult) ed *= 2;

        gPendingEnemyDmg = ed;

        if (!ult) {
            gBPhase      = BattlePhase::REFLEX_PROMPT;
            gReflexTimer = gReflexWindow;
            gAnimTimer   = 0.3f;
        } else {
            h.hp -= ed;
            spawnParticles({ (float)gHeroXPos, 430.f }, 25, C_RED, 150.f, 0.8f);
            gShakeX     = 12.f;
            gShakeTimer = 0.8f;
            gFlashMsg   = "!!! ULTIMATE !!! -" + intStr(ed);
            gFlashColor = C_RED;
            gAnimTimer  = 1.2f;

            if (h.hp <= 0) {
                h.hp    = 0;
                gBPhase = BattlePhase::HERO_DEAD;
            } else {
                gBPhase = BattlePhase::PLAYER_TURN;
            }
        }
        break;
    }

    case BattlePhase::REFLEX_PROMPT: {
        gBPhase      = BattlePhase::REFLEX_WAITING;
        gReflexTimer = gReflexWindow;
        break;
    }

    case BattlePhase::REFLEX_WAITING: {
        gReflexTimer -= dt;

        bool pressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        bool timeout = gReflexTimer <= 0.0f;

        if (pressed || timeout) {
            double reduction = 0.0;
            if (pressed && !timeout) {
                double rt = (gReflexWindow - gReflexTimer);
                reduction = max(0.0, min(0.95, 1.0 - (rt / gReflexWindow)));
                long long rtMs = (long long)(rt * 1000);
                h.reactSumMs += rtMs;
                h.reactCount++;

                string tag = reduction > 0.8 ? "PERFECT BLOCK!" : reduction > 0.5 ? "GREAT BLOCK!" : reduction > 0.2 ? "BLOCKED!" : "GRAZED...";
                Color tagCol = reduction > 0.8 ? C_YELLOW : reduction > 0.5 ? C_GREEN : reduction > 0.2 ? C_CYAN : C_GREY;
                gFlashMsg   = tag + " (" + intStr((int)rtMs) + "ms)";
                gFlashColor = tagCol;
                spawnParticles({ (float)gHeroXPos, 420.f }, (int)(reduction * 20 + 5), C_CYAN, 100.f, 0.6f);
            } else {
                gFlashMsg   = "TOO SLOW!";
                gFlashColor = C_GREY;
            }

            int ed = max(1, (int)(gPendingEnemyDmg * (1.0 - reduction)));
            if (e.canPoison && !h.poisoned && rnd(1, 4) == 1) {
                h.poisoned = true;
                h.pTurns   = 3;
                showNotif("*** POISONED! ***");
                spawnParticles({ (float)gHeroXPos, 440.f }, 10, C_PURPLE);
            }

            h.hp       -= ed;
            gShakeX     = 4.f;
            gShakeTimer = 0.3f;
            gAnimTimer  = 0.8f;

            if (h.hp <= 0) {
                h.hp    = 0;
                gBPhase = BattlePhase::HERO_DEAD;
            } else {
                gBPhase = BattlePhase::PLAYER_TURN;
            }
        }
        break;
    }

    case BattlePhase::ENEMY_DEAD: {
        h.xp   += e.xpR;
        h.gold += e.goldR;
        if (e.name == "Goblin") h.gKills++;
        if (e.name == "Dragon" && !h.q3) {
            h.q3    = true;
            h.maxHp += 15;
            h.hp    = h.maxHp;
            showNotif("Quest 3 Complete! +15 MaxHP!");
        }
        gShownXp   = 0;
        gShownGold = 0;
        lvlUp(h);
        gLevelUpIdx = 0;
        gScreen     = Screen::VICTORY;
        break;
    }

    case BattlePhase::HERO_DEAD: {
        gScreen = Screen::GAME_OVER;
        break;
    }

    case BattlePhase::RUN: {
        gScreen = Screen::HUB;
        break;
    }
    }

    // Player action inputs
    if (gBPhase == BattlePhase::PLAYER_TURN && gAnimTimer <= 0.0f) {
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) {
            gHeroTargetX = gEnemyXPos - 80;
            gBPhase      = BattlePhase::ANIM_PLAYER_ATK;
            gAnimTimer   = 0.35f;
        }
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) {
            if (h.potions > 0) {
                if (h.poisoned) { h.poisoned = false; h.pTurns = 0; }
                int hl = rnd(20, 35);
                h.hp   = min(h.maxHp, h.hp + hl);
                h.potions--;
                spawnParticles({ (float)gHeroXPos, 420.f }, 15, C_GREEN, 80.f, 0.6f);
                gFlashMsg   = "+" + intStr(hl) + " HP Healed!";
                gFlashColor = C_GREEN;
                gAnimTimer  = 0.6f;
                gBPhase     = BattlePhase::ANIM_ENEMY_ATK;
            } else {
                showNotif("No potions left!");
            }
        }
        if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) {
            if (rnd(1, 3) == 1) {
                int ed = scaleEnemyDamage(max(1, e.atk - h.def + rnd(-2, 3)));
                h.hp -= ed;
                gFlashMsg   = "Escape failed! -" + intStr(ed) + " HP";
                gFlashColor = C_RED;
                gAnimTimer  = 0.8f;
                if (h.hp <= 0) {
                    h.hp    = 0;
                    gBPhase = BattlePhase::HERO_DEAD;
                } else {
                    gBPhase = BattlePhase::ANIM_ENEMY_ATK;
                }
            } else {
                gBPhase    = BattlePhase::RUN;
                gAnimTimer = 0.4f;
            }
        }
    }
}

void updateShop() {
    Hero& h = gHero;
    auto tryBuyWeapon = [&](int i) {
        if (h.gold >= wCost[i]) {
            h.gold   -= wCost[i];
            h.weapon  = i;
            showNotif(string(wName[i]) + " equipped!");
        } else {
            showNotif("Not enough gold!");
        }
    };
    auto tryBuyArmor = [&](int i) {
        if (h.gold >= aCost[i]) {
            h.gold  -= aCost[i];
            h.armor  = i;
            showNotif(string(aName[i]) + " equipped!");
        } else {
            showNotif("Not enough gold!");
        }
    };

    if (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) tryBuyWeapon(1);
    if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) tryBuyWeapon(2);
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) tryBuyWeapon(3);
    if (IsKeyPressed(KEY_FOUR)  || IsKeyPressed(KEY_KP_4)) tryBuyArmor(1);
    if (IsKeyPressed(KEY_FIVE)  || IsKeyPressed(KEY_KP_5)) tryBuyArmor(2);
    if (IsKeyPressed(KEY_SIX)   || IsKeyPressed(KEY_KP_6)) tryBuyArmor(3);
    if (IsKeyPressed(KEY_SEVEN) || IsKeyPressed(KEY_KP_7)) {
        if (h.gold >= 15) {
            h.gold -= 15;
            h.potions++;
            showNotif("Potion purchased!");
        } else {
            showNotif("Not enough gold!");
        }
    }
    if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_ESCAPE)) gScreen = Screen::HUB;
}

void updateStats() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        gScreen = Screen::HUB;
    }
}

void updateQuests() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        gScreen = Screen::HUB;
    }
}

void updateDungeonSelect() {
    auto tryDungeon = [&](int i) {
        if (gHero.lvl >= dMinLv[i]) {
            gHero.dungeon = i;
            showNotif("Entering " + string(dName[i]) + "!");
            gScreen = Screen::HUB;
        } else {
            showNotif(string("Need Level ") + intStr(dMinLv[i]) + " to enter!");
        }
    };
    if (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) tryDungeon(0);
    if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) tryDungeon(1);
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) tryDungeon(2);
    if (IsKeyPressed(KEY_ESCAPE)) gScreen = Screen::HUB;
}

void updateGameOver() {
    Hero& h = gHero;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        h.hp       = max(1, h.maxHp / 2);
        h.poisoned = false;
        h.pTurns   = 0;
        gScreen    = Screen::HUB;
    }
}

void updateVictory(float dt) {
    if (gShownXp < gXpGain)     gShownXp   = min(gXpGain,   gShownXp   + max(1, gXpGain / 60));
    if (gShownGold < gGoldGain) gShownGold = min(gGoldGain, gShownGold + max(1, gGoldGain / 60));

    spawnParticles({ (float)rnd(200, SCREEN_W - 200), (float)rnd(100, 400) }, 1, C_GOLD, 50.f, 0.5f);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (gShownXp >= gXpGain && gShownGold >= gGoldGain) {
            if (!gLevelUpMsgs.empty() && gLevelUpIdx < (int)gLevelUpMsgs.size()) {
                gScreen = Screen::LEVEL_UP;
            } else {
                gLevelUpMsgs.clear();
                gScreen = Screen::HUB;
            }
        } else {
            gShownXp   = gXpGain;
            gShownGold = gGoldGain;
        }
    }
}

void updateLevelUp() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        gLevelUpIdx++;
        if (gLevelUpIdx >= (int)gLevelUpMsgs.size()) {
            gLevelUpMsgs.clear();
            gLevelUpIdx = 0;
            gScreen     = Screen::HUB;
        }
    }
}

// ---------------------------------------------------------------------------
// MAIN APPLICATION ENTRY POINT
// ---------------------------------------------------------------------------
int main() {
    srand((unsigned)time(0));

    InitWindow(SCREEN_W, SCREEN_H, "Mini RPG: Legends Edition");
    SetExitKey(KEY_NULL);

    auto lastFrameTime = chrono::high_resolution_clock::now();
    int currentFps = 60;
    int frameCount = 0;
    float fpsTimer = 0.0f;

    while (!WindowShouldClose()) {
        auto now = chrono::high_resolution_clock::now();
        float dt = chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        if (dt > 0.1f)  dt = 0.016f;
        if (dt <= 0.0f) dt = 0.016f;

        frameCount++;
        fpsTimer += dt;
        if (fpsTimer >= 0.5f) {
            currentFps = (int)(frameCount / fpsTimer);
            frameCount = 0;
            fpsTimer   = 0.0f;
        }

        // Updates
        if (gNotifTimer > 0) gNotifTimer -= dt;
        updateParticles(dt);

        if (gScreen == Screen::BATTLE) {
            gHeroXPos  = (int)myLerp((float)gHeroXPos,  (float)gHeroTargetX,  dt * 10.f);
            gEnemyXPos = (int)myLerp((float)gEnemyXPos, (float)gEnemyTargetX, dt * 10.f);
            updateBattle(dt);
        } else {
            gHeroXPos  = gHeroTargetX  = 200;
            gEnemyXPos = gEnemyTargetX = 900;
        }

        switch (gScreen) {
        case Screen::MAIN_MENU:    updateMainMenu();      break;
        case Screen::CLASS_SELECT: updateClassSelect();   break;
        case Screen::NAME_INPUT:   updateNameInput();     break;
        case Screen::HUB:          updateHub();           break;
        case Screen::EXPLORE:      updateExplore();       break;
        case Screen::SHOP:         updateShop();          break;
        case Screen::STATS:        updateStats();         break;
        case Screen::QUESTS:       updateQuests();        break;
        case Screen::DUNGEON_SEL:  updateDungeonSelect(); break;
        case Screen::GAME_OVER:    updateGameOver();      break;
        case Screen::VICTORY:      updateVictory(dt);     break;
        case Screen::LEVEL_UP:     updateLevelUp();       break;
        }

        // Renders
        BeginDrawing();
        ClearBackground(C_BG);

        switch (gScreen) {
        case Screen::MAIN_MENU:    drawMainMenu();      break;
        case Screen::CLASS_SELECT: drawClassSelect();   break;
        case Screen::NAME_INPUT:   drawNameInput();     break;
        case Screen::HUB:          drawHub();           break;
        case Screen::EXPLORE:      drawExplore();       break;
        case Screen::BATTLE:       drawBattle();        break;
        case Screen::SHOP:         drawShop();          break;
        case Screen::STATS:        drawStats();         break;
        case Screen::QUESTS:       drawQuests();        break;
        case Screen::DUNGEON_SEL:  drawDungeonSelect(); break;
        case Screen::GAME_OVER:    drawGameOver();      break;
        case Screen::VICTORY:      drawVictory();       break;
        case Screen::LEVEL_UP:     drawLevelUp();       break;
        }

        drawParticles();
        drawNotif();

        DrawText(TextFormat("FPS:%d", currentFps), SCREEN_W - 80, 4, 14, C_GREY);

        EndDrawing();
        SwapScreenBuffer();
        PollInputEvents();

        // 60 FPS frame limiter
        auto frameEndTime = chrono::high_resolution_clock::now();
        float frameElapsed = chrono::duration<float>(frameEndTime - now).count();
        if (frameElapsed < 0.0166f) {
            int sleepMs = (int)((0.0166f - frameElapsed) * 1000.0f);
            if (sleepMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }

    CloseWindow();
    return 0;
}
