/*
 * DEEP SPACE OUTPOST — C 语言终端版 v3「6 种 BOSS · 阶段精英 · 强化」
 * ---------------------------------------------------------------
 * 相对 v2 的改动（与网页版同步）：
 *   - 第 5/10/15/20/25/30 关各是一种独立 BOSS：
 *     晶簇核心(弹幕) / 棱光之眼(激光) / 熔核巨像(陨石) /
 *     裂空之刃(突进) / 虫巢母体(召唤) / 虚空主宰(瞬移)
 *   - 最终 BOSS「终焉·母舰」按血量阶段解锁前面所有技能
 *   - 每个阶段的 4 个普通关各出现 1 只阶段专属精英（同阶段 BOSS 的缩小版）
 *   - 每关通关后弹出三选一强化（11 种，可叠加，按 1/2/3 选择）
 *
 * 相对 v1 的改动：
 *   - 关卡固定 30 关；第 5/10/15/20/25/30 关为 BOSS 关
 *   - 30 关通关后出现「最终 BOSS · 终焉·母舰」，HP 168000
 *   - 小怪爆金币大幅减少，波次奖励 / 装备价格同步重新平衡
 *   - 移除全部自动辅助：自瞄、弱点优先、穿透弹、无敌、护盾、
 *     自动回血、射速超频，以及 H/K 之类调试作弊键
 *   - 瞄准改为完全手动：J / L 按住旋转（无鼠标环境）
 *   - 装备从 10 款扩充到 16 款，且每一款都真实参与战斗计算
 *   - 新增机库：4 槽 × 每槽 4 款部件 = 256 种基础图纸，
 *     可组装、可随机搭建、可两机融合，能搭出许多款飞机
 *
 * 飞机说明：全部机型 / 部件均为虚构代号 + 风格化外形，
 *   US 风格 = 菱形飞翼与隐身布局气质，CN 风格 = 鸭翼 + 三角翼气质，
 *   MIX = 融合布局。不对应任何真实型号，不引用真实性能参数。
 *
 * 零外部依赖，跨平台 C11（Linux/macOS 走 termios，Windows 走控制台 API）：
 *   Linux / macOS:  gcc -std=c11 -O2 -Wall -Wextra -o game game.c -lm
 *   Windows MSVC:   cl /utf-8 /std:c11 /O2 game.c
 *   Windows MinGW:  gcc -std=c11 -O2 -Wall -Wextra -o game.exe game.c
 *
 * 操作：
 *   W/A/S/D  移动        空格   切换持续开火（再按一次停止）
 *   J / L    手动旋转瞄准        C     冲刺闪避
 *   Tab      切换面板（商店 / 装备 / 机库 / 图纸）
 *   ↑/↓     选择条目      ←/→   图纸 tab 内换部件（其他 tab 切面板）
 *   Enter    购买 / 设为出战 / 组装新机
 *   B        组装新机     G     随机搭建     F     融合两机
 *   1~9      快速切换出战机体
 *   R        重新开始     X     退出（Ctrl+C 同样会正常退出）
 *
 * 关卡规则：只能从第 1 关开始逐关推进，打完本关全部敌人才能进入下一关。
 *   游戏内没有任何选关 / 跳关键，命令行也不提供跳关参数；
 *   R 重新开始同样回到第 1 关。
 *
 * 自测： ./game --selftest 30000   （无头逻辑帧，测试驱动，不是游戏内辅助）
 *       ./game --snapshot          （第 1 关跑若干帧后输出一帧纯文本画面）
 *       ./game --jetlab           （枚举 256 种飞机图纸并打印样例外形）
 *       ./game --frames 120       （自动化：交互模式跑 120 帧后自动退出）
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#  ifndef ENABLE_EXTENDED_FLAGS
#    define ENABLE_EXTENDED_FLAGS 0x0080
#  endif
#  ifndef ENABLE_QUICK_EDIT_MODE
#    define ENABLE_QUICK_EDIT_MODE 0x0040
#  endif
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/select.h>
#  include <sys/ioctl.h>
#  include <fcntl.h>
#endif

#define PI 3.14159265358979323846
#define TAU (PI * 2.0)

/* ===== 世界与容量 ===== */
#define WORLD_W 960.0
#define WORLD_H 600.0
#define MAX_ENEMIES  160
#define MAX_BULLETS  320
#define MAX_EBULLETS 420
#define MAX_COINS    320
#define MAX_PICKUPS  32
#define MAX_FLOATERS 48
#define MAX_PARTICLES 260

#define PANEL_W 34
#define MIN_COLS 40
#define MAX_COLS 78
#define MIN_ROWS 18
#define MAX_ROWS 40

/* ===== 关卡 / BOSS ===== */
#define MAX_WAVE 30
#define BOSS_EVERY 5
#define FINAL_BOSS_HP 168000.0
#define BOSS_HP(w) (800.0 + (double)(w) * 260.0)

/* ===== 机库 ===== */
#define PART_SLOTS 4
#define PART_PER_SLOT 4
#define JET_COMBOS 256                     /* 4 槽 × 每槽 4 款 = 4^4 */
#define MAX_JETS 32
#define JET_BUILD_COST 120
#define JET_RANDOM_COST 60
#define JET_FUSE_COST 180

/* ===== 小工具 ===== */
static double randf(double a, double b) { return a + (b - a) * ((double)rand() / RAND_MAX); }
static double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
static int    clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static double dist2(double ax, double ay, double bx, double by) {
    double dx = ax - bx, dy = ay - by; return dx * dx + dy * dy;
}

/* ===== 平台层 ===== */
#ifdef _WIN32
static HANDLE g_hIn = INVALID_HANDLE_VALUE, g_hOut = INVALID_HANDLE_VALUE;
static double g_qpcFreq = 0.0;
static DWORD g_origOutMode = 0, g_origInMode = 0, g_origOutCP = 0;
static int  g_consoleOut = 0, g_consoleIn = 0, g_vtOn = 0, g_consoleDone = 0;

static double now_ms(void) {
    LARGE_INTEGER c;
    if (g_qpcFreq <= 0.0) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_qpcFreq = (double)f.QuadPart;
        if (g_qpcFreq <= 0.0) g_qpcFreq = 1.0;
    }
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / g_qpcFreq;
}
static void sleep_ms(double ms) { if (ms > 0.0) Sleep((DWORD)(ms + 0.5)); }

/* 打开虚拟终端序列 + UTF-8 代码页：否则 ANSI 颜色会原样打印、中文也会乱码 */
static void consoleFini(void);
static void consoleInit(void) {
    g_hIn  = GetStdHandle(STD_INPUT_HANDLE);
    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    g_origOutCP = GetConsoleOutputCP();
    if (g_origOutCP) SetConsoleOutputCP(CP_UTF8);
    if (GetConsoleMode(g_hOut, &g_origOutMode)) {
        g_consoleOut = 1;
        if (SetConsoleMode(g_hOut, g_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) g_vtOn = 1;
        else SetConsoleMode(g_hOut, g_origOutMode);
    }
    g_consoleIn = (g_hIn != INVALID_HANDLE_VALUE) && _isatty(_fileno(stdin));
    atexit(consoleFini);                      /* --help 等提前退出的路径也还原代码页 */
}
static void consoleFini(void) {
    if (g_consoleDone) return;
    g_consoleDone = 1;
    if (g_vtOn) {
        printf("\033[?25h\033[0m");
        fflush(stdout);
        SetConsoleMode(g_hOut, g_origOutMode);
    }
    if (g_origOutCP) SetConsoleOutputCP(g_origOutCP);
}
static int termSize(int *cols, int *rows) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!g_consoleOut || !GetConsoleScreenBufferInfo(g_hOut, &info)) return 0;
    *cols = (int)(info.srWindow.Right - info.srWindow.Left + 1);
    *rows = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
    return (*cols > 0 && *rows > 0) ? 1 : 0;
}
static int ansiOK(void) { return g_vtOn; }   /* 不支持 VT 的老控制台退回纯文本，避免满屏转义符 */

/* Windows 的 argv 是 ANSI(GBK) 编码，而控制台已切成 UTF-8，直接用会乱码 */
static const char *argText(const char *ansi) {
    static char buf[1024];
    wchar_t w[512];
    int n = MultiByteToWideChar(CP_ACP, 0, ansi, -1, w, (int)(sizeof(w) / sizeof(w[0])));
    if (n <= 0) return ansi;
    int m = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, (int)sizeof(buf), NULL, NULL);
    return (m > 0) ? buf : ansi;
}
#else
static double now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
static void sleep_ms(double ms) { if (ms > 0.0) usleep((useconds_t)(ms * 1000.0 + 0.5)); }
static void consoleInit(void) { }
static void consoleFini(void) { }
static int termSize(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0 || ws.ws_row == 0) return 0;
    *cols = (int)ws.ws_col; *rows = (int)ws.ws_row;
    return 1;
}
static int ansiOK(void) { return 1; }
static const char *argText(const char *s) { return s; }
#endif

/* ===== 颜色 ===== */
enum { C_DIM = 0, C_CYAN, C_RED, C_GREEN, C_GOLD, C_PURPLE, C_WHITE, C_AMBER, C_GRID, C_COUNT };
static const char *ANSI[C_COUNT] = {
    "\033[0;90m", "\033[1;96m", "\033[1;91m", "\033[1;92m", "\033[1;93m",
    "\033[1;95m", "\033[1;97m", "\033[0;33m", "\033[0;90m"
};

/* ===== 商店装备定义（16 款，全部真实生效） ===== */
enum {
    S_DMG = 0, S_RATE, S_MULTI, S_CRIT, S_HP, S_ARMOR, S_SPEED, S_PICKUP,
    S_GREED, S_XP, S_BOSS, S_LINK, S_LEECH, S_LUCK, S_DASH, S_RANGE, SHOP_N
};
typedef struct {
    const char *name, *desc;
    char icon;
    int cat, maxLv, col;
    double base, growth;
} ShopDef;
static const ShopDef SHOP_DEFS[SHOP_N] = {
    {"动能插件",   "子弹伤害",     '+', 0, 14, C_RED,    10.0, 1.42},
    {"冷却核心",   "射击间隔",     '~', 0, 10, C_CYAN,   16.0, 1.45},
    {"多管阵列",   "单次开火弹数", '#', 0,  4, C_PURPLE, 85.0, 1.85},
    {"裂解瞄准镜", "暴击率",       '*', 0,  8, C_AMBER,  30.0, 1.50},
    {"生体强化",   "最大生命",     'H', 1, 12, C_GREEN,  14.0, 1.42},
    {"复合装甲",   "受到伤害减免", 'A', 1, 10, C_CYAN,   24.0, 1.46},
    {"推进器",     "移动速度",     'S', 1,  8, C_GREEN,  20.0, 1.46},
    {"磁力线圈",   "金币吸取半径", 'O', 2,  6, C_GREEN,  22.0, 1.50},
    {"贪婪增幅",   "金币获取",     '$', 2, 10, C_GOLD,   26.0, 1.50},
    {"神经加速",   "经验获取",     'X', 2,  8, C_PURPLE, 20.0, 1.48},
    {"猎杀协议",   "对 BOSS 伤害", 'B', 0, 10, C_PURPLE, 34.0, 1.50},
    {"串联弹芯",   "子弹贯穿次数", '/', 0,  3, C_CYAN,   90.0, 1.90},
    {"生体虹吸",   "击杀回复生命", 'V', 1,  8, C_GREEN,  28.0, 1.50},
    {"回收协议",   "补给掉落概率", '?', 2,  8, C_GOLD,   22.0, 1.48},
    {"矢量推进",   "冲刺冷却",     'C', 1,  6, C_CYAN,   26.0, 1.50},
    {"长管腔",     "弹速与射程",   'L', 0,  8, C_AMBER,  24.0, 1.48},
};

/* ===== 海克斯强化（每关通关三选一，可叠加） ===== */
enum { H_DMG = 0, H_RATE, H_HP, H_CRIT, H_LINK, H_BOSS, H_GOLD, H_SPD,
       H_MULTI, H_ARMOR, H_LEECH, HEX_N };
typedef struct { const char *name, *desc; int rar, max; } HexDef;
static const HexDef HEXES[HEX_N] = {
    {"动能增强", "+15% 子弹伤害",     1, 6},
    {"冷却回路", "-8% 射击间隔",      1, 6},
    {"附加装甲", "+25 最大生命",      1, 6},
    {"裂解瞄准", "+12% 暴击率",       2, 3},
    {"穿甲弹芯", "子弹贯穿 +1",       2, 2},
    {"猎杀协议", "对 BOSS 伤害 +25%", 2, 3},
    {"拾荒者",   "+15% 金币获取",     1, 4},
    {"推进优化", "+8% 移动速度",      1, 4},
    {"多管齐射", "每次开火 +1 发",    3, 2},
    {"相位护盾", "-15% 受到伤害",     2, 3},
    {"吸血涂层", "击杀回复 3 点生命", 2, 3},
};
static const char *RAR_NAME[4] = {"", "白银", "黄金", "棱彩"};

/* ===== 敌人类型（金币产出已削减） ===== */
enum { E_GRUNT = 0, E_FAST, E_TANK, E_SPLITTER, E_ELITE, ETYPE_N };
typedef struct {
    const char *name;
    double hp, speed, r, gold, xp, dmg;
    char ch; int col;
} ETypeDef;
static const ETypeDef ETYPE[ETYPE_N] = {
    {"一级体", 28.0,  62.0, 13.0, 1.0,  8.0,  8.0,  'o', C_RED},
    {"疾行者", 20.0, 120.0, 10.0, 1.0, 11.0,  7.0,  '*', C_AMBER},
    {"重装体", 90.0,  38.0, 21.0, 2.0, 22.0, 16.0,  'O', C_PURPLE},
    {"分裂体", 48.0,  70.0, 16.0, 2.0, 16.0, 10.0,  'S', C_CYAN},
    {"精英",  220.0,  52.0, 26.0, 4.0, 60.0, 26.0,  'E', C_GOLD},
};

/* ===== 虚构飞机部件 =====
   style: 0 = US 风格（菱形飞翼/隐身布局气质）
          1 = CN 风格（鸭翼 + 三角翼气质）
          2 = MIX（融合布局）
   全部为虚构代号，不对应真实型号与真实性能参数。 */
typedef struct {
    const char *code, *name;
    int style;
    double dmg, hp, spd, rate;   /* 伤害 / 生命 / 移速 / 射速加成 */
} Part;
static const Part PARTS[PART_SLOTS][PART_PER_SLOT] = {
    { /* 0 机身 */
        {"F1", "菱形飞翼",   0, 0.10, 0.06, 0.02, 0.00},
        {"F2", "鸭式升力体", 1, 0.06, 0.12, 0.00, 0.02},
        {"F3", "融合升力体", 2, 0.08, 0.08, 0.04, 0.00},
        {"F4", "模块化骨架", 2, 0.04, 0.18, 0.00, 0.00},
    },
    { /* 1 机翼 */
        {"W1", "后掠三角翼", 0, 0.06, 0.02, 0.10, 0.00},
        {"W2", "鸭翼三角翼", 1, 0.08, 0.02, 0.08, 0.00},
        {"W3", "变后掠翼",   2, 0.05, 0.06, 0.06, 0.00},
        {"W4", "联翼布局",   0, 0.04, 0.10, 0.04, 0.00},
    },
    { /* 2 引擎 */
        {"N1", "双矢量喷口", 0, 0.02, 0.00, 0.14, 0.04},
        {"N2", "三元矢量",   1, 0.06, 0.00, 0.12, 0.00},
        {"N3", "组合循环",   2, 0.04, 0.04, 0.10, 0.02},
        {"N4", "分布式推进", 0, 0.00, 0.08, 0.16, 0.00},
    },
    { /* 3 武器 */
        {"G1", "隐身弹舱",   0, 0.16, 0.00, 0.00, 0.00},
        {"G2", "机炮阵列",   1, 0.12, 0.00, 0.00, 0.06},
        {"G3", "定向吊舱",   2, 0.14, 0.00, 0.00, 0.04},
        {"G4", "多联挂架",   1, 0.10, 0.00,-0.02, 0.10},
    },
};
static const char *SLOT_NAME[PART_SLOTS] = {"机身", "机翼", "引擎", "武器"};
static const char *STYLE_NAME[3] = {"US", "CN", "MIX"};
static const char *STYLE_FULL[3] = {"飞翼隐身", "鸭翼三角", "融合布局"};

typedef struct {
    char code[12], label[20];
    int style;
    int p[PART_SLOTS];
    int used;
} Jet;
static Jet jets[MAX_JETS];
static int jetCount;

/* ===== 实体 ===== */
typedef struct {
    double x, y, r, hp, maxHp;
    double aimAngle, lastMoveAngle, fireCd;
    double dashCd, dashTime, dashDx, dashDy, invuln;
} Player;

typedef struct {
    double x, y, hp, maxHp, speed, r, dmg, phase, spawnAnim, hitFlash;
    double xp, fireCd, summonCd, orbit;
    int type, level, gold, used;
    int counts;              /* 是否计入本关 waveLeft */
    int isBoss, isFinal, stage;
    int bkind;               /* BOSS 种类 0..6 */
    int sei;                 /* 阶段精英类型 0..5，-1 = 普通小怪 */
    double cd, cd2, cd3, cd4, cd5, spin, chargeT, dashT, dashVx, dashVy;
    char bossName[24];
} Enemy;

typedef struct {
    double x, y, vx, vy, r, life, dmg;
    int crit, hitsLeft, used;
    unsigned int hitMask;
} Bullet;

typedef struct { double x, y, vx, vy, r, life, dmg, hom; int used; } EBullet;
typedef struct { double x, y, vx, vy, life, phase; int value, used; } Coin;
typedef struct { double x, y, life, phase; int used; } Pickup;
typedef struct { double x, y, vy, life; int col, used; char text[16]; } Floater;
typedef struct { double x, y, vx, vy, life, maxLife; int col, used; } Particle;

/* 激光束（蓄力 → 扫射） */
typedef struct {
    double x, y, ang, rot, charge, active, dur, w, dmg;
    int used;
} Beam;
/* 陨石（落点预警 → 爆炸） */
typedef struct {
    double tx, ty, t, dur, dmg, r;
    int used;
} Meteor;
#define MAX_BEAMS 24
#define MAX_METEORS 40

/* ===== 派生属性 ===== */
typedef struct {
    double damage, interval, crit, maxHp, armor, speedMul, pickupR, goldMul, xpMul;
    double bossMul, leech, luck, dashMul, rangeMul;
    int bulletCount, link;
} Stats;

/* ===== 全局状态 ===== */
typedef struct {
    int running, over, victory, quit;
    double t;
    int wave, score, kills, shots, hits, bossKills;
    int gold, level;
    double xp;
    int waveTotal, waveLeft;
    double spawnTimer;
    int between, finalPhase, bossAlive;
    double waveGap;

    int bought[SHOP_N];

    int tab, sel;             /* 0 商店 / 1 装备 / 2 机库 / 3 图纸 */
    int selftest, plain, snapshot;
    int firing, autoFire;
    int cols, rows;

    int activeJet;
    int blueprint[PART_SLOTS];
    unsigned char comboSeen[32];   /* 256 种基础图纸发现位图 */

    int hex[HEX_N];          /* 海克斯强化层数 */
    int pick3;               /* 通关后正在三选一 */
    int pickOpt[3];

    char banner[48];
    double bannerLife;
    int bannerCol;
} Game;

static Game  G;
static Player P;
static Enemy    enemies[MAX_ENEMIES];     static int enemyCount;
static Bullet   bullets[MAX_BULLETS];     static int bulletCount;
static EBullet  ebullets[MAX_EBULLETS];   static int ebulletCount;
static Coin     coins[MAX_COINS];         static int coinCount;
static Pickup   pickups[MAX_PICKUPS];     static int pickupCount;
static Floater  floaters[MAX_FLOATERS];   static int floaterCount;
static Particle particles[MAX_PARTICLES]; static int particleCount;
static Beam     beams[MAX_BEAMS];
static Meteor   meteors[MAX_METEORS];

static Player *pl = &P;

/* 前置声明 */
static int keyDown(char c);
static void gameOver(void);
static void startWave(void);
static void spawnEnemy(int forceType, int counts);
static void spawnBoss(int isFinal);
static void fire(void);
static void damagePlayer(double amount);
static void heal(double amount);
static void gainXP(double amount);
static void dropCoins(double x, double y, int amount);
static void buy(int id);
static Stats stats(void);
static int shopPrice(int id);
static void showBanner(const char *text, int col);

/* ===== 数组增删（swap-remove，保持紧凑） ===== */
static int addEnemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) if (!enemies[i].used) {
        memset(&enemies[i], 0, sizeof(Enemy)); enemies[i].used = 1;
        if (i >= enemyCount) enemyCount = i + 1;
        return i;
    }
    return -1;
}
static void removeEnemy(int i) { enemies[i].used = 0; }
static int addBullet(void) {
    for (int i = 0; i < MAX_BULLETS; i++) if (!bullets[i].used) {
        memset(&bullets[i], 0, sizeof(Bullet)); bullets[i].used = 1;
        if (i >= bulletCount) bulletCount = i + 1;
        return i;
    }
    return -1;
}
static int addEBullet(double x, double y, double vx, double vy, double dmg) {
    for (int i = 0; i < MAX_EBULLETS; i++) if (!ebullets[i].used) {
        EBullet *b = &ebullets[i];
        memset(b, 0, sizeof(EBullet));
        b->x = x; b->y = y; b->vx = vx; b->vy = vy;
        b->r = 4.0; b->life = 7.0; b->dmg = dmg; b->used = 1;
        if (i >= ebulletCount) ebulletCount = i + 1;
        return i;
    }
    return -1;
}
static int addCoin(void) {
    for (int i = 0; i < MAX_COINS; i++) if (!coins[i].used) {
        memset(&coins[i], 0, sizeof(Coin)); coins[i].used = 1;
        if (i >= coinCount) coinCount = i + 1;
        return i;
    }
    return -1;
}
static int addPickup(void) {
    for (int i = 0; i < MAX_PICKUPS; i++) if (!pickups[i].used) {
        memset(&pickups[i], 0, sizeof(Pickup)); pickups[i].used = 1;
        if (i >= pickupCount) pickupCount = i + 1;
        return i;
    }
    return -1;
}
static void addFloater(double x, double y, const char *text, int col) {
    for (int i = 0; i < MAX_FLOATERS; i++) if (!floaters[i].used) {
        Floater *f = &floaters[i];
        memset(f, 0, sizeof(Floater));
        f->x = x; f->y = y; f->vy = -58; f->life = 0.95; f->col = col; f->used = 1;
        snprintf(f->text, sizeof(f->text), "%s", text);
        if (i >= floaterCount) floaterCount = i + 1;
        return;
    }
}
static void addParticles(double x, double y, int col, int n, double spd, double life) {
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < MAX_PARTICLES; i++) if (!particles[i].used) {
            Particle *p = &particles[i];
            double a = randf(0, TAU), s = randf(spd * 0.3, spd);
            memset(p, 0, sizeof(Particle));
            p->x = x; p->y = y; p->vx = cos(a)*s; p->vy = sin(a)*s;
            p->life = p->maxLife = life; p->col = col; p->used = 1;
            if (i >= particleCount) particleCount = i + 1;
            break;
        }
    }
}

/* ===== 机库：部件 / 组合 / 融合 ===== */
static void jetBonus(int idx, double *dmg, double *hp, double *spd, double *rate) {
    *dmg = *hp = *spd = *rate = 0.0;
    if (idx < 0 || idx >= MAX_JETS || !jets[idx].used) return;
    for (int k = 0; k < PART_SLOTS; k++) {
        int v = clampi(jets[idx].p[k], 0, PART_PER_SLOT - 1);
        const Part *q = &PARTS[k][v];
        *dmg += q->dmg; *hp += q->hp; *spd += q->spd; *rate += q->rate;
    }
}
static int comboIndex(const int p[PART_SLOTS]) {
    return clampi(p[0], 0, 3) * 64 + clampi(p[1], 0, 3) * 16
         + clampi(p[2], 0, 3) * 4  + clampi(p[3], 0, 3);
}
static void markCombo(const int p[PART_SLOTS]) {
    int v = comboIndex(p);
    G.comboSeen[v >> 3] |= (unsigned char)(1u << (v & 7));
}
static int comboCount(void) {
    int n = 0;
    for (int i = 0; i < JET_COMBOS; i++) {
        if (G.comboSeen[i >> 3] & (1u << (i & 7))) n++;
    }
    return n;
}
static int addJet(void) {
    for (int i = 0; i < MAX_JETS; i++) if (!jets[i].used) {
        memset(&jets[i], 0, sizeof(Jet));
        jets[i].used = 1;
        if (i >= jetCount) jetCount = i + 1;
        return i;
    }
    return -1;
}
static void jetSet(int idx, const char *code, const char *label, int style,
                   int a, int b, int c, int d) {
    Jet *j = &jets[idx];
    snprintf(j->code, sizeof(j->code), "%s", code);
    snprintf(j->label, sizeof(j->label), "%s", label);
    j->style = clampi(style, 0, 2);
    j->p[0] = a; j->p[1] = b; j->p[2] = c; j->p[3] = d;
    markCombo(j->p);
}
static int styleOfParts(const int p[PART_SLOTS]) {
    int us = 0, cn = 0;
    for (int k = 0; k < PART_SLOTS; k++) {
        int s = PARTS[k][clampi(p[k], 0, 3)].style;
        if (s == 0) us++; else if (s == 1) cn++;
    }
    if (us > cn) return 0;
    if (cn > us) return 1;
    return 2;
}
static void initJets(void) {
    memset(jets, 0, sizeof(jets));
    jetCount = 0;
    int i = addJet(); jetSet(i, "VX-71", "夜隼",   0, 0, 0, 0, 0);
    i = addJet();     jetSet(i, "LY-31", "龙吟",   1, 1, 1, 1, 1);
    i = addJet();     jetSet(i, "MX-09", "混天",   2, 2, 2, 2, 2);
}
/* 组装：以当前图纸 4 个部件生成一架新机（4^4 = 256 种基础组合） */
static int buildJet(int cost) {
    if (G.gold < cost) { showBanner("金币不足", C_RED); return -1; }
    int i = addJet();
    if (i < 0) { showBanner("机库已满", C_RED); return -1; }
    G.gold -= cost;
    char code[12], label[20];
    snprintf(code, sizeof(code), "BX-%02d", jetCount);
    snprintf(label, sizeof(label), "自组机 #%d", jetCount);
    jetSet(i, code, label, styleOfParts(G.blueprint),
           G.blueprint[0], G.blueprint[1], G.blueprint[2], G.blueprint[3]);
    G.activeJet = i;
    char buf[40]; snprintf(buf, sizeof(buf), "组装 %s [%s]", code, STYLE_NAME[jets[i].style]);
    showBanner(buf, C_CYAN);
    addParticles(pl->x, pl->y, C_CYAN, 18, 260, 0.7);
    return i;
}
/* 随机搭建：快速批量产出机型，用于凑齐 256 图纸 */
static int randomJet(int cost) {
    for (int k = 0; k < PART_SLOTS; k++) G.blueprint[k] = rand() % PART_PER_SLOT;
    return buildJet(cost);
}
/* 融合：两架机体各槽随机取件 + 25% 变异，产出全新机型 */
static int fuseJets(int a, int b) {
    if (a < 0 || b < 0 || a == b || a >= MAX_JETS || b >= MAX_JETS) return -1;
    if (!jets[a].used || !jets[b].used) { showBanner("需要两架机体", C_RED); return -1; }
    if (G.gold < JET_FUSE_COST) { showBanner("金币不足", C_RED); return -1; }
    int i = addJet();
    if (i < 0) { showBanner("机库已满", C_RED); return -1; }
    G.gold -= JET_FUSE_COST;
    int p[PART_SLOTS];
    for (int k = 0; k < PART_SLOTS; k++) {
        p[k] = (rand() % 2) ? jets[a].p[k] : jets[b].p[k];
        if (randf(0, 1) < 0.25) p[k] = rand() % PART_PER_SLOT;
    }
    char code[12], label[20];
    snprintf(code, sizeof(code), "FX-%02d", jetCount);
    snprintf(label, sizeof(label), "融合机 #%d", jetCount);
    int st = (jets[a].style == jets[b].style) ? jets[a].style : 2;
    jetSet(i, code, label, st, p[0], p[1], p[2], p[3]);
    G.activeJet = i;
    char buf[40]; snprintf(buf, sizeof(buf), "融合 %s [%s]", code, STYLE_NAME[jets[i].style]);
    showBanner(buf, C_PURPLE);
    addParticles(pl->x, pl->y, C_PURPLE, 24, 320, 0.8);
    return i;
}
/* 风格化 ASCII 俯视外形：机身定上/下轮廓，机翼定侧翼，引擎定尾喷，武器定中心符号 */
static void jetArt(int idx, char out[3][24]) {
    int p[PART_SLOTS] = {0, 0, 0, 0};
    int style = 2;
    if (idx >= 0 && idx < MAX_JETS && jets[idx].used) {
        for (int k = 0; k < PART_SLOTS; k++) p[k] = jets[idx].p[k];
        style = jets[idx].style;
    }
    const char wingL[4] = {'<', '(', '[', '{'};
    const char wingR[4] = {'>', ')', ']', '}'};
    const char eng[4]   = {'=', '~', '^', ':'};
    const char gun[4]   = {'#', '=', '*', '+'};
    char w = gun[p[3]];
    if (style == 0) {
        snprintf(out[0], 24, "    /%c\\    ", w);
        snprintf(out[1], 24, "   %c/%c\\%c   ", wingL[p[1]], w, wingR[p[1]]);
        snprintf(out[2], 24, "    \\%c/    ", eng[p[2]]);
    } else if (style == 1) {
        snprintf(out[0], 24, "  _%c/|\\%c_  ", wingL[p[1]], wingR[p[1]]);
        snprintf(out[1], 24, "  %c/%c|%c\\%c  ", wingL[p[1]], w, w, wingR[p[1]]);
        snprintf(out[2], 24, "   \\%c|%c/   ", eng[p[2]], eng[p[2]]);
    } else {
        snprintf(out[0], 24, "   /%c|%c\\   ", w, w);
        snprintf(out[1], 24, "  %c/%c/%c\\%c  ", wingL[p[1]], w, w, wingR[p[1]]);
        snprintf(out[2], 24, "   \\%c_%c/   ", eng[p[2]], eng[p[2]]);
    }
}
static char jetGlyph(void) {
    if (G.activeJet >= 0 && G.activeJet < MAX_JETS && jets[G.activeJet].used) {
        int s = jets[G.activeJet].style;
        return s == 0 ? '^' : (s == 1 ? 'A' : 'W');
    }
    return '@';
}

/* ===== 派生属性 ===== */
static int xpNeed(int lv) { return (int)(28.0 * pow(1.42, lv - 1)); }
static int shopPrice(int id) {
    const ShopDef *d = &SHOP_DEFS[id];
    return (int)(d->base * pow(d->growth, G.bought[id]) + 0.5);
}
static void shopEffect(int id, int lv, char *buf, size_t n) {
    switch (id) {
        case S_DMG:    snprintf(buf, n, "+%d%% 伤害", lv * 50); break;
        case S_RATE:   snprintf(buf, n, "-%d%% 间隔", (int)((1 - pow(0.945, lv)) * 100)); break;
        case S_MULTI:  snprintf(buf, n, "%d 发/次", 1 + lv); break;
        case S_CRIT:   snprintf(buf, n, "+%d%% 暴击", lv * 7); break;
        case S_HP:     snprintf(buf, n, "+%d 生命", lv * 22); break;
        case S_ARMOR:  snprintf(buf, n, "-%d%% 伤害", (int)((1 - pow(0.88, lv)) * 60)); break;
        case S_SPEED:  snprintf(buf, n, "+%d%% 移速", lv * 9); break;
        case S_PICKUP: snprintf(buf, n, "%d 半径", lv * 55); break;
        case S_GREED:  snprintf(buf, n, "+%d%% 金币", lv * 18); break;
        case S_XP:     snprintf(buf, n, "+%d%% 经验", lv * 22); break;
        case S_BOSS:   snprintf(buf, n, "BOSS +%d%%", lv * 22); break;
        case S_LINK:   snprintf(buf, n, "贯穿 %d", lv); break;
        case S_LEECH:  snprintf(buf, n, "击杀 +%d", (int)(lv * 1.6)); break;
        case S_LUCK:   snprintf(buf, n, "+%d%% 掉落", (int)(lv * 1.2)); break;
        case S_DASH:   snprintf(buf, n, "-%d%% 冷却", lv * 9); break;
        case S_RANGE:  snprintf(buf, n, "+%d%% 射程", lv * 10); break;
        default:       snprintf(buf, n, "-"); break;
    }
}
static Stats stats(void) {
    Stats s;
    const int *b = G.bought;
    s.damage      = 12.0 * (1.0 + b[S_DMG] * 0.5);
    s.interval    = 0.145 * pow(0.945, b[S_RATE]);
    s.bulletCount = 1 + b[S_MULTI];
    s.crit        = b[S_CRIT] * 0.07;
    s.maxHp       = 100.0 + b[S_HP] * 22.0;
    s.armor       = 1.0 - pow(0.88, b[S_ARMOR]);
    s.speedMul    = 1.0 + b[S_SPEED] * 0.09;
    s.pickupR     = b[S_PICKUP] * 55.0;
    s.goldMul     = 1.0 + b[S_GREED] * 0.18;
    s.xpMul       = 1.0 + b[S_XP] * 0.22;
    s.bossMul     = 1.0 + b[S_BOSS] * 0.22;
    s.link        = b[S_LINK];
    s.leech       = b[S_LEECH] * 1.6;
    s.luck        = b[S_LUCK] * 0.012;
    s.dashMul     = 1.0 - b[S_DASH] * 0.09;
    s.rangeMul    = 1.0 + b[S_RANGE] * 0.10;

    double jd = 0, jh = 0, js = 0, jr = 0;
    jetBonus(G.activeJet, &jd, &jh, &js, &jr);
    s.damage   *= (1.0 + jd);
    s.maxHp    *= (1.0 + jh);
    s.speedMul *= (1.0 + js);
    s.interval *= (1.0 - clampd(jr, 0.0, 0.55));

    /* ---- 海克斯强化叠加 ---- */
    s.damage   *= pow(1.15, G.hex[H_DMG]);
    s.interval *= pow(0.92, G.hex[H_RATE]);
    s.maxHp    += 25.0 * G.hex[H_HP];
    s.crit     += 0.12 * G.hex[H_CRIT];
    s.link     += G.hex[H_LINK];
    s.bossMul  *= pow(1.25, G.hex[H_BOSS]);
    s.goldMul  *= pow(1.15, G.hex[H_GOLD]);
    s.speedMul *= pow(1.08, G.hex[H_SPD]);
    s.bulletCount += G.hex[H_MULTI];
    s.armor    = 1.0 - (1.0 - s.armor) * pow(0.85, G.hex[H_ARMOR]);
    s.leech    += 3.0 * G.hex[H_LEECH];
    return s;
}

/* ===== 提示条 ===== */
static void showBanner(const char *text, int col) {
    snprintf(G.banner, sizeof(G.banner), "%s", text);
    G.bannerLife = 1.8; G.bannerCol = col;
}

/* ===== 敌人 / BOSS 生成 ===== */
static void spawnEnemy(int forceType, int counts) {
    int idx = addEnemy(); if (idx < 0) return;
    Enemy *e = &enemies[idx];
    int w = G.wave;

    int pool[8], pn = 0;
    pool[pn++] = E_GRUNT;
    if (w >= 2) pool[pn++] = E_FAST;
    if (w >= 3) pool[pn++] = E_TANK;
    if (w >= 4) pool[pn++] = E_SPLITTER;
    int type = forceType >= 0 ? forceType : pool[rand() % pn];
    const ETypeDef *b = &ETYPE[type];

    double scale = 1.0 + (w - 1) * 0.17;
    double goldScale = 1.0 + (w - 1) * 0.10;   /* v2：金币成长放缓 */
    int edge = rand() % 4;
    double m = 44.0, x = 0, y = 0;
    if      (edge == 0) { x = randf(0, WORLD_W); y = -m; }
    else if (edge == 1) { x = WORLD_W + m;       y = randf(0, WORLD_H); }
    else if (edge == 2) { x = randf(0, WORLD_W); y = WORLD_H + m; }
    else                { x = -m;                y = randf(0, WORLD_H); }

    e->x = x; e->y = y; e->type = type; e->level = w; e->counts = counts;
    e->hp = e->maxHp = b->hp * scale;
    e->speed = b->speed * (1.0 + (w - 1) * 0.042);
    e->r = b->r;
    e->dmg = b->dmg * (1.0 + (w - 1) * 0.10);
    /* v2：小怪爆金币数量削减（基础 1~4，且成长放缓） */
    e->gold = (int)(b->gold * goldScale + 0.5); if (e->gold < 1) e->gold = 1;
    e->xp = b->xp * sqrt(scale);
    e->phase = randf(0, TAU);
    e->spawnAnim = 1.0;
    e->sei = -1;                 /* 普通小怪，不是阶段精英 */
}

/* ===== BOSS 种类（每 5 关一种，机制各不相同） ===== */
static const char *BOSS_NAME[7] = {"晶簇核心","棱光之眼","熔核巨像","裂空之刃","虫巢母体","虚空主宰","终焉·母舰"};
static const char *BOSS_TAG[7]  = {"弹幕","激光","陨石","突进","召唤","瞬移","全技能"};
static const char  BOSS_CH[7]   = {'C','E','F','B','H','V','M'};
static const int   BOSS_COL[7]  = {C_CYAN,C_PURPLE,C_AMBER,C_RED,C_GREEN,C_PURPLE,C_GOLD};
static int stageOf(int wave) { return clampi((wave - 1) / BOSS_EVERY, 0, 5); }

/* 阶段精英：同阶段 BOSS 的缩小版 */
static const char *ELITE_NAME[6] = {"晶簇幼体","棱光浮游","熔渣爬行者","裂空掠袭者","虫巢孵育体","虚空残影"};
static const int   ELITE_COL[6]  = {C_CYAN,C_PURPLE,C_AMBER,C_RED,C_GREEN,C_PURPLE};
static const char  ELITE_CH[6]   = {'c','e','f','b','h','v'};
static const double ELITE_HP[6]  = {0.62,0.56,0.78,0.52,0.70,0.60};
static const double ELITE_R[6]   = {16,15,18,14,17,15};

/* ---- 激光 ---- */
static void spawnBeam(double x, double y, double ang, double rot,
                      double charge, double dur, double w, double dmg) {
    for (int i = 0; i < MAX_BEAMS; i++) if (!beams[i].used) {
        Beam *b = &beams[i];
        memset(b, 0, sizeof(Beam));
        b->x = x; b->y = y; b->ang = ang; b->rot = rot;
        b->charge = charge; b->dur = dur; b->w = w; b->dmg = dmg; b->used = 1;
        return;
    }
}
static void updateBeams(double dt) {
    for (int i = 0; i < MAX_BEAMS; i++) {
        Beam *b = &beams[i]; if (!b->used) continue;
        if (b->charge > 0) {
            b->charge -= dt;
            b->ang += b->rot * dt * 0.35;
            if (b->charge <= 0) b->active = b->dur;
            continue;
        }
        if (b->active > 0) {
            b->active -= dt;
            b->ang += b->rot * dt;
            double dx = cos(b->ang), dy = sin(b->ang);
            double rx = pl->x - b->x, ry = pl->y - b->y;
            double proj = rx * dx + ry * dy;
            if (proj > 0) {
                double perp = fabs(rx * -dy + ry * dx);
                if (perp < pl->r + b->w * 0.5 && pl->invuln <= 0) damagePlayer(b->dmg);
            }
            if (b->active <= 0) b->used = 0;
            continue;
        }
        b->used = 0;
    }
}

/* ---- 陨石 ---- */
static void spawnMeteor(double dmg, double delay, double r) {
    for (int i = 0; i < MAX_METEORS; i++) if (!meteors[i].used) {
        Meteor *m = &meteors[i];
        memset(m, 0, sizeof(Meteor));
        m->tx = clampd(pl->x + randf(-85, 85), 40, WORLD_W - 40);
        m->ty = clampd(pl->y + randf(-65, 65), 60, WORLD_H - 40);
        m->t = -delay; m->dur = 1.0; m->dmg = dmg; m->r = r; m->used = 1;
        return;
    }
}
static void updateMeteors(double dt) {
    for (int i = 0; i < MAX_METEORS; i++) {
        Meteor *m = &meteors[i]; if (!m->used) continue;
        m->t += dt;
        if (m->t < m->dur) continue;
        m->used = 0;
        addParticles(m->tx, m->ty, C_AMBER, 16, 270, 0.6);
        double rr = m->r + pl->r;
        if (dist2(m->tx, m->ty, pl->x, pl->y) < rr * rr && pl->invuln <= 0) damagePlayer(m->dmg);
    }
}

/* ---- 追踪弹 ---- */
static void homingBurst(Enemy *e, int n, double dmg) {
    double base = atan2(pl->y - e->y, pl->x - e->x);
    for (int i = 0; i < n; i++) {
        double a = base + (i - (n - 1) / 2.0) * 0.4;
        int idx = addEBullet(e->x, e->y, cos(a) * 125, sin(a) * 125, dmg);
        if (idx >= 0) ebullets[idx].hom = 1.7;
    }
}

/* ===== BOSS 弹幕 ===== */
static void bossFire(Enemy *e, int count, double speed, double spread, double dmg) {
    double base = atan2(pl->y - e->y, pl->x - e->x);
    for (int i = 0; i < count; i++) {
        double t = (count == 1) ? 0.0 : (i - (count - 1) / 2.0) / ((count - 1) / 2.0);
        double a = base + t * spread;
        addEBullet(e->x, e->y, cos(a) * speed, sin(a) * speed, dmg);
    }
}
static void ringFire(Enemy *e, int count, double speed, double dmg) {
    for (int i = 0; i < count; i++) {
        double a = TAU * i / count + G.t * 0.5;
        addEBullet(e->x, e->y, cos(a) * speed, sin(a) * speed, dmg);
    }
}
static void bossOrbit(Enemy *e, double dt, double mul) {
    e->orbit += dt * (0.55 + e->stage * 0.20) * mul;
    double tx = WORLD_W / 2 + sin(e->orbit) * WORLD_W * 0.30;
    double ty = 130.0 + sin(e->orbit * 0.7) * 45.0;
    double k = fmin(1.0, dt * 1.6);
    e->x += (tx - e->x) * k;
    e->y += (ty - e->y) * k;
}

/* 0 晶簇核心：扇形 + 环形 */
static void aiCrystal(Enemy *e, double dt) {
    bossOrbit(e, dt, 1.0);
    e->cd -= dt;
    if (e->cd <= 0) {
        bossFire(e, 4 + e->stage * 2, 165 + e->stage * 28, 0.6, e->dmg * 0.48);
        e->cd = fmax(0.8, 1.7 - e->stage * 0.3);
    }
    if (e->stage >= 1) {
        e->cd2 -= dt;
        if (e->cd2 <= 0) {
            ringFire(e, 10 + e->stage * 4, 105 + e->stage * 18, e->dmg * 0.40);
            e->cd2 = fmax(1.8, 2.8 - e->stage * 0.4);
        }
    }
}
/* 1 棱光之眼：蓄力激光扫射 */
static void aiLaser(Enemy *e, double dt) {
    bossOrbit(e, dt, 0.8);
    e->cd -= dt;
    if (e->cd <= 0) {
        int n = 1 + e->stage;
        double base = atan2(pl->y - e->y, pl->x - e->x);
        for (int i = 0; i < n; i++)
            spawnBeam(e->x, e->y, base + (i - (n - 1) / 2.0) * 0.6,
                      (i % 2 ? 1 : -1) * (0.45 + e->stage * 0.25),
                      0.85, 0.6, 14.0, 10.0 + e->stage * 3.0);
        e->cd = fmax(1.4, 2.9 - e->stage * 0.45);
    }
    e->cd2 -= dt;
    if (e->cd2 <= 0) { bossFire(e, 3, 120, 0.5, e->dmg * 0.3); e->cd2 = 2.0; }
}
/* 2 熔核巨像：陨石雨 */
static void aiMeteor(Enemy *e, double dt) {
    bossOrbit(e, dt, 0.65);
    e->cd -= dt;
    if (e->cd <= 0) {
        int n = 2 + e->stage;
        for (int i = 0; i < n; i++) spawnMeteor(10.0 + e->stage * 3.0, i * 0.22, 44.0);
        if (e->stage >= 1) ringFire(e, 8 + e->stage * 3, 95, e->dmg * 0.35);
        e->cd = fmax(1.6, 2.6 - e->stage * 0.35);
    }
}
/* 3 裂空之刃：蓄力冲刺 */
static void aiBlade(Enemy *e, double dt) {
    if (e->dashT > 0) {
        e->dashT -= dt;
        e->x = clampd(e->x + e->dashVx * dt, 26, WORLD_W - 26);
        e->y = clampd(e->y + e->dashVy * dt, 60, WORLD_H - 60);
        if (dist2(e->x, e->y, pl->x, pl->y) < (e->r + pl->r) * (e->r + pl->r) && pl->invuln <= 0)
            damagePlayer(e->dmg * 0.8);
        if (e->dashT <= 0) ringFire(e, 12 + e->stage * 4, 140, e->dmg * 0.42);
        return;
    }
    if (e->chargeT > 0) {
        e->chargeT -= dt;
        e->x += (pl->x - e->x) * fmin(1.0, dt * 1.2);
        e->y += (pl->y - e->y) * fmin(1.0, dt * 1.2);
        e->x = clampd(e->x, 60, WORLD_W - 60);
        e->y = clampd(e->y, 70, WORLD_H - 90);
        if (e->chargeT <= 0) {
            double a = atan2(pl->y - e->y, pl->x - e->x);
            double sp = 520 + e->stage * 110;
            e->dashVx = cos(a) * sp; e->dashVy = sin(a) * sp;
            e->dashT = 0.30 + e->stage * 0.04;
        }
        return;
    }
    bossOrbit(e, dt, 1.1);
    e->cd -= dt;
    if (e->cd <= 0) { e->chargeT = 0.55; e->cd = fmax(1.5, 2.8 - e->stage * 0.4); }
}
/* 4 虫巢母体：召唤 + 追踪弹 */
static void aiHive(Enemy *e, double dt) {
    bossOrbit(e, dt, 0.55);
    e->cd -= dt;
    if (e->cd <= 0) {
        for (int i = 0; i < 2 + e->stage; i++) spawnEnemy(-1, 0);
        e->cd = fmax(3.2, 5.5 - e->stage * 0.7);
    }
    e->cd2 -= dt;
    if (e->cd2 <= 0) { homingBurst(e, 2 + e->stage, e->dmg * 0.38); e->cd2 = fmax(1.6, 2.6 - e->stage * 0.3); }
}
/* 5 虚空主宰：瞬移 + 螺旋 */
static void aiVoid(Enemy *e, double dt) {
    e->cd -= dt;
    if (e->cd <= 0) {
        e->x = randf(WORLD_W * 0.18, WORLD_W * 0.82);
        e->y = randf(90, 210);
        e->cd = fmax(2.4, 3.8 - e->stage * 0.4);
        e->cd3 = 2.4;
    }
    if (e->cd3 > 0) {
        e->cd3 -= dt;
        e->cd2 -= dt;
        if (e->cd2 <= 0) {
            e->spin += 0.52;
            int arms = 2 + e->stage;
            for (int i = 0; i < arms; i++) {
                double a = e->spin + TAU * i / arms;
                addEBullet(e->x, e->y, cos(a) * 145, sin(a) * 145, e->dmg * 0.4);
            }
            e->cd2 = 0.24;
        }
    }
    e->x += (pl->x - e->x) * fmin(1.0, dt * 0.35);
    e->y += (150 - e->y) * fmin(1.0, dt * 1.2);
    e->x = clampd(e->x, 70, WORLD_W - 70);
    e->y = clampd(e->y, 80, 260);
}
/* 6 终焉·母舰：全技能 */
static void aiFinal(Enemy *e, double dt) {
    bossOrbit(e, dt, 0.75);
    e->cd -= dt;
    if (e->cd <= 0) {
        bossFire(e, 5 + e->stage * 2, 165 + e->stage * 20, 0.55, e->dmg * 0.45);
        e->cd = fmax(0.85, 1.6 - e->stage * 0.3);
    }
    e->cd2 -= dt;
    if (e->cd2 <= 0) { homingBurst(e, 2, e->dmg * 0.35); e->cd2 = fmax(2.2, 3.4 - e->stage * 0.5); }
    if (e->stage >= 1) {
        e->cd3 -= dt;
        if (e->cd3 <= 0) {
            double base = atan2(pl->y - e->y, pl->x - e->x);
            for (int i = 0; i < 2; i++)
                spawnBeam(e->x, e->y, base + (i ? PI : 0), (i ? -1 : 1) * 0.5, 0.85, 0.65, 15.0, 12.0);
            e->cd3 = 4.0;
        }
        e->cd4 -= dt;
        if (e->cd4 <= 0) { spawnMeteor(10.0, 0.0, 44.0); spawnMeteor(10.0, 0.3, 44.0); e->cd4 = 3.2; }
    }
    if (e->stage >= 2) {
        e->cd5 -= dt;
        if (e->cd5 <= 0) { spawnEnemy(-1, 0); spawnEnemy(-1, 0); e->cd5 = 6.5; }
    }
}

static void updateBoss(Enemy *e, double dt) {
    double ratio = e->hp / e->maxHp;
    int ns = ratio > 0.66 ? 0 : (ratio > 0.33 ? 1 : 2);
    if (ns > e->stage) {
        e->stage = ns;
        char buf[48];
        snprintf(buf, sizeof(buf), "%s · 阶段 %d", e->bossName, e->stage + 1);
        showBanner(buf, BOSS_COL[e->bkind]);
        addParticles(e->x, e->y, BOSS_COL[e->bkind], 26, 320, 0.8);
    }
    switch (e->bkind) {
        case 0: aiCrystal(e, dt); break;
        case 1: aiLaser(e, dt); break;
        case 2: aiMeteor(e, dt); break;
        case 3: aiBlade(e, dt); break;
        case 4: aiHive(e, dt); break;
        case 5: aiVoid(e, dt); break;
        default: aiFinal(e, dt); break;
    }
}

/* ===== 阶段精英小怪 ===== */
static void spawnStageElite(int wave) {
    int idx = addEnemy(); if (idx < 0) return;
    Enemy *e = &enemies[idx];
    int st = stageOf(wave);
    const ETypeDef *b = &ETYPE[E_ELITE];
    double scale = 1.0 + (wave - 1) * 0.17;
    double goldScale = 1.0 + (wave - 1) * 0.10;
    int edge = rand() % 4;
    double m = 44.0, x = 0, y = 0;
    if      (edge == 0) { x = randf(60, WORLD_W - 60); y = -m; }
    else if (edge == 1) { x = WORLD_W + m;            y = randf(60, WORLD_H - 60); }
    else if (edge == 2) { x = randf(60, WORLD_W - 60); y = WORLD_H + m; }
    else                { x = -m;                     y = randf(60, WORLD_H - 60); }
    e->x = x; e->y = y; e->type = E_ELITE; e->level = wave; e->counts = 1;
    e->sei = st;
    e->hp = e->maxHp = b->hp * scale * ELITE_HP[st];
    e->speed = b->speed * (1.0 + (wave - 1) * 0.042);
    e->r = ELITE_R[st];
    e->dmg = b->dmg * (1.0 + (wave - 1) * 0.10) * 0.5;
    e->gold = (int)(b->gold * goldScale * 3.0 + 0.5);
    if (e->gold < 1) e->gold = 1;
    e->xp = b->xp * sqrt(scale) * 2.2;
    e->phase = randf(0, TAU);
    e->spawnAnim = 1.0;
    e->cd = 1.6;
}

static void updateStageElite(Enemy *e, double dt) {
    if (e->sei == 3 && e->dashT > 0) {          /* 掠袭者冲刺 */
        e->dashT -= dt;
        e->x = clampd(e->x + e->dashVx * dt, e->r, WORLD_W - e->r);
        e->y = clampd(e->y + e->dashVy * dt, e->r, WORLD_H - e->r);
        if (dist2(e->x, e->y, pl->x, pl->y) < (e->r + pl->r) * (e->r + pl->r) && pl->invuln <= 0)
            damagePlayer(e->dmg);
        return;
    }
    e->cd -= dt;
    if (e->cd > 0) return;
    switch (e->sei) {
        case 0: {                               /* 三向晶片 */
            double base = atan2(pl->y - e->y, pl->x - e->x);
            for (int i = -1; i <= 1; i++)
                addEBullet(e->x, e->y, cos(base + i * 0.30) * 130, sin(base + i * 0.30) * 130, e->dmg * 0.55);
            e->cd = 2.0; break;
        }
        case 1:                                 /* 短激光 */
            spawnBeam(e->x, e->y, atan2(pl->y - e->y, pl->x - e->x), 0, 0.5, 0.32, 8.0, 9.0);
            e->cd = 3.4; break;
        case 2:                                 /* 小陨石 */
            spawnMeteor(8.0, 0.0, 28.0); e->cd = 3.0; break;
        case 3: {                               /* 短冲刺 */
            double a = atan2(pl->y - e->y, pl->x - e->x);
            e->dashVx = cos(a) * 430; e->dashVy = sin(a) * 430;
            e->dashT = 0.22; e->cd = 2.4; break;
        }
        case 4:                                 /* 孵化小怪 */
            spawnEnemy(0, 0); e->cd = 6.0; break;
        case 5: {                               /* 瞬移 + 十字弹 */
            e->x = clampd(pl->x + randf(-170, 170), 40, WORLD_W - 40);
            e->y = clampd(pl->y + randf(-170, 170), 40, WORLD_H - 40);
            for (int i = 0; i < 4; i++) {
                double a = TAU * i / 4 + randf(0, 0.3);
                addEBullet(e->x, e->y, cos(a) * 120, sin(a) * 120, e->dmg * 0.45);
            }
            e->cd = 3.6; break;
        }
    }
}

static void spawnBoss(int isFinal) {
    int idx = addEnemy(); if (idx < 0) return;
    Enemy *e = &enemies[idx];
    int kind = isFinal ? 6 : clampi(G.wave / BOSS_EVERY - 1, 0, 5);
    e->x = WORLD_W / 2; e->y = 110.0;
    e->type = E_ELITE; e->level = G.wave; e->counts = 0;
    e->isBoss = 1; e->isFinal = isFinal;
    e->bkind = kind; e->sei = -1;
    e->hp = e->maxHp = isFinal ? FINAL_BOSS_HP : BOSS_HP(G.wave);
    e->speed = 0.0;
    e->r = isFinal ? 46.0 : 30.0 + kind * 2.0;
    e->dmg = isFinal ? 34.0 : (18.0 + G.wave * 0.8);
    e->gold = isFinal ? 3000 : (180 + G.wave * 22);
    e->xp = isFinal ? 3000.0 : (120.0 + G.wave * 8.0);
    e->phase = randf(0, TAU);
    e->orbit = randf(0, TAU);
    e->spawnAnim = 1.0;
    e->cd = 1.4; e->cd2 = 1.2; e->cd3 = 2.2; e->cd4 = 2.6; e->cd5 = 4.5;
    e->spin = randf(0, TAU);
    snprintf(e->bossName, sizeof(e->bossName), "%s", BOSS_NAME[kind]);
    G.bossAlive = 1;
    char buf[56];
    snprintf(buf, sizeof(buf), "BOSS · %s [%s]  HP %.0f", BOSS_NAME[kind], BOSS_TAG[kind], e->maxHp);
    showBanner(buf, BOSS_COL[kind]);
}

static void startWave(void) {
    G.waveTotal = 4 + G.wave / 2;
    G.waveLeft  = G.waveTotal;
    G.spawnTimer = 0.0;
    G.between = 0;
    G.bossAlive = 0;
    if (G.wave % BOSS_EVERY == 0) {
        G.waveLeft = 3 + G.wave / 10;
        spawnBoss(0);
        char buf[40];
        snprintf(buf, sizeof(buf), "第 %d/%d 关 · BOSS 关", G.wave, MAX_WAVE);
        showBanner(buf, C_PURPLE);
    } else {
        int st = stageOf(G.wave);
        spawnStageElite(G.wave);
        G.waveLeft++;
        char buf[48];
        snprintf(buf, sizeof(buf), "第 %d/%d 关 · 精英 %s", G.wave, MAX_WAVE, ELITE_NAME[st]);
        showBanner(buf, ELITE_COL[st]);
    }
}

static void pickHex(int i) {
    if (i < 0 || i > 2) return;
    int id = G.pickOpt[i];
    if (id < 0) return;
    G.hex[id]++;
    G.pick3 = 0;
    char buf[48];
    snprintf(buf, sizeof(buf), "强化 · %s", HEXES[id].name);
    showBanner(buf, C_GOLD);
    addFloater(pl->x, pl->y - 40, HEXES[id].name, C_GOLD);
}
static void offerHex(void) {
    int opts[3], n = 0, guard = 0;
    while (n < 3 && guard++ < 300) {
        int id = rand() % HEX_N;
        if (G.hex[id] >= HEXES[id].max) continue;
        int dup = 0;
        for (int i = 0; i < n; i++) if (opts[i] == id) dup = 1;
        if (dup) continue;
        opts[n++] = id;
    }
    if (n == 0) return;
    while (n < 3) opts[n++] = -1;
    for (int i = 0; i < 3; i++) G.pickOpt[i] = opts[i];
    G.pick3 = 1;
    if (G.selftest) pickHex(rand() % 3);      /* 无头自测自动选一个 */
}

/* ===== 掉落金币 ===== */

static void dropCoins(double x, double y, int amount) {
    Stats s = stats();
    int total = (int)(amount * s.goldMul + 0.5); if (total < 1) total = 1;
    int count = (int)ceil(total / 1.6); if (count < 1) count = 1; if (count > 9) count = 9;
    int per = total / count, rest = total - per * count;
    for (int i = 0; i < count; i++) {
        int v = per + (rest > 0 ? 1 : 0); if (rest > 0) rest--;
        int ci = addCoin(); if (ci < 0) return;
        Coin *c = &coins[ci];
        double a = randf(0, TAU), sp = randf(70, 200);
        c->x = x; c->y = y; c->vx = cos(a)*sp; c->vy = sin(a)*sp;
        c->value = v; c->life = 16.0; c->phase = randf(0, TAU);
    }
    addParticles(x, y, C_GOLD, 5, 180, 0.4);
}

/* ===== 仅用于无头自测的目标索引（游戏内不存在自瞄） ===== */
static int findTargetForTest(void) {
    int best = -1; double bestD = 1e30;
    for (int i = 0; i < enemyCount; i++) {
        Enemy *e = &enemies[i]; if (!e->used) continue;
        double d2 = dist2(e->x, e->y, pl->x, pl->y);
        if (d2 < bestD) { bestD = d2; best = i; }
    }
    return best;
}

/* ===== 开火（完全手动瞄准） ===== */
static void fire(void) {
    if (!G.running || G.over) return;
    Stats s = stats();
    if (pl->fireCd > 0) return;
    pl->fireCd = s.interval;
    G.shots++;

    double aimDir = pl->aimAngle;
    int n = s.bulletCount;
    double spread = n > 1 ? 0.09 : 0.028;
    for (int i = 0; i < n; i++) {
        double off = (n == 1) ? 0.0 : (i - (n - 1) / 2.0) * spread;
        double a = aimDir + off + randf(-0.02, 0.02);
        int bi = addBullet(); if (bi < 0) break;
        Bullet *b = &bullets[bi];
        b->x = pl->x + cos(aimDir) * 16.0;
        b->y = pl->y + sin(aimDir) * 16.0;
        double spd = 780.0 * sqrt(s.rangeMul);
        b->vx = cos(a) * spd; b->vy = sin(a) * spd;
        b->r = 3.0; b->life = 1.5 * s.rangeMul;
        b->dmg = s.damage;
        b->crit = (randf(0, 1) < s.crit);
        b->hitsLeft = s.link;      /* 装备「串联弹芯」提供的贯穿次数 */
    }
    addParticles(pl->x + cos(aimDir)*18, pl->y + sin(aimDir)*18, C_CYAN, 2, 110, 0.16);
}

/* ===== 伤害 / 治疗 / 经验 ===== */
static void damagePlayer(double amount) {
    /* 无头自测 / 快照的测试驱动：忽略伤害以便推进关卡与稳定出图；
       交互模式中此分支永远不成立，游戏内没有任何无敌功能 */
    if (G.selftest || G.snapshot) return;
    Stats s = stats();
    double dmg = amount * (1.0 - s.armor * 0.6);
    if (dmg > 0.5) {
        pl->hp -= dmg;
        char buf[16]; snprintf(buf, sizeof(buf), "-%d", (int)dmg);
        addFloater(pl->x, pl->y - 20, buf, C_RED);
    }
    pl->invuln = 0.35;
    if (pl->hp <= 0) { pl->hp = 0; gameOver(); }
}
static void heal(double amount) {
    Stats s = stats();
    double before = pl->hp;
    pl->hp = fmin(s.maxHp, pl->hp + amount);
    if (pl->hp - before >= 1.0) {
        char buf[16]; snprintf(buf, sizeof(buf), "+%d", (int)(pl->hp - before));
        addFloater(pl->x, pl->y - 24, buf, C_GREEN);
    }
}
static void onLevelUp(void) {
    Stats s = stats();
    pl->maxHp = s.maxHp;
    pl->hp = fmin(pl->maxHp, pl->hp + 25.0);
    int bonus = (int)((4 + G.level * 2) * s.goldMul + 0.5);
    G.gold += bonus;
    char buf[16]; snprintf(buf, sizeof(buf), "LV.%d", G.level);
    addFloater(pl->x, pl->y - 40, buf, C_PURPLE);
    addParticles(pl->x, pl->y, C_PURPLE, 22, 300, 0.8);
    char bb[24]; snprintf(bb, sizeof(bb), "等级提升 LV.%d", G.level);
    showBanner(bb, C_GOLD);
}
static void gainXP(double amount) {
    Stats s = stats();
    G.xp += amount * s.xpMul;
    int need = xpNeed(G.level);
    int guard = 0;
    while (G.xp >= need && guard++ < 200) {
        G.xp -= need; G.level++;
        onLevelUp();
        need = xpNeed(G.level);
    }
}

/* ===== 击杀 ===== */
static void killEnemy(int idx) {
    Enemy *e = &enemies[idx];
    double ex = e->x, ey = e->y;
    int gold = e->gold, type = e->type, level = e->level;
    int isBoss = e->isBoss, isFinal = e->isFinal, counts = e->counts;
    int col = isBoss ? (isFinal ? C_GOLD : C_PURPLE) : ETYPE[type].col;
    double dmg = e->dmg, xp = e->xp;
    char bname[24]; snprintf(bname, sizeof(bname), "%s", e->bossName);

    removeEnemy(idx);
    G.kills++;
    G.score += (int)(10 * level * (1 + gold * 0.1));
    dropCoins(ex, ey, gold);
    gainXP(xp);
    addParticles(ex, ey, col, isBoss ? 46 : 10, isBoss ? 380 : 250, isBoss ? 1.0 : 0.55);
    if (counts) G.waveLeft = (G.waveLeft > 0) ? G.waveLeft - 1 : 0;

    Stats s = stats();
    if (s.leech > 0) heal(s.leech);

    if (isBoss) {
        G.bossAlive = 0;
        G.bossKills++;
        for (int i = 0; i < ebulletCount; i++) ebullets[i].used = 0;
        if (isFinal) {
            G.victory = 1; G.running = 0;
            showBanner("通关 · 终焉·母舰 已击破", C_GOLD);
        } else {
            char buf[40]; snprintf(buf, sizeof(buf), "%s 已击破", bname);
            showBanner(buf, C_GOLD);
        }
    } else if (type == E_SPLITTER) {
        double sc = 1.0 + (G.wave - 1) * 0.17;
        for (int k = 0; k < 2; k++) {
            int ni = addEnemy(); if (ni < 0) break;
            Enemy *c = &enemies[ni];
            c->x = ex + randf(-16, 16); c->y = ey + randf(-16, 16);
            c->type = E_GRUNT; c->level = G.wave; c->counts = 1;
            c->hp = c->maxHp = ETYPE[E_GRUNT].hp * 0.5 * sc;
            c->speed = 100.0; c->r = 8.0;
            c->dmg = dmg * 0.5;
            c->gold = (int)(gold * 0.4 + 0.5); if (c->gold < 1) c->gold = 1;
            c->xp = xp * 0.35; c->phase = randf(0, TAU); c->spawnAnim = 0.6;
            c->sei = -1;
            G.waveLeft++;
        }
    }
    if (randf(0, 1) < 0.04 + s.luck) {
        int pi = addPickup(); if (pi >= 0) {
            pickups[pi].x = ex; pickups[pi].y = ey;
            pickups[pi].life = 14.0; pickups[pi].phase = 0;
        }
    }
}

/* ===== 商店 ===== */
static void buy(int id) {
    if (G.bought[id] >= SHOP_DEFS[id].maxLv) return;
    int price = shopPrice(id);
    if (G.gold < price) { showBanner("金币不足", C_RED); return; }
    G.gold -= price; G.bought[id]++;
    char buf[32]; snprintf(buf, sizeof(buf), "%s LV%d", SHOP_DEFS[id].name, G.bought[id]);
    addFloater(pl->x, pl->y - 34, buf, SHOP_DEFS[id].col);
}

/* ===== 游戏流程 ===== */
static void startGame(void) {
    /* memset 会抹掉终端尺寸/运行模式，必须先保存再回填 */
    int k_snapshot = G.snapshot, k_plain = G.plain, k_selftest = G.selftest;
    int k_cols = G.cols, k_rows = G.rows, k_tab = G.tab, k_sel = G.sel;
    memset(&G, 0, sizeof(Game));
    G.snapshot = k_snapshot; G.plain = k_plain; G.selftest = k_selftest;
    G.cols = k_cols; G.rows = k_rows; G.tab = k_tab; G.sel = k_sel;
    G.running = 1; G.over = 0; G.victory = 0;
    G.wave = 1; G.level = 1;

    memset(&P, 0, sizeof(Player));
    pl->x = WORLD_W / 2; pl->y = WORLD_H * 0.62;
    pl->r = 13.0; pl->hp = pl->maxHp = 100.0;
    pl->aimAngle = -PI / 2; pl->lastMoveAngle = -PI / 2;

    for (int i = 0; i < MAX_ENEMIES; i++)   { enemies[i].used = 0; }
    for (int i = 0; i < MAX_BULLETS; i++)   { bullets[i].used = 0; }
    for (int i = 0; i < MAX_EBULLETS; i++)  { ebullets[i].used = 0; }
    for (int i = 0; i < MAX_COINS; i++)     { coins[i].used = 0; }
    for (int i = 0; i < MAX_PICKUPS; i++)   { pickups[i].used = 0; }
    for (int i = 0; i < MAX_FLOATERS; i++)  { floaters[i].used = 0; }
    for (int i = 0; i < MAX_PARTICLES; i++) { particles[i].used = 0; }
    enemyCount = bulletCount = ebulletCount = coinCount = 0;
    pickupCount = floaterCount = particleCount = 0;

    initJets();
    G.activeJet = 0;
    for (int k = 0; k < PART_SLOTS; k++) G.blueprint[k] = 0;

    /* 永远从第 1 关开始，不存在选关 / 跳关入口 */
    startWave();
}
static void gameOver(void) {
    G.over = 1; G.running = 0;
    showBanner("SIGNAL LOST", C_RED);
    addParticles(pl->x, pl->y, C_CYAN, 30, 380, 0.9);
}

/* ===== 逻辑更新 ===== */
static void updateEffects(double dt) {
    for (int i = 0; i < particleCount; i++) if (particles[i].used) {
        Particle *p = &particles[i];
        p->x += p->vx * dt; p->y += p->vy * dt;
        p->vx *= pow(0.05, dt); p->vy *= pow(0.05, dt);
        p->life -= dt; if (p->life <= 0) p->used = 0;
    }
    for (int i = 0; i < floaterCount; i++) if (floaters[i].used) {
        Floater *f = &floaters[i];
        f->y += f->vy * dt; f->vy *= pow(0.2, dt);
        f->life -= dt; if (f->life <= 0) f->used = 0;
    }
}

static void update(double dt) {
    if (!G.running || G.over || G.victory) {
        updateEffects(dt);
        if (G.bannerLife > 0) G.bannerLife -= dt;
        return;
    }
    if (G.pick3) { updateEffects(dt); if (G.bannerLife > 0) G.bannerLife -= dt; return; }
    G.t += dt;
    if (G.bannerLife > 0) G.bannerLife -= dt;

    Stats s = stats();
    if (pl->maxHp < s.maxHp) { double diff = s.maxHp - pl->maxHp; pl->maxHp = s.maxHp; pl->hp += diff; }
    else if (pl->maxHp > s.maxHp) { pl->maxHp = s.maxHp; pl->hp = fmin(pl->hp, pl->maxHp); }

    /* 移动 */
    double mx = 0, my = 0;
    if (keyDown('w')) my -= 1;
    if (keyDown('s')) my += 1;
    if (keyDown('a')) mx -= 1;
    if (keyDown('d')) mx += 1;
    double ml = hypot(mx, my);
    if (ml > 0) { mx /= ml; my /= ml; pl->lastMoveAngle = atan2(my, mx); }
    double spd = 235.0 * s.speedMul;

    if (pl->dashTime > 0) {
        pl->dashTime -= dt;
        pl->x += pl->dashDx * spd * 2.9 * dt;
        pl->y += pl->dashDy * spd * 2.9 * dt;
        if (randf(0, 1) < 0.6) addParticles(pl->x, pl->y, C_CYAN, 1, 40, 0.35);
    } else {
        pl->x += mx * spd * dt;
        pl->y += my * spd * dt;
    }
    pl->x = clampd(pl->x, pl->r, WORLD_W - pl->r);
    pl->y = clampd(pl->y, pl->r, WORLD_H - pl->r);

    /* 手动瞄准：J / L 按住旋转，不再有任何自动吸附 */
    if (keyDown('j')) pl->aimAngle -= 1.8 * dt;
    if (keyDown('l')) pl->aimAngle += 1.8 * dt;
    while (pl->aimAngle > PI)  pl->aimAngle -= TAU;
    while (pl->aimAngle < -PI) pl->aimAngle += TAU;

    pl->fireCd = fmax(0.0, pl->fireCd - dt);
    pl->dashCd = fmax(0.0, pl->dashCd - dt);
    pl->invuln = fmax(0.0, pl->invuln - dt);

    if (G.firing) fire();

    /* ---- 我方子弹 ---- */
    for (int i = 0; i < bulletCount; i++) {
        Bullet *b = &bullets[i]; if (!b->used) continue;
        b->x += b->vx * dt; b->y += b->vy * dt; b->life -= dt;
        if (b->life <= 0 || b->x < -20 || b->x > WORLD_W + 20 || b->y < -20 || b->y > WORLD_H + 20) {
            b->used = 0; continue;
        }
        for (int j = 0; j < enemyCount; j++) {
            Enemy *e = &enemies[j]; if (!e->used) continue;
            if (j < 32 && (b->hitMask & (1u << j))) continue;
            double rr = b->r + e->r;
            if (dist2(b->x, b->y, e->x, e->y) < rr * rr) {
                double dmg = b->crit ? b->dmg * 2.2 : b->dmg;
                if (e->isBoss) dmg *= s.bossMul;
                e->hp -= dmg; e->hitFlash = 1.0; G.hits++;
                addParticles(b->x, b->y, b->crit ? C_GOLD : C_WHITE, b->crit ? 5 : 2, 110, 0.22);
                if (!e->isBoss) {
                    double dd = hypot(e->x - b->x, e->y - b->y); if (dd < 0.001) dd = 1;
                    e->x += (e->x - b->x) / dd * 9.0;
                    e->y += (e->y - b->y) / dd * 9.0;
                }
                if (j < 32) b->hitMask |= (1u << j);
                if (e->hp <= 0) killEnemy(j);
                if (b->hitsLeft > 0) b->hitsLeft--;
                else { b->used = 0; break; }
            }
        }
    }

    /* ---- 敌方弹幕 ---- */
    for (int i = 0; i < ebulletCount; i++) {
        EBullet *b = &ebullets[i]; if (!b->used) continue;
        if (b->hom > 0) {                       /* 追踪弹缓慢转向玩家 */
            double cur = atan2(b->vy, b->vx);
            double want = atan2(pl->y - b->y, pl->x - b->x);
            double d = want - cur;
            while (d > PI) d -= TAU;
            while (d < -PI) d += TAU;
            double step = b->hom * dt;
            if (d > step) d = step; else if (d < -step) d = -step;
            double sp = hypot(b->vx, b->vy);
            double na = cur + d;
            b->vx = cos(na) * sp; b->vy = sin(na) * sp;
        }
        b->x += b->vx * dt; b->y += b->vy * dt; b->life -= dt;
        if (b->life <= 0 || b->x < -30 || b->x > WORLD_W + 30 || b->y < -30 || b->y > WORLD_H + 30) {
            b->used = 0; continue;
        }
        double rr = b->r + pl->r;
        if (dist2(b->x, b->y, pl->x, pl->y) < rr * rr) {
            if (pl->invuln <= 0) damagePlayer(b->dmg);
            b->used = 0;
        }
    }

    updateBeams(dt);
    updateMeteors(dt);

    /* ---- 敌人 ---- */
    int alive = 0;
    for (int i = 0; i < enemyCount; i++) {
        Enemy *e = &enemies[i]; if (!e->used) continue;
        alive++;
        e->hitFlash  = fmax(0.0, e->hitFlash - dt * 5);
        e->spawnAnim = fmax(0.0, e->spawnAnim - dt * 2.2);

        if (e->isBoss) { updateBoss(e, dt); continue; }

        double dx = pl->x - e->x, dy = pl->y - e->y;
        double ang = atan2(dy, dx) + sin(G.t * 2 + e->phase) * 0.25;
        double sp = e->speed * (e->spawnAnim > 0 ? 0.25 : 1.0);
        e->x += cos(ang) * sp * dt;
        e->y += sin(ang) * sp * dt;

        for (int k = i - 1; k >= 0; k--) {
            Enemy *o = &enemies[k]; if (!o->used || o->isBoss) continue;
            double rr = e->r + o->r;
            double ddx = e->x - o->x, ddy = e->y - o->y, dd2 = ddx*ddx + ddy*ddy;
            if (dd2 < rr*rr && dd2 > 0.01) {
                double dd = sqrt(dd2), push = (rr - dd) * 0.5;
                e->x += ddx/dd*push; e->y += ddy/dd*push;
                o->x -= ddx/dd*push; o->y -= ddy/dd*push;
            }
        }

        if (e->sei >= 0) updateStageElite(e, dt);

        double rr = e->r + pl->r;
        if (dist2(e->x, e->y, pl->x, pl->y) < rr*rr && pl->invuln <= 0) {
            damagePlayer(e->dmg);
            addParticles(e->x, e->y, e->sei >= 0 ? ELITE_COL[e->sei] : ETYPE[e->type].col, 8, 180, 0.4);
            if (e->sei >= 0) {
                /* 阶段精英不会同归于尽，只被弹开 */
                double dd = hypot(e->x - pl->x, e->y - pl->y);
                if (dd < 0.001) dd = 1;
                e->x = clampd(pl->x + (e->x - pl->x) / dd * (rr + 6), e->r, WORLD_W - e->r);
                e->y = clampd(pl->y + (e->y - pl->y) / dd * (rr + 6), e->r, WORLD_H - e->r);
            } else {
                if (e->counts) G.waveLeft = (G.waveLeft > 0) ? G.waveLeft - 1 : 0;
                removeEnemy(i); alive--;
            }
        }
    }

    /* ---- 金币 ---- */
    double pickR = pl->r + 46.0 + s.pickupR;
    for (int i = 0; i < coinCount; i++) {
        Coin *c = &coins[i]; if (!c->used) continue;
        c->life -= dt; c->phase += dt * 6;
        double d2 = dist2(c->x, c->y, pl->x, pl->y);
        double d  = sqrt(d2); if (d < 0.001) d = 0.001;
        if (d2 < (pl->r + 11) * (pl->r + 11)) {
            G.gold += c->value;
            char buf[16]; snprintf(buf, sizeof(buf), "+%d", c->value);
            addFloater(pl->x + randf(-12, 12), pl->y - 18, buf, C_GOLD);
            c->used = 0; continue;
        }
        if (d2 < pickR * pickR) {
            double pull = 900.0 * (1.0 - d / pickR) + 320.0;
            c->x += (pl->x - c->x) / d * pull * dt;
            c->y += (pl->y - c->y) / d * pull * dt;
            c->vx = c->vy = 0;
        } else {
            c->x += c->vx * dt; c->y += c->vy * dt;
            double damp = pow(0.0015, dt);
            c->vx *= damp; c->vy *= damp;
            c->x = clampd(c->x, 6, WORLD_W - 6);
            c->y = clampd(c->y, 6, WORLD_H - 6);
        }
        if (c->life <= 0) c->used = 0;
    }

    /* ---- 补给包 ---- */
    for (int i = 0; i < pickupCount; i++) {
        Pickup *p = &pickups[i]; if (!p->used) continue;
        p->life -= dt; p->phase += dt * 4;
        if (p->life <= 0) { p->used = 0; continue; }
        double rr = 9.0 + pl->r + 6.0;
        if (dist2(p->x, p->y, pl->x, pl->y) < rr*rr) {
            heal(28);
            addParticles(p->x, p->y, C_GREEN, 8, 160, 0.4);
            p->used = 0;
        }
    }

    /* ---- 关卡 / BOSS 流程 ---- */
    if (!G.between && !G.finalPhase) {
        if (G.waveLeft > 0 && alive < 24) {
            G.spawnTimer -= dt;
            if (G.spawnTimer <= 0) {
                spawnEnemy(-1, 1);
                G.spawnTimer = fmax(0.30, 0.95 - G.wave * 0.05);
            }
        } else if (G.waveLeft <= 0 && alive == 0 && !G.bossAlive) {
            if (G.wave >= MAX_WAVE) {
                /* 30 关通关 → 最终 BOSS */
                G.finalPhase = 1;
                G.waveLeft = 0;
                offerHex();
                spawnBoss(1);
                showBanner("最终 BOSS · 终焉·母舰", C_GOLD);
            } else {
                G.between = 1; G.waveGap = 2.0;
                G.wave++;
                int bonus = (int)((18 + G.wave * 6) * s.goldMul + 0.5);
                G.gold += bonus;
                char buf[28]; snprintf(buf, sizeof(buf), "+%d 过关奖励", bonus);
                addFloater(pl->x, pl->y - 46, buf, C_GOLD);
                showBanner("区域清除", C_CYAN);
                offerHex();
            }
        }
    } else if (G.between) {
        G.waveGap -= dt;
        if (G.waveGap <= 0) startWave();
    }

    updateEffects(dt);
}

/* ===== 终端渲染 ===== */
#define ROWCAP 96
static char cells[MAX_ROWS][MAX_COLS + 1];
static unsigned char colors[MAX_ROWS][MAX_COLS + 1];
static char panelLine[MAX_ROWS][ROWCAP];

static void putCell(int gx, int gy, char ch, int col) {
    if (gx < 0 || gx >= G.cols || gy < 0 || gy >= G.rows) return;
    cells[gy][gx] = ch; colors[gy][gx] = (unsigned char)col;
}
static void mapToCell(double x, double y, int *gx, int *gy) {
    *gx = (int)(x / WORLD_W * (G.cols - 1));
    *gy = (int)(y / WORLD_H * (G.rows - 1));
}

/* UTF-8 显示宽度：CJK 占 2 列、ASCII 占 1 列 */
static int dispWidth(const char *s) {
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p < 0x80) { w += 1; p += 1; }
        else if ((*p & 0xE0) == 0xC0) { w += 1; p += 2; }
        else if ((*p & 0xF0) == 0xE0) { w += 2; p += 3; }
        else if ((*p & 0xF8) == 0xF0) { w += 2; p += 4; }
        else { w += 1; p += 1; }
    }
    return w;
}
static void rowStart(char *b, const char *s) { snprintf(b, ROWCAP, "%s", s); }
static void rowAdd(char *b, const char *s) {
    size_t u = strlen(b);
    if (u + 1 < ROWCAP) snprintf(b + u, ROWCAP - u, "%s", s);
}
static void rowPad(char *b, const char *s, int width) {
    rowAdd(b, s);
    int pad = width - dispWidth(s);
    size_t u = strlen(b);
    for (int i = 0; i < pad && u + 1 < ROWCAP; i++) b[u++] = ' ';
    b[u] = '\0';
}
static int tabMax(int tab) {
    if (tab == 0) return SHOP_N;
    if (tab == 2) return jetCount > 0 ? jetCount : 1;
    if (tab == 3) return PART_SLOTS;
    return 1;
}

static void buildPanel(void) {
    Stats s = stats();
    for (int i = 0; i < G.rows; i++) panelLine[i][0] = '\0';
    int ln = 0;
    if (G.pick3) {
        snprintf(panelLine[ln++], ROWCAP, "== 强化 · 第 %d 关 ==", G.wave - 1);
        snprintf(panelLine[ln++], ROWCAP, "选一个，永久叠加：");
        if (ln < G.rows) panelLine[ln++][0] = '\0';
        for (int i = 0; i < 3 && ln < G.rows - 2; i++) {
            int id = G.pickOpt[i];
            if (id < 0) continue;
            const HexDef *h = &HEXES[id];
            snprintf(panelLine[ln++], ROWCAP, " [%d] %s", i + 1, h->name);
            snprintf(panelLine[ln++], ROWCAP, "     %s", h->desc);
            snprintf(panelLine[ln++], ROWCAP, "     <%s> 已叠 %d", RAR_NAME[h->rar], G.hex[id]);
            if (ln < G.rows) panelLine[ln++][0] = '\0';
        }
        return;
    }
    const char *tabs[4] = {"商店", "装备", "机库", "图纸"};
    if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "终端 // %s (Tab 切换)", tabs[G.tab]);
    if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "------------------------------");

    if (G.tab == 0) {
        /* 商店：16 款装备，带滚动 */
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "金币 %d $   第 %d/%d 关", G.gold, G.wave, MAX_WAVE);
        int visible = G.rows - ln - 2;
        if (visible < 1) visible = 1;
        int top = 0;
        if (SHOP_N > visible) top = clampi(G.sel - visible / 2, 0, SHOP_N - visible);
        for (int id = top; id < SHOP_N && ln < G.rows - 1; id++) {
            const ShopDef *d = &SHOP_DEFS[id];
            int lv = G.bought[id];
            char eff[24], price[12], head[16], lvtxt[8], *b = panelLine[ln++];
            shopEffect(id, lv, eff, sizeof(eff));
            if (lv >= d->maxLv) snprintf(price, sizeof(price), "MAX");
            else snprintf(price, sizeof(price), "%d$", shopPrice(id));
            snprintf(head, sizeof(head), "%c ", d->icon);
            snprintf(lvtxt, sizeof(lvtxt), "L%d", lv);
            rowStart(b, id == G.sel ? "> " : "  ");
            rowPad(b, head, 3);
            rowPad(b, d->name, 11);
            rowPad(b, lvtxt, 4);
            rowPad(b, eff, 11);
            rowAdd(b, price);
        }
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "↑↓ 选择   Enter 购买");
    } else if (G.tab == 1) {
        /* 装备 / 属性总览 */
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "已装备强化");
        int shown = 0;
        for (int id = 0; id < SHOP_N && ln < G.rows - 9; id++) {
            if (G.bought[id] <= 0) continue;
            char eff[24], head[16], lvtxt[8], *b = panelLine[ln++];
            shopEffect(id, G.bought[id], eff, sizeof(eff));
            snprintf(head, sizeof(head), "%c ", SHOP_DEFS[id].icon);
            snprintf(lvtxt, sizeof(lvtxt), "L%d", G.bought[id]);
            rowStart(b, "  ");
            rowPad(b, head, 3);
            rowPad(b, SHOP_DEFS[id].name, 11);
            rowPad(b, lvtxt, 4);
            rowAdd(b, eff);
            shown++;
        }
        if (!shown && ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "  （暂无，去商店购买）");
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "------------------------------");
        char v[8][16];
        snprintf(v[0], 16, "%.0f", s.damage);
        snprintf(v[1], 16, "%.0fms", s.interval * 1000);
        snprintf(v[2], 16, "%d", s.bulletCount);
        snprintf(v[3], 16, "%.0f%%", s.crit * 100);
        snprintf(v[4], 16, "%.0f", s.maxHp);
        snprintf(v[5], 16, "BOSS %.0f%%", (s.bossMul - 1) * 100);
        snprintf(v[6], 16, "贯穿 %d", s.link);
        snprintf(v[7], 16, "%.0f%%", (s.goldMul - 1) * 100);
        const char *nm[8] = {"伤害","间隔","弹数","暴击","生命","猎杀","串联","金币"};
        for (int i = 0; i < 8 && ln < G.rows; i++) {
            char *b = panelLine[ln++];
            rowStart(b, "  ");
            rowPad(b, nm[i], 10);
            rowAdd(b, v[i]);
        }
    } else if (G.tab == 2) {
        /* 机库：已有飞机 + 风格化外形 */
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP,
                                  "机库 %d/%d  图纸 %d/256", jetCount, MAX_JETS, comboCount());
        int visible = G.rows - ln - 6;
        if (visible < 1) visible = 1;
        int top = 0;
        if (jetCount > visible) top = clampi(G.sel - visible / 2, 0, jetCount - visible);
        for (int id = top; id < jetCount && ln < G.rows - 5; id++) {
            if (!jets[id].used) continue;
            char head[20], st[10], *b = panelLine[ln++];
            snprintf(head, sizeof(head), "%s", jets[id].code);
            snprintf(st, sizeof(st), "[%s]", STYLE_NAME[jets[id].style]);
            rowStart(b, id == G.sel ? "> " : "  ");
            rowPad(b, head, 8);
            rowPad(b, st, 6);
            rowPad(b, jets[id].label, 12);
            rowAdd(b, id == G.activeJet ? "出战" : "  ");
        }
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "------------------------------");
        int idx = (jetCount > 0) ? clampi(G.sel, 0, jetCount - 1) : -1;   /* 空机库时为 -1，避免 jets[-1] 越界 */
        if (ln < G.rows) {
            int ok = (idx >= 0 && jets[idx].used);
            snprintf(panelLine[ln++], ROWCAP, "%s · %s",
                     ok ? jets[idx].label : "机体",
                     ok ? STYLE_FULL[jets[idx].style] : "虚构原型");
        }
        char art[3][24];
        jetArt(idx, art);
        for (int k = 0; k < 3 && ln < G.rows - 1; k++) {
            snprintf(panelLine[ln++], ROWCAP, "  %s", art[k]);
        }
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "Enter 出战  F 融合  (虚构机型)");
    } else {
        /* 图纸：4 槽换件 → 组装 / 随机 / 融合 */
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP,
                                  "图纸 %d/256   机体 %d/%d", comboCount(), jetCount, MAX_JETS);
        for (int k = 0; k < PART_SLOTS && ln < G.rows - 6; k++) {
            int v = G.blueprint[k];
            const Part *q = &PARTS[k][v];
            char left[32], right[20], *b = panelLine[ln++];
            snprintf(left, sizeof(left), "%s %s", SLOT_NAME[k], q->name);
            snprintf(right, sizeof(right), "[%s]", STYLE_NAME[q->style]);
            rowStart(b, k == G.sel ? "> " : "  ");
            rowPad(b, left, 19);
            rowAdd(b, right);
        }
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "------------------------------");
        char art[3][24];
        jetArt(-1, art);
        for (int k = 0; k < 3 && ln < G.rows - 2; k++) {
            snprintf(panelLine[ln++], ROWCAP, "  %s", art[k]);
        }
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "↑↓ 选槽  ←→ 换件");
        if (ln < G.rows) snprintf(panelLine[ln++], ROWCAP, "B/Enter 组装%d$  G 随机%d$",
                                  JET_BUILD_COST, JET_RANDOM_COST);
    }
}

static void render(void) {
    for (int y = 0; y < G.rows; y++) {
        for (int x = 0; x < G.cols; x++) {
            cells[y][x] = ((x % 8 == 0) && (y % 4 == 0)) ? '.' : ' ';
            colors[y][x] = C_GRID;
        }
        cells[y][G.cols] = '\0';
    }

    for (int i = 0; i < pickupCount; i++) if (pickups[i].used) {
        int gx, gy; mapToCell(pickups[i].x, pickups[i].y, &gx, &gy);
        putCell(gx, gy, '+', C_GREEN);
    }
    for (int i = 0; i < particleCount; i++) if (particles[i].used) {
        int gx, gy; mapToCell(particles[i].x, particles[i].y, &gx, &gy);
        putCell(gx, gy, '.', particles[i].col);
    }
    for (int i = 0; i < coinCount; i++) if (coins[i].used) {
        int gx, gy; mapToCell(coins[i].x, coins[i].y, &gx, &gy);
        putCell(gx, gy, '$', C_GOLD);
    }
    for (int i = 0; i < ebulletCount; i++) if (ebullets[i].used) {
        int gx, gy; mapToCell(ebullets[i].x, ebullets[i].y, &gx, &gy);
        putCell(gx, gy, 'o', C_RED);
    }
    for (int i = 0; i < enemyCount; i++) if (enemies[i].used) {
        Enemy *e = &enemies[i];
        int gx, gy; mapToCell(e->x, e->y, &gx, &gy);
        if (e->isBoss) {
            int col = e->hitFlash > 0.3 ? C_WHITE : BOSS_COL[e->bkind];
            putCell(gx - 1, gy, '<', col);
            putCell(gx,     gy, BOSS_CH[e->bkind], col);
            putCell(gx + 1, gy, '>', col);
            if (gy > 0) putCell(gx, gy - 1, (char)('0' + e->stage + 1), col);
        } else if (e->sei >= 0) {
            int col = e->hitFlash > 0.3 ? C_WHITE : ELITE_COL[e->sei];
            putCell(gx, gy, ELITE_CH[e->sei], col);
            if (gy > 0) putCell(gx, gy - 1, '*', col);
        } else {
            int col = e->hitFlash > 0.3 ? C_WHITE : ETYPE[e->type].col;
            putCell(gx, gy, ETYPE[e->type].ch, col);
            if (e->type == E_ELITE && gy > 0) putCell(gx, gy - 1, '!', C_GOLD);
        }
    }
    for (int i = 0; i < bulletCount; i++) if (bullets[i].used) {
        int gx, gy; mapToCell(bullets[i].x, bullets[i].y, &gx, &gy);
        putCell(gx, gy, bullets[i].crit ? '*' : '-', bullets[i].crit ? C_GOLD : C_CYAN);
    }
    for (int i = 0; i < MAX_BEAMS; i++) if (beams[i].used) {
        Beam *b = &beams[i];
        int col = b->charge > 0 ? C_AMBER : C_RED;
        char ch = b->charge > 0 ? '.' : '=';
        for (double t = 10; t < 1400; t += 12) {
            int gx, gy;
            mapToCell(b->x + cos(b->ang) * t, b->y + sin(b->ang) * t, &gx, &gy);
            putCell(gx, gy, ch, col);
        }
    }
    for (int i = 0; i < MAX_METEORS; i++) if (meteors[i].used) {
        Meteor *m = &meteors[i];
        int gx, gy; mapToCell(m->tx, m->ty, &gx, &gy);
        putCell(gx, gy, m->t > 0 ? 'x' : '.', C_AMBER);
    }
    {
        int gx, gy; mapToCell(pl->x, pl->y, &gx, &gy);
        putCell(gx, gy, jetGlyph(), G.over ? C_RED : C_CYAN);
        double ax = pl->x + cos(pl->aimAngle) * 40.0;
        double ay = pl->y + sin(pl->aimAngle) * 40.0;
        int cx2, cy2; mapToCell(ax, ay, &cx2, &cy2);
        if (cx2 != gx || cy2 != gy) putCell(cx2, cy2, 'x', C_DIM);
    }
    for (int i = 0; i < floaterCount; i++) if (floaters[i].used) {
        int gx, gy; mapToCell(floaters[i].x, floaters[i].y, &gx, &gy);
        putCell(gx, gy, floaters[i].text[0] ? floaters[i].text[0] : '+', floaters[i].col);
    }

    buildPanel();

    if (!G.plain) printf("\033[H\033[?25l");
    char hud[256];
    int bossPct = -1; char bossNm[24];
    for (int i = 0; i < enemyCount; i++) if (enemies[i].used && enemies[i].isBoss) {
        bossPct = (int)clampd(enemies[i].hp / enemies[i].maxHp * 100.0, 0.0, 100.0);
        snprintf(bossNm, sizeof(bossNm), "%s", enemies[i].bossName);
        break;
    }
    if (bossPct >= 0) {
        snprintf(hud, sizeof(hud),
                 " HP %3d/%-3d LV%-2d %4d$  关 %2d/%2d  杀 %-4d  BOSS %s %d%%  %s",
                 (int)ceil(pl->hp), (int)pl->maxHp, G.level, G.gold, G.wave, MAX_WAVE,
                 G.kills, bossNm, bossPct, G.autoFire ? "[连发]" : "");
    } else {
        snprintf(hud, sizeof(hud),
                 " HP %3d/%-3d LV%-2d %4d$  关 %2d/%2d  杀 %-4d  剩余 %-2d  %s",
                 (int)ceil(pl->hp), (int)pl->maxHp, G.level, G.gold, G.wave, MAX_WAVE,
                 G.kills, G.waveLeft, G.autoFire ? "[连发]" : "");
    }
    if (G.plain) printf("%s\n", hud);
    else printf("\033[1;96m%s\033[0m\033[K\n", hud);

    for (int y = 0; y < G.rows; y++) {
        if (G.plain) {
            printf("%s  | %s\n", cells[y], panelLine[y]);
        } else {
            int cur = -1;
            for (int x = 0; x < G.cols; x++) {
                int c = colors[y][x];
                if (c != cur) { printf("%s", ANSI[c]); cur = c; }
                putchar(cells[y][x]);
            }
            printf("\033[0m  \033[0;36m|\033[0m %s", panelLine[y]);
            int pad = PANEL_W - dispWidth(panelLine[y]);
            for (int k = 0; k < pad; k++) putchar(' ');
            printf("\033[K\n");
        }
    }
    const char *foot;
    if (G.victory) foot = ">> 通关！30 关 + 最终 BOSS 已击破 (R 重开) <<";
    else if (G.over) foot = ">> GAME OVER (R 重开) <<";
    else if (G.bannerLife > 0) foot = G.banner;
    else if (G.pick3) foot = " >>> 按 1 / 2 / 3 选择一个强化 <<<";
    else foot = " W/A/S/D 移动  J/L 瞄准  空格 开火  C 冲刺  Tab 面板  X 退出";
    if (G.plain) printf("%s\n", foot);
    else printf("\033[%s;93m  >> %s <<\033[0m\033[K\n",
                G.victory || G.over ? "1" : "0", foot);
    if (!G.plain) { printf("\033[K"); fflush(stdout); }
}

/* ===== 键盘（非阻塞；Windows 走控制台输入事件，POSIX 走 raw 模式 + select） ===== */
static int rawOn = 0;
static double holdTime[256];
static int edge[256];
#ifdef _WIN32
static int held[256];          /* Windows 能收到 key-up，直接用真实按下状态 */
#else
static struct termios origTerm;
#endif

static void applyKey(int ch);
static void applyArrow(char d);

static int keyDown(char c) {
    unsigned char u = (unsigned char)c;
#ifdef _WIN32
    return held[u] != 0;
#else
    return (now_ms() - holdTime[u]) < 320.0;
#endif
}
static void restoreTerm(void) {
#ifdef _WIN32
    if (rawOn) { SetConsoleMode(g_hIn, g_origInMode); rawOn = 0; }
#else
    if (rawOn) {
        tcsetattr(STDIN_FILENO, TCSANOW, &origTerm);
        printf("\033[?25h\033[0m\n");
        fflush(stdout);
        rawOn = 0;
    }
#endif
    consoleFini();
}
static void setRaw(void) {
#ifdef _WIN32
    if (!g_consoleIn) return;                  /* stdin 被重定向时不做终端设置 */
    if (!GetConsoleMode(g_hIn, &g_origInMode)) return;
    DWORD m = g_origInMode;
    m &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                  ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT);
    m |= ENABLE_EXTENDED_FLAGS;
    m &= ~(DWORD)ENABLE_QUICK_EDIT_MODE;       /* 避免鼠标框选把控制台冻住 */
    SetConsoleMode(g_hIn, m);
    rawOn = 1;
#else
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &origTerm);
    struct termios t = origTerm;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    rawOn = 1;
#endif
    atexit(restoreTerm);
}

/* 记录一次按下：只有“刚按下”那一下才算边沿，按住自动重复不会连点 */
static void applyKey(int ch) {
    unsigned char u = (unsigned char)ch;
    double tnow = now_ms();
#ifdef _WIN32
    if (!held[u]) edge[u] = 1;
    held[u] = 1;
#else
    if (tnow - holdTime[u] > 240.0) edge[u] = 1;   /* 终端收不到 key-up，用时间窗过滤重复字节 */
#endif
    holdTime[u] = tnow;
}

/* 方向键：↑↓ 选条目，←→ 在图纸页换部件、其他页切面板 */
static void applyArrow(char d) {
    int mx = tabMax(G.tab);
    if (mx < 1) mx = 1;
    if (d == 'A') {
        G.sel = (G.sel - 1 + mx) % mx;
    } else if (d == 'B') {
        G.sel = (G.sel + 1) % mx;
    } else if (d == 'C') {
        if (G.tab == 3) {
            int k = clampi(G.sel, 0, PART_SLOTS - 1);
            G.blueprint[k] = (G.blueprint[k] + 1) % PART_PER_SLOT;
        } else { G.tab = (G.tab + 1) % 4; G.sel = 0; }
    } else if (d == 'D') {
        if (G.tab == 3) {
            int k = clampi(G.sel, 0, PART_SLOTS - 1);
            G.blueprint[k] = (G.blueprint[k] + PART_PER_SLOT - 1) % PART_PER_SLOT;
        } else { G.tab = (G.tab + 3) % 4; G.sel = 0; }
    }
}

static void handleEdge(int ch) {
    char c = (char)ch;
    if (G.pick3) {
        if (c >= '1' && c <= '3') pickHex(c - '1');
        return;
    }
    if (c == '\t') { G.tab = (G.tab + 1) % 4; G.sel = 0; return; }
    if (c == '\n' || c == '\r') {
        if (G.tab == 0) buy(clampi(G.sel, 0, SHOP_N - 1));
        else if (G.tab == 2) {
            int id = (jetCount > 0) ? clampi(G.sel, 0, jetCount - 1) : -1;
            if (id >= 0 && jets[id].used) {
                G.activeJet = id;
                char buf[40]; snprintf(buf, sizeof(buf), "%s 出战", jets[id].code);
                showBanner(buf, C_CYAN);
            }
        } else if (G.tab == 3) {
            buildJet(JET_BUILD_COST);
        }
        return;
    }
    if (c == ' ') {
        G.autoFire = !G.autoFire;
        if (!G.autoFire) holdTime[(unsigned char)' '] = 0.0;   /* 关闭时立即停火 */
        return;
    }
    if (c == 'b' || c == 'B') { if (G.tab == 3) buildJet(JET_BUILD_COST); return; }
    if (c == 'g' || c == 'G') { if (G.tab == 3) randomJet(JET_RANDOM_COST); return; }
    if (c == 'f' || c == 'F') {
        if (G.tab == 2 && jetCount > 1) {          /* 少于 2 架时 % jetCount 会除零 */
            int a = clampi(G.sel, 0, jetCount - 1);
            int b = (a + 1) % jetCount;
            int ni = fuseJets(a, b);
            if (ni >= 0) G.sel = ni;
        }
        return;
    }
    if (c >= '1' && c <= '9') {
        int j = c - '1';
        if (j < MAX_JETS && jets[j].used) {
            G.activeJet = j;
            char buf[40]; snprintf(buf, sizeof(buf), "%s 出战", jets[j].code);
            showBanner(buf, C_CYAN);
        }
        return;
    }
    if (c == 'r' || c == 'R') { startGame(); return; }
    if (c == 'x' || c == 'X') { G.quit = 1; return; }
    if (c == 'c' || c == 'C') {
        if (!G.running || pl->dashCd > 0) return;
        double dx = 0, dy = 0;
        if (keyDown('w')) dy -= 1;
        if (keyDown('s')) dy += 1;
        if (keyDown('a')) dx -= 1;
        if (keyDown('d')) dx += 1;
        if (dx == 0 && dy == 0) { dx = cos(pl->aimAngle); dy = sin(pl->aimAngle); }
        double l = hypot(dx, dy);
        pl->dashDx = dx / l; pl->dashDy = dy / l;
        pl->dashTime = 0.16;
        pl->dashCd = fmax(0.30, 0.90 * stats().dashMul);
        pl->invuln = 0.28;
        return;
    }
    /* J / L 在 update() 中做持续旋转，这里不重复处理 */
}

#ifdef _WIN32
/* 把控制台输入队列里的事件一次消费完（非阻塞） */
static void pumpInputWin(void) {
    DWORD pending = 0;
    if (g_hIn == INVALID_HANDLE_VALUE) return;
    while (GetNumberOfConsoleInputEvents(g_hIn, &pending) && pending > 0) {
        INPUT_RECORD rec;
        DWORD got = 0;
        if (!ReadConsoleInputW(g_hIn, &rec, 1, &got) || got == 0) break;
        if (rec.EventType != KEY_EVENT) continue;
        KEY_EVENT_RECORD *k = &rec.Event.KeyEvent;
        int vk  = (int)k->wVirtualKeyCode;
        int uni = (int)k->uChar.UnicodeChar;
        if (k->bKeyDown) {
            if (vk == VK_UP)    { applyArrow('A'); continue; }
            if (vk == VK_DOWN)  { applyArrow('B'); continue; }
            if (vk == VK_RIGHT) { applyArrow('C'); continue; }
            if (vk == VK_LEFT)  { applyArrow('D'); continue; }
            if (uni == 3) { G.quit = 1; continue; }        /* Ctrl+C：走正常退出流程 */
            if (uni <= 0 || uni > 127) continue;           /* 忽略输入法与功能键 */
            int key = (uni >= 'A' && uni <= 'Z') ? uni + 32 : uni;
            int rep = k->wRepeatCount ? (int)k->wRepeatCount : 1;
            for (int r = 0; r < rep; r++) applyKey(key);
        } else if (uni > 0 && uni < 128) {
            held[(uni >= 'A' && uni <= 'Z') ? uni + 32 : uni] = 0;
        }
    }
}
#endif

static void readInput(void) {
#ifdef _WIN32
    pumpInputWin();
#else
    unsigned char buf[128];
    static unsigned char pend[3];
    static int plen = 0;
    static double escDeadline = 0.0;
    double tnow = now_ms();
    fd_set fds; struct timeval tv = {0, 0};
    FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        for (ssize_t i = 0; i < n; i++) {
            unsigned char ch = buf[i];
            if (plen > 0) {                        /* 正在解析 ESC [ X */
                if (plen < 3) pend[plen] = ch;
                escDeadline = tnow + 60.0;
                if (++plen >= 3) {
                    if (pend[1] == '[') applyArrow((char)pend[2]);
                    plen = 0;
                }
                continue;
            }
            if (ch == 0x1b) { plen = 1; escDeadline = tnow + 60.0; continue; }
            applyKey(ch);
        }
    }
    /* 单独按一下 Esc：超时丢弃，不再把后面两次按键当成转义序列吞掉 */
    if (plen > 0 && tnow > escDeadline) plen = 0;
#endif
    for (int i = 0; i < 256; i++) if (edge[i]) { edge[i] = 0; handleEdge(i); }
    G.firing = G.autoFire || keyDown(' ');
}

/* ===== 无头自测 =====
   注意：下面的自动瞄准 / 免伤 / 自动购买都只在 --selftest 内运行，
   是测试驱动，不是游戏功能。交互模式里这些逻辑完全不存在。 */
static void selftest(int frames) {
    G.selftest = 1;
    startGame();
    G.firing = 1;
    int boughtCount = 0, built = 0, fused = 0, randomed = 0;
    int reachedFinal = 0, victory = 0;
    /* 记录关卡推进轨迹，用来验证「一关一关闯」而不是跳关 */
    int walk[MAX_WAVE + 8], walkN = 0, jumped = 0;
    walk[walkN++] = G.wave;
    for (int f = 0; f < frames; f++) {
        /* 测试驱动：对准最近目标（含 BOSS），锁定血量，模拟走位 */
        int ti = findTargetForTest();
        if (ti >= 0) pl->aimAngle = atan2(enemies[ti].y - pl->y, enemies[ti].x - pl->x);
        pl->hp = pl->maxHp;
        holdTime[(unsigned char)'w'] = (f % 60 < 30) ? now_ms() : 0;
        holdTime[(unsigned char)'d'] = (f % 80 < 40) ? now_ms() : 0;

        update(1.0 / 30.0);

        if (G.wave != walk[walkN - 1]) {
            if (G.wave != walk[walkN - 1] + 1) jumped = 1;
            if (walkN < MAX_WAVE + 8) walk[walkN++] = G.wave;
        }
        if (G.finalPhase) reachedFinal = 1;
        if (G.victory) { victory = 1; break; }

        if (f % 30 == 29) {
            /* 优先堆输出类装备 */
            static const int order[] = {S_DMG, S_RATE, S_MULTI, S_BOSS, S_CRIT,
                                        S_LINK, S_HP, S_ARMOR, S_RANGE, S_GREED};
            int n = (int)(sizeof(order) / sizeof(order[0]));
            for (int k = 0; k < n; k++) {
                int id = order[k];
                if (G.bought[id] < SHOP_DEFS[id].maxLv && G.gold >= shopPrice(id)) {
                    int before = G.bought[id]; buy(id);
                    if (G.bought[id] > before) boughtCount++;
                    break;
                }
            }
        }
        if (f % 150 == 149) {
            if (G.gold >= JET_FUSE_COST && jetCount > 2 && jetCount < MAX_JETS) {
                int a = rand() % jetCount, b = rand() % jetCount;
                if (a != b && jets[a].used && jets[b].used && fuseJets(a, b) >= 0) fused++;
            } else if (G.gold >= JET_BUILD_COST && jetCount < MAX_JETS) {
                if (buildJet(JET_BUILD_COST) >= 0) built++;
            }
            if (G.gold >= JET_RANDOM_COST && jetCount < MAX_JETS) {
                if (randomJet(JET_RANDOM_COST) >= 0) randomed++;
            }
            /* 测试驱动：选出输出加成最高的机体 */
            int best = 0; double bestv = -1e9;
            for (int j = 0; j < MAX_JETS; j++) {
                if (!jets[j].used) continue;
                double jd = 0, jh = 0, js = 0, jr = 0;
                jetBonus(j, &jd, &jh, &js, &jr);
                double v = jd * 2.0 + jr * 3.0 + js * 0.3;
                if (v > bestv) { bestv = v; best = j; }
            }
            G.activeJet = best;
        }
    }
    Stats s = stats();
    int kinds = 0;
    for (int id = 0; id < SHOP_N; id++) if (G.bought[id] > 0) kinds++;
    printf("=== SELFTEST v2 · %d frames ===\n", frames);
    printf("关卡 %d/%d   最终阶段=%d   通关=%d   BOSS击杀=%d\n",
           G.wave, MAX_WAVE, reachedFinal, victory, G.bossKills);
    printf("关卡推进轨迹(%d 步, 逐关+1=%s): ", walkN, jumped ? "否【存在跳关】" : "是");
    for (int i = 0; i < walkN; i++) printf("%d%s", walk[i], i + 1 < walkN ? "→" : "");
    printf("%s\n", reachedFinal ? "→最终BOSS" : "");
    printf("LV=%d 击杀=%d 得分=%d 金币=%d 射击=%d 命中=%d\n",
           G.level, G.kills, G.score, G.gold, G.shots, G.hits);
    printf("装备购买=%d 已购种类=%d/%d 伤害=%.1f 弹数=%d 间隔=%.0fms BOSS加成=%.0f%%\n",
           boughtCount, kinds, SHOP_N, s.damage, s.bulletCount,
           s.interval * 1000, (s.bossMul - 1) * 100);
    printf("最终BOSS设定HP=%.0f  自动辅助=已全部移除（本自测的自动瞄准仅为测试驱动）\n",
           FINAL_BOSS_HP);

    /* 机库批量搭建验证：测试驱动注资，不参与关卡经济，专门验证「能搭许多款」 */
    G.gold += 100000;
    if (buildJet(0) >= 0) built++;
    while (jetCount < MAX_JETS - 4) {
        if (randomJet(0) < 0) break;
        randomed++;
    }
    for (int n = 0; n < 60 && jetCount < MAX_JETS; n++) {
        int a = rand() % jetCount, b = rand() % jetCount;
        if (a != b && jets[a].used && jets[b].used && fuseJets(a, b) >= 0) fused++;
    }
    printf("机库批量搭建: %d/%d 架  图纸发现 %d/%d  组装=%d 随机=%d 融合=%d\n",
           jetCount, MAX_JETS, comboCount(), JET_COMBOS, built, randomed, fused);
    printf("机型风格: US(飞翼隐身) / CN(鸭翼三角) / MIX(融合)，全为虚构代号，非真实型号\n");
}

/* ===== 机库图纸实验室：验证「通过组合 / 融合能搭出许多款飞机」 ===== */
static void jetlab(void) {
    int sc[3] = {0, 0, 0};
    double dmin = 1e9, dmax = -1e9, hmin = 1e9, hmax = -1e9;
    double smin = 1e9, smax = -1e9, rmin = 1e9, rmax = -1e9;
    for (int a = 0; a < PART_PER_SLOT; a++) {
        for (int b = 0; b < PART_PER_SLOT; b++) {
            for (int c = 0; c < PART_PER_SLOT; c++) {
                for (int d = 0; d < PART_PER_SLOT; d++) {
                    int p[PART_SLOTS] = {a, b, c, d};
                    double dm = 0, hp = 0, sp = 0, rt = 0;
                    for (int k = 0; k < PART_SLOTS; k++) {
                        const Part *q = &PARTS[k][p[k]];
                        dm += q->dmg; hp += q->hp; sp += q->spd; rt += q->rate;
                    }
                    sc[styleOfParts(p)]++;
                    if (dm < dmin) dmin = dm;
                    if (dm > dmax) dmax = dm;
                    if (hp < hmin) hmin = hp;
                    if (hp > hmax) hmax = hp;
                    if (sp < smin) smin = sp;
                    if (sp > smax) smax = sp;
                    if (rt < rmin) rmin = rt;
                    if (rt > rmax) rmax = rt;
                }
            }
        }
    }
    printf("=== JET LAB · 虚构机型图纸实验室 ===\n");
    printf("部件槽: 机身 / 机翼 / 引擎 / 武器，每槽 %d 款 → 基础组合 %d 种\n",
           PART_PER_SLOT, JET_COMBOS);
    printf("风格分布: US(飞翼隐身) %d 种 / CN(鸭翼三角) %d 种 / MIX(融合) %d 种\n",
           sc[0], sc[1], sc[2]);
    printf("属性跨度: 伤害 +%.0f%%~+%.0f%%  生命 +%.0f%%~+%.0f%%  "
           "移速 +%.0f%%~+%.0f%%  射速 +%.0f%%~+%.0f%%\n",
           dmin*100, dmax*100, hmin*100, hmax*100,
           smin*100, smax*100, rmin*100, rmax*100);

    unsigned char seen[32];
    memset(seen, 0, sizeof(seen));
    int covered = 0;
    for (int n = 0; n < 4000; n++) {
        int p[PART_SLOTS];
        for (int k = 0; k < PART_SLOTS; k++) {
            p[k] = rand() % PART_PER_SLOT;
            if (randf(0, 1) < 0.25) p[k] = rand() % PART_PER_SLOT;   /* 融合变异 */
        }
        int v = comboIndex(p);
        if (!(seen[v >> 3] & (1u << (v & 7)))) {
            seen[v >> 3] |= (unsigned char)(1u << (v & 7));
            covered++;
        }
    }
    printf("两机融合（随机取件 + 25%% 变异）抽样 4000 次，覆盖 %d/%d 种图纸\n",
           covered, JET_COMBOS);
    printf("机库一次可同时保存 %d 架，可反复组装 / 随机搭建 / 融合 / 重建\n", MAX_JETS);

    printf("\n样例外形（ASCII 俯视，虚构代号，风格化，不对应真实型号）:\n");
    initJets();
    const int samples[6][PART_SLOTS] = {
        {0, 0, 0, 0}, {1, 1, 1, 1}, {2, 2, 2, 2},
        {0, 1, 2, 3}, {3, 2, 1, 0}, {2, 0, 3, 1},
    };
    for (int i = 0; i < 6; i++) {
        int idx = addJet(); if (idx < 0) break;
        char code[12]; snprintf(code, sizeof(code), "LB-%02d", i + 1);
        jetSet(idx, code, "试验机", styleOfParts(samples[i]),
               samples[i][0], samples[i][1], samples[i][2], samples[i][3]);
        char art[3][24];
        jetArt(idx, art);
        printf("\n  %s  [%s] %s\n", code, STYLE_NAME[jets[idx].style],
               STYLE_FULL[jets[idx].style]);
        for (int k = 0; k < 3; k++) printf("     %s\n", art[k]);
    }
    printf("\n注: US / CN 仅为外形气质标签，全部机型为虚构原型，"
           "不含真实型号与真实性能参数。\n");
}

/* ===== main ===== */
int main(int argc, char **argv) {
    consoleInit();
#ifdef _WIN32
    srand((unsigned)time(NULL) ^ (unsigned)GetCurrentProcessId());
#else
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
#endif
    int frames = 30000;
    int doSelftest = 0, doJetlab = 0, runFrames = 0;
    int gWaveArg = 0, gPic3 = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) {
            doSelftest = 1;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') frames = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            runFrames = atoi(argv[++i]);          /* 自动化用：交互模式跑固定帧数后退出 */
        } else if (!strcmp(argv[i], "--wave") && i + 1 < argc) {
            gWaveArg = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--pic3")) {
            gPic3 = 1;
        } else if (!strcmp(argv[i], "--snapshot")) {
            G.snapshot = 1; G.plain = 1;
        } else if (!strcmp(argv[i], "--jetlab")) {
            doJetlab = 1;
        } else if (!strcmp(argv[i], "--help")) {
            printf("用法: %s [--selftest N] [--snapshot] [--jetlab] [--frames N] [--help]\n", argText(argv[0]));
            printf("默认进入交互模式：从第 1 关开始逐关推进，共 30 关，无选关 / 跳关入口。\n");
            printf("--snapshot 输出第 1 关的一帧纯文本画面；--jetlab 枚举飞机图纸；\n");
            printf("--selftest 为无头逻辑自测（测试驱动，不是游戏内辅助）。\n");
            printf("--frames 为自动化验证：交互模式跑 N 帧后自动退出。\n");
            return 0;
        }
    }
    if (frames <= 0) frames = 30000;
    if (doJetlab) { jetlab(); return 0; }

    /* 预留：左侧地图 G.cols 列 + "  | " 4 列 + 面板最宽 36 列，再多留 1 列余量。
       原来只扣了 PANEL_W + 4，而面板实际最宽 36 列，窄终端上整行会折行，
       表现为价格末尾（如 "10$" 的 $）被折到下一行再被覆盖，显示不全 */
    int tw = 100, th = 30;
    if (!termSize(&tw, &th)) { tw = 100; th = 30; }
    G.cols = clampi(tw - (PANEL_W + 7), MIN_COLS, MAX_COLS);
    G.rows = clampi(th - 3, MIN_ROWS, MAX_ROWS);

    if (G.snapshot) {
        startGame();
        G.firing = 1;
        if (gWaveArg > 0) {
            G.wave = clampi(gWaveArg, 1, MAX_WAVE);
            for (int i = 0; i < enemyCount; i++) enemies[i].used = 0;
            startWave();
        }
        for (int f = 0; f < 150; f++) {
            /* 快照也让准星朝上，便于看到 BOSS 与弹幕 */
            pl->aimAngle = -PI / 2;
            update(1.0 / 30.0);
        }
        if (gPic3) offerHex();
        render();
        return 0;
    }
    if (doSelftest) {
        selftest(frames);
        return 0;
    }

    setRaw();
    G.plain = ansiOK() ? 0 : 1;
    startGame();
    double last = now_ms();
    long frame = 0;
    while (!G.quit) {
        double now = now_ms();
        double dt = (now - last) / 1000.0; last = now;
        if (dt > 0.05) dt = 0.05;
        int w2 = 0, h2 = 0;
        if (termSize(&w2, &h2)) {          /* 拖动窗口后跟着重排，避免折行 / 错位 */
            G.cols = clampi(w2 - (PANEL_W + 7), MIN_COLS, MAX_COLS);
            G.rows = clampi(h2 - 3, MIN_ROWS, MAX_ROWS);
        }
        readInput();
        update(dt);
        render();
        double used = now_ms() - now;
        sleep_ms(33.0 - used);
        frame++;
        if (runFrames > 0 && frame >= runFrames) break;
    }
    restoreTerm();
    printf("已退出。最终: 关卡 %d/%d / 等级 %d / 击杀 %d / BOSS %d / 金币 %d / 机库 %d 架\n",
           G.wave, MAX_WAVE, G.level, G.kills, G.bossKills, G.gold, jetCount);
    return 0;
}
