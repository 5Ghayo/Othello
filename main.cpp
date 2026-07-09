#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define NOGDI
#endif
#include <raylib.h>
#undef BLACK
#undef WHITE
#undef GRAY
#undef LIGHTGRAY
#undef DARKGRAY
#undef YELLOW
#undef GOLD
#undef BLUE
#undef GREEN
#undef RED
#undef MAROON
#ifdef _WIN32
    #undef CloseWindow
    #undef ShowCursor
#endif

#include "board.h"
#include "ai.h"
#include "game.h"
#include "themes.h"
#include "replay.h"
#include "network.h"

#include <string>
#include <vector>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <ifaddrs.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
#include <deque>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef _WIN32
    #undef DrawTextEx
    #undef LoadImage
#endif

// ======== Constants ========
constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 800;
constexpr char WINDOW_TITLE[] = "黑白棋 Othello";

constexpr int BOARD_X = 80;
constexpr int BOARD_Y = 90;
constexpr int CELL_SIZE = 72;
constexpr int BOARD_PX = CELL_SIZE * BOARD_SIZE;
// raylib color literals (macros were undefined above)
#define RAYWHITE_ Color{255,255,255,255}
#define GOLD_ Color{255,215,0,255}
#define GREEN_ Color{0,200,0,255}
#define YELLOW_ Color{255,255,0,255}

// ======== Globals ========
static Game game;
static Font gameFont;
static Sound sndPlace, sndHover;

static int currentTheme = 0;        // 0=Green, 1=Wood, 2=Dark
static bool showMenu = true;
static int aiDiffSelection = 1;
static int colorSelection = 0;
static std::string menuMessage = "";
static bool exitRequested = false;
static double menuMessageTimer = 0;

// Replay globals
static ReplaySystem replaySys;
static bool showReplayUI = false;
static bool replayPlaying = false;
static Board replayBoard;
static float replayAutoTimer = 0;

// Network globals
static NetworkSession netSession;
static std::string netInputBuffer;
static bool netInputActive = false;
static float netHeartbeatTimer = 0;
static bool showNetSetup = false;
static int netSetupStep = 0; // 0=choose, 1=host(server), 2=client(input ip)
static std::string localIP;
static std::string netSetupError = "";
static double netErrorTimer = 0;

static std::string getLocalIP() {
#ifdef _WIN32
    // Windows: use gethostname + getaddrinfo
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints, *info = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, NULL, &hints, &info) == 0 && info) {
            struct sockaddr_in *addr = (struct sockaddr_in *)info->ai_addr;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            std::string ip(buf);
            freeaddrinfo(info);
            if (ip != "127.0.0.1") return ip;
        }
    }
    return "127.0.0.1";
#else
    struct ifaddrs *ifaddr, *ifa;
    std::string ip = "127.0.0.1";
    if (getifaddrs(&ifaddr) == -1) return ip;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            std::string s(buf);
            if (s != "127.0.0.1") { ip = s; break; }
        }
    }
    freeifaddrs(ifaddr);
    return ip;
#endif
}

// Flip animation
struct FlipAnim { int row, col; float progress; bool fromBlack; bool isPlacement; };
static std::deque<FlipAnim> flipAnims;

static Texture2D texBg;
static Texture2D texTileLight, texTileDark;
static Texture2D texPieceBlack, texPieceWhite;
static bool texturesLoaded = false;
static Board prevBoard;
static int lastMoveCount = 0;

// ======== Coord Helpers ========
inline int c2px(int col) { return BOARD_X + col * CELL_SIZE + CELL_SIZE/2; }
inline int c2py(int row) { return BOARD_Y + row * CELL_SIZE + CELL_SIZE/2; }
inline bool p2c(int px, int py, int& row, int& col) {
    if (px < BOARD_X || px >= BOARD_X + BOARD_PX) return false;
    if (py < BOARD_Y || py >= BOARD_Y + BOARD_PX) return false;
    col = (px - BOARD_X) / CELL_SIZE;
    row = (py - BOARD_Y) / CELL_SIZE;
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

// ======== Drawing ========

static GameTheme getActiveTheme() { return getTheme(currentTheme); }

static void LoadThemeTextures() {
    std::string dir = GetApplicationDirectory();
    Image imgT = LoadImage((dir + "assets/textures/board_tile_light.png").c_str());
    if (imgT.data != nullptr) {
        texTileLight = LoadTextureFromImage(imgT);
        Image imgT2 = LoadImage((dir + "assets/textures/board_tile_dark.png").c_str());
        texTileDark = LoadTextureFromImage(imgT2);
        UnloadImage(imgT); UnloadImage(imgT2);
    }
    Image imgB = LoadImage((dir + "assets/textures/punBlack.png").c_str());
    if (imgB.data != nullptr) {
        texPieceBlack = LoadTextureFromImage(imgB);
        Image imgW = LoadImage((dir + "assets/textures/punWhite.png").c_str());
        texPieceWhite = LoadTextureFromImage(imgW);
        UnloadImage(imgB); UnloadImage(imgW);
    }
    texturesLoaded = texTileLight.id != 0;
}

void DrawBoard(const Board& board) {
    GameTheme t = getActiveTheme();

    // Background
    DrawRectangle(BOARD_X - 10, BOARD_Y - 10, BOARD_PX + 20, BOARD_PX + 20, t.boardBg);
    DrawRectangle(BOARD_X - 6, BOARD_Y - 6, BOARD_PX + 12, BOARD_PX + 12, t.boardBorder);

    // Grid
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int pos = BOARD_X + i * CELL_SIZE;
        DrawLine(pos, BOARD_Y, pos, BOARD_Y + BOARD_PX, t.boardLine);
        pos = BOARD_Y + i * CELL_SIZE;
        DrawLine(BOARD_X, pos, BOARD_X + BOARD_PX, pos, t.boardLine);
    }

    // Textured tiles (when useBoardTexture is enabled)
    if (t.useBoardTexture && texTileLight.id != 0 && texTileDark.id != 0) {
        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                int tx = BOARD_X + c * CELL_SIZE;
                int ty = BOARD_Y + r * CELL_SIZE;
                Texture2D tile = ((r + c) % 2 == 0) ? texTileLight : texTileDark;
                DrawTexturePro(tile, {0,0,(float)tile.width,(float)tile.height},
                               {(float)tx, (float)ty, (float)CELL_SIZE, (float)CELL_SIZE},
                               {0,0}, 0, RAYWHITE_);
            }
        }
    }

    // Labels
    for (int i = 0; i < BOARD_SIZE; i++) {
        char l[2] = {(char)('A' + i), 0};
        DrawTextEx(gameFont, l, {(float)(BOARD_X + i*CELL_SIZE + CELL_SIZE/2 - 10), (float)(BOARD_Y - 36)}, 24, 0, t.textMain);
        char n[3]; snprintf(n, sizeof(n), "%d", i+1);
        DrawTextEx(gameFont, n, {(float)(BOARD_X - 40), (float)(BOARD_Y + i*CELL_SIZE + CELL_SIZE/2 - 12)}, 24, 0, t.textMain);
    }

    // Pieces
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            Cell cell = board.at(r, c);
            if (cell == CELL_EMPTY) continue;

            int cx = c2px(c), cy = c2py(r);
            bool isBlack = (cell == CELL_BLACK);

            if (t.usePieceTexture && texPieceBlack.id != 0 && texPieceWhite.id != 0) {
                // Draw from texture
                Texture2D tex = isBlack ? texPieceBlack : texPieceWhite;
                float scale = (float)CELL_SIZE / (float)tex.width;
                float pw = tex.width * scale;
                float ph = tex.height * scale;
                DrawTexturePro(tex, {0,0,(float)tex.width,(float)tex.height},
                               {(float)cx - pw/2, (float)cy - ph/2, pw, ph},
                               {0,0}, 0, RAYWHITE_);
            } else {
                // Fallback: draw circles
                Color pColor = isBlack ? t.blackPiece : t.whitePiece;
                Color pBorder = isBlack ? t.blackBorder : t.whiteBorder;
                Color pHigh = isBlack ? t.blackHighlight : t.whiteHighlight;

                float radius = (CELL_SIZE / 2) - 4.0f;
                DrawCircle(cx, cy, radius + 2.0f, pBorder);
                DrawCircle(cx, cy, radius, pColor);
                DrawCircle(cx-4, cy-4, radius * 0.33f, pHigh);
            }
        }
    }

    // Animations (flip + placement)
    for (auto& a : flipAnims) {
        int cx = c2px(a.col), cy = c2py(a.row);
        float radius = (CELL_SIZE / 2) - 4.0f;
        float p = a.progress;

        if (a.isPlacement) {
            // Placement: scale up with mild overshoot bounce
            float scale;
            if (p < 0.65f) {
                scale = p / 0.65f;                // grow to 1.0
            } else {
                float t = (p - 0.65f) / 0.35f;    // 0→1 over remaining
                scale = 1.0f + 0.08f * cosf(t * PI * 3.0f) * (1.0f - t); // damped bounce
            }
            float r = radius * fabsf(scale);
            bool isBlack = a.fromBlack;
            Color col = isBlack ? t.blackPiece : t.whitePiece;
            Color border = isBlack ? t.blackBorder : t.whiteBorder;
            Color high = isBlack ? t.blackHighlight : t.whiteHighlight;

            // Shadow
            DrawCircle(cx + 3, cy + 3, r, Color{0,0,0,25});
            // Border
            DrawCircle(cx, cy, r + 2.0f * scale, border);
            // Piece
            DrawCircle(cx, cy, r, col);
            // Highlight
            DrawCircle(cx - 4 * fabsf(scale), cy - 4 * fabsf(scale), r * 0.33f, high);
            // Ring pulse
            if (p < 0.3f) {
                float ringR = radius * (0.5f + p / 0.3f * 1.0f);
                DrawRing({(float)cx, (float)cy}, ringR - 1.5f, ringR, 0, 360,
                         12, Color{255,255,255,(unsigned char)(80 * (1.0f - p / 0.3f))});
            }
        } else {
            // 3D flip: rotate around Y-axis with edge detail and shadow
            float angle = p * PI;
            float sx = cosf(angle);
            float abs_sx = fabsf(sx);

            // Determine colors
            bool showingOld = (sx > 0);
            Color pieceCol = showingOld == a.fromBlack ? t.blackPiece : t.whitePiece;
            Color edgeCol = showingOld == a.fromBlack ? t.blackBorder : t.whiteBorder;
            Color highCol = showingOld == a.fromBlack ? t.blackHighlight : t.whiteHighlight;

            // Shadow (offset, shrinks at edge-on)
            DrawEllipse(cx + 3, cy + 3, radius * abs_sx * 0.9f, radius * 0.9f,
                        Color{0,0,0,25});

            // Slight rise during middle of flip (piece lifts)
            float rise = sinf(angle) * 4.0f;
            int fry = cy - (int)rise;

            // Edge thickness (side of piece, visible when rotated)
            float edgeW = 3.0f * (1.0f - abs_sx) * 0.8f;
            float faceR = radius - edgeW;
            if (faceR < 1.0f) faceR = 1.0f;

            // Draw edge / side
            DrawEllipse(cx, fry, radius * abs_sx, radius, edgeCol);

            // Draw top face
            DrawEllipse(cx, fry, faceR * abs_sx, faceR, pieceCol);

            // Draw highlight
            if (abs_sx > 0.05f) {
                float hlX = -4.0f * (0.6f + 0.4f * abs_sx);
                float hlY = -4.0f;
                float hlR = faceR * 0.25f;
                DrawCircle(cx + hlX, fry + hlY, hlR, highCol);
            }
        }
    }

    // Valid move hints
    Cell cur = showReplayUI ? CELL_EMPTY : game.getCurrentPlayer();
    if (!showReplayUI) {
        auto vm = board.getValidMoves(cur);
        for (auto& m : vm) DrawCircle(c2px(m.col), c2py(m.row), (CELL_SIZE >= 72 ? 8.0f : 6.0f), t.hintDot);
    }

    // Hover preview
    if (!showReplayUI && !game.isAIThinking()) {
        Vector2 ms = GetMousePosition();
        int hr, hc;
        if (p2c((int)ms.x, (int)ms.y, hr, hc) && board.isValidMove(hr, hc, cur)) {
            float prevR = (CELL_SIZE / 2) - 4.0f;
            Color prev = (cur == CELL_BLACK) ? Color{30,30,30,100} : Color{230,230,230,100};
            DrawCircle(c2px(hc), c2py(hr), prevR, prev);
        }
    }
}

void DrawPanel(const Game& g) {
    GameTheme t = getActiveTheme();
    const int px = BOARD_X + BOARD_PX + 40, py = BOARD_Y;
    DrawRectangleRounded({(float)px, (float)py, 220, 440}, 0.1f, 8, t.panelBg);

    DrawTextEx(gameFont, "黑白棋", {(float)(px+15), (float)(py+15)}, 36, 0, t.textMain);

    // Scores
    DrawTextEx(gameFont, "黑方", {(float)(px+15), (float)(py+65)}, 28, 0, t.blackPiece);
    char sb[16]; snprintf(sb, sizeof(sb), "%d", g.getBlackScore());
    DrawTextEx(gameFont, sb, {(float)(px+120), (float)(py+65)}, 28, 0, t.textMain);

    DrawTextEx(gameFont, "白方", {(float)(px+15), (float)(py+100)}, 28, 0, t.textAccent);
    char sw[16]; snprintf(sw, sizeof(sw), "%d", g.getWhiteScore());
    DrawTextEx(gameFont, sw, {(float)(px+120), (float)(py+100)}, 28, 0, t.textMain);

    // Turn
    Cell turn = showReplayUI ? CELL_EMPTY : g.getCurrentPlayer();
    if (!showReplayUI) {
        DrawCircle(px+20, py+155, 10.0f, (turn == CELL_BLACK) ? t.blackPiece : t.whitePiece);
        DrawTextEx(gameFont, (turn == CELL_BLACK) ? "黑方回合" : "白方回合",
                   {(float)(px+40), (float)(py+146)}, 26, 0, t.textMain);
    }

    // Status
    std::string status = showReplayUI ? replaySys.getStatusText() : g.getStatusMessage();
    DrawTextEx(gameFont, status.c_str(), {(float)(px+15), (float)(py+190)}, 20, 0, t.textAccent);

    // Move count
    char mc[32]; snprintf(mc, sizeof(mc), "步数: %d", g.getMoveCount());
    DrawTextEx(gameFont, mc, {(float)(px+15), (float)(py+225)}, 22, 0, t.textDim);

    // Undo
    char uc[32]; snprintf(uc, sizeof(uc), "悔棋: %d/%d", g.getUndoCount(), Game::MAX_UNDO);
    DrawTextEx(gameFont, uc, {(float)(px+15), (float)(py+250)}, 22, 0, t.textDim);

    // Mode
    const char* md = "人人对战";
    if (g.getMode() == GameMode::PVAI) md = "人机对战";
    else if (g.getMode() == GameMode::NETWORK) md = "网络对战";
    if (showReplayUI) md = "棋谱回放";
    DrawTextEx(gameFont, md, {(float)(px+15), (float)(py+290)}, 20, 0, t.textDim);

    // Theme name
    DrawTextEx(gameFont, ("主题:" + t.name).c_str(),
               {(float)(px+15), (float)(py+330)}, 18, 0, t.textDim);

    // Network info
    if (g.getMode() == GameMode::NETWORK && netSession.isConnected()) {
        DrawTextEx(gameFont, netSession.getConnectionInfo().c_str(),
                   {(float)(px+15), (float)(py+360)}, 18, 0, t.textAccent);
    }
}

void DrawButton(int x, int y, int w, int h, const char* text, bool hover) {
    GameTheme t = getActiveTheme();
    Color bg = hover ? Color{90,90,120,255} : Color{60,60,80,255};
    DrawRectangleRounded({(float)x, (float)y, (float)w, (float)h}, 0.2f, 8, bg);
    Vector2 ts = MeasureTextEx(gameFont, text, 26, 0);
    DrawTextEx(gameFont, text, {(float)(x + (w - ts.x)/2), (float)(y + (h - 28)/2)}, 26, 0, RAYWHITE_);
}

void DrawNetSetupScreen() {
    GameTheme t = getActiveTheme();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, t.menuBg);
    if (texBg.id != 0) DrawTexturePro(texBg, {0,0,(float)texBg.width,(float)texBg.height}, {0,0,(float)SCREEN_W,(float)SCREEN_H}, {0,0}, 0, Color{255,255,255,(unsigned char)t.bgAlpha});

    Vector2 ms = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    const int BW = 280, BH = 50, BX = (SCREEN_W - BW)/2;

    if (netSetupStep == 0) {
        DrawTextEx(gameFont, "网络对战设置", {(float)((SCREEN_W - MeasureTextEx(gameFont, "网络对战设置", 48, 0).x)/2), 80}, 48, 0, t.textMain);

        int by = 200;
        bool h1 = ms.x >= BX && ms.x <= BX+BW && ms.y >= by && ms.y <= by+BH;
        DrawButton(BX, by, BW, BH, "创建房间(主机)", h1);
        if (h1 && pressed) { netSetupStep = 1; localIP = getLocalIP(); netSession.startListening(9876); }

        by += 60;
        bool h2 = ms.x >= BX && ms.x <= BX+BW && ms.y >= by && ms.y <= by+BH;
        DrawButton(BX, by, BW, BH, "加入房间", h2);
        if (h2 && pressed) { netSetupStep = 2; netInputBuffer.clear(); netInputActive = true; }

        by += 60;
        bool h3 = ms.x >= BX && ms.x <= BX+BW && ms.y >= by && ms.y <= by+BH;
        DrawButton(BX, by, BW, BH, "返回", h3);
        if (h3 && pressed) { showNetSetup = false; netSetupStep = 0; netSession.disconnect(); }
    }
    else if (netSetupStep == 1) {
        DrawTextEx(gameFont, "等待对手连接...", {(float)((SCREEN_W - MeasureTextEx(gameFont, "等待对手连接...", 36, 0).x)/2), 120}, 36, 0, t.textAccent);
        DrawTextEx(gameFont, ("本机 IP: " + localIP).c_str(), {(float)((SCREEN_W - MeasureTextEx(gameFont, ("本机 IP: " + localIP).c_str(), 24, 0).x)/2), 200}, 24, 0, t.textMain);
        DrawTextEx(gameFont, "端口: 9876", {(float)((SCREEN_W - MeasureTextEx(gameFont, "端口: 9876", 24, 0).x)/2), 240}, 24, 0, t.textMain);
        DrawTextEx(gameFont, "让对方在\"加入房间\"输入上方 IP", {(float)((SCREEN_W - 500)/2), 290}, 18, 0, t.textDim);

        float pulse = sinf(GetTime() * 3.0f) * 0.3f + 0.7f;
        DrawCircle(SCREEN_W/2, 370, 10.0f, Color{0,255,0,(unsigned char)(255 * pulse)});
        DrawTextEx(gameFont, "等待中...", {(float)((SCREEN_W - MeasureTextEx(gameFont, "等待中...", 20, 0).x)/2), 395}, 20, 0, Color{0,255,0,(unsigned char)(200 * pulse)});

        if (netSession.acceptConnection()) {
            showNetSetup = false;
            showMenu = false;
            game.startNetwork(CELL_BLACK);
        }

        bool hc = ms.x >= SCREEN_W/2-60 && ms.x <= SCREEN_W/2+60 && ms.y >= 440 && ms.y <= 480;
        if (hc) DrawRectangleRounded({(float)(SCREEN_W/2-60), 440, 120, 40}, 0.2f, 6, Color{80,40,40,255});
        DrawTextEx(gameFont, "取消", {(float)(SCREEN_W/2 - 30), 450}, 24, 0, t.textDim);
        if (hc && pressed) { netSession.disconnect(); showNetSetup = false; netSetupStep = 0; }
    }
    else if (netSetupStep == 2) {
        DrawTextEx(gameFont, "连接对手", {(float)((SCREEN_W - MeasureTextEx(gameFont, "连接对手", 36, 0).x)/2), 120}, 36, 0, t.textMain);
        DrawTextEx(gameFont, "输入对方 IP 地址:", {(float)((SCREEN_W - 400)/2), 190}, 24, 0, t.textAccent);

        const int IW = 400, IH = 40, IX = (SCREEN_W - IW)/2, IY = 230;
        DrawRectangle(IX, IY, IW, IH, Color{50,50,70,255});
        DrawRectangleLines(IX, IY, IW, IH, Color{100,100,140,255});
        const char* inputDisplay = netInputBuffer.empty() ? "例: 192.168.1.100" : netInputBuffer.c_str();
        DrawTextEx(gameFont, inputDisplay, {(float)(IX+10), (float)(IY+6)}, 24, 0,
                   netInputBuffer.empty() ? Color{100,100,100,255} : t.textMain);
        if (netInputActive && fmodf(GetTime(), 1.0f) < 0.5f) {
            Vector2 ts = MeasureTextEx(gameFont, netInputBuffer.c_str(), 24, 0);
            DrawRectangle(IX + 10 + (int)ts.x, IY + 8, 2, IH - 16, t.textMain);
        }

        int cby = IY + 60;
        bool canConnect = !netInputBuffer.empty();
        bool hc2 = canConnect && ms.x >= BX && ms.x <= BX+BW && ms.y >= cby && ms.y <= cby+BH;
        if (canConnect) DrawButton(BX, cby, BW, BH, "连接", hc2);
        if (hc2 && pressed) {
            if (netSession.connectToHost(netInputBuffer, 9876)) {
                netInputActive = false;
                showNetSetup = false;
                showMenu = false;
                game.startNetwork(CELL_WHITE);
            } else {
                netSetupError = netSession.getLastError();
                netErrorTimer = GetTime();
                netSession.disconnect();
            }
        }

        int bby = cby + 60;
        bool hb = ms.x >= BX && ms.x <= BX+BW && ms.y >= bby && ms.y <= bby+BH;
        DrawButton(BX, bby, BW, BH, "返回", hb);
        if (hb && pressed) { netSetupStep = 0; netInputActive = false; netSession.disconnect(); }
    }

    if (!netSetupError.empty() && GetTime() - netErrorTimer < 3.0) {
        DrawTextEx(gameFont, netSetupError.c_str(), {(float)((SCREEN_W - 400)/2), 480}, 20, 0, Color{200,50,50,255});
    } else {
        netSetupError.clear();
    }
}

void DrawMenu() {
    GameTheme t = getActiveTheme();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, t.menuBg);
    if (texBg.id != 0) DrawTexturePro(texBg, {0,0,(float)texBg.width,(float)texBg.height}, {0,0,(float)SCREEN_W,(float)SCREEN_H}, {0,0}, 0, Color{255,255,255,(unsigned char)t.bgAlpha});

    DrawTextEx(gameFont, "黑白棋 Othello", {340, 70}, 56, 0, t.textMain);
    DrawTextEx(gameFont, "--- Reversi ---", {470, 135}, 24, 0, t.textDim);

    // Menu items
    const int BW = 240, BH = 50, BX = (SCREEN_W - BW)/2;
    const char* items[] = {"人人对战", "人机对战", "网络对战", "棋谱回放", "退出游戏"};
    int by = 180;
    Vector2 ms = GetMousePosition();
    bool btnP = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    for (int i = 0; i < 5; i++) {
        bool hv = (ms.x >= BX && ms.x <= BX+BW && ms.y >= by && ms.y <= by+BH);
        DrawButton(BX, by, BW, BH, items[i], hv);
        if (hv && btnP) {
            if (i == 0) { game.startPvP(); showMenu = false; }
            else if (i == 1) {
                Cell pc = (colorSelection == 0) ? CELL_BLACK : CELL_WHITE;
                AIDifficulty ad[] = {AIDifficulty::EASY, AIDifficulty::MEDIUM, AIDifficulty::HARD};
                game.startPvAI(pc, ad[aiDiffSelection]);
                showMenu = false;
            }
            else if (i == 2) {
                // Show network setup screen
                netSetupStep = 0;
                showNetSetup = true;
            }
            else if (i == 3) {
                // Load replay
                {
                    const char* path = "record.oth";
                    if (replaySys.loadFromFile(path)) {
                        replaySys.startPlayback();
                        replayBoard = replaySys.getBoardAtStep(0);
                        replayAutoTimer = 0;
                        showReplayUI = true;
                        showMenu = false;
                    } else {
                        menuMessage = "未找到棋谱文件，请先对弈一局并保存";
                        menuMessageTimer = GetTime();
                    }
                }
            }
            else if (i == 4) { exitRequested = true; break; }
        }
        by += 60;
    }

    // Theme selector
    int sx = BX - 80, sy = SCREEN_H - 95;
    DrawTextEx(gameFont, "主题:", {(float)sx, (float)(sy-5)}, 24, 0, t.textAccent);
    for (int i = 0; i < getThemeCount(); i++) {
        bool sel = (i == currentTheme);
        if (sel) DrawRectangle(sx+80+i*100, sy-2, 90, 24, Color{80,80,30,100});
        DrawTextEx(gameFont, getTheme(i).name.c_str(), {(float)(sx+85+i*100), (float)sy}, 22, 0, sel ? GOLD_ : t.textDim);
        if (btnP && ms.x >= sx+80+i*100 && ms.x <= sx+170+i*100 && ms.y >= sy-2 && ms.y <= sy+22)
            currentTheme = i;
    }

    // AI difficulty
    int sy2 = sy + 30;
    DrawTextEx(gameFont, "AI:", {(float)sx, (float)(sy2-5)}, 24, 0, t.textAccent);
    const char* diffs[] = {"简单", "中级", "困难"};
    for (int i = 0; i < 3; i++) {
        bool sel = (i == aiDiffSelection);
        if (sel) DrawRectangle(sx+60+i*75, sy2-2, 65, 24, Color{80,80,30,100});
        DrawTextEx(gameFont, diffs[i], {(float)(sx+65+i*75), (float)sy2}, 22, 0, sel ? GOLD_ : t.textDim);
        if (btnP && ms.x >= sx+60+i*75 && ms.x <= sx+125+i*75 && ms.y >= sy2-2 && ms.y <= sy2+22)
            aiDiffSelection = i;
    }

    // Menu feedback message
    if (!menuMessage.empty() && GetTime() - menuMessageTimer < 3.0) {
        Vector2 msgSz = MeasureTextEx(gameFont, menuMessage.c_str(), 20, 0);
        DrawTextEx(gameFont, menuMessage.c_str(),
                   {(float)((SCREEN_W - msgSz.x)/2), (float)(SCREEN_H - 40)},
                   20, 0, YELLOW_);
    }

    // Color
    int sy3 = sy2 + 30;
    DrawTextEx(gameFont, "子:", {(float)sx, (float)(sy3-5)}, 24, 0, t.textAccent);
    const char* cols[] = {"黑", "白"};
    for (int i = 0; i < 2; i++) {
        bool sel = (i == colorSelection);
        if (sel) DrawRectangle(sx+60+i*75, sy3-2, 50, 24, Color{80,80,30,100});
        DrawTextEx(gameFont, cols[i], {(float)(sx+65+i*75), (float)sy3}, 22, 0, sel ? GOLD_ : t.textDim);
        if (btnP && ms.x >= sx+60+i*75 && ms.x <= sx+110+i*75 && ms.y >= sy3-2 && ms.y <= sy3+22)
            colorSelection = i;
    }
}

// ======== Replay Controls ========
void DrawReplayControls() {
    if (!showReplayUI || !replaySys.isPlaying()) return;

    GameTheme t = getActiveTheme();
    int px = 60, py = SCREEN_H - 80, pw = SCREEN_W - 370;

    DrawRectangleRounded({(float)px, (float)py, (float)pw, 60}, 0.1f, 8, t.panelBg);

    // Step controls
    Vector2 ms = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // << Step backward
    bool h1 = ms.x >= px+10 && ms.x <= px+55 && ms.y >= py+10 && ms.y <= py+45;
    DrawButton(px+10, py+10, 45, 40, "<<", h1);
    if (h1 && pressed) { replaySys.stepBackward(); replayBoard = replaySys.getBoardAtStep(replaySys.getCurrentStep()); }

    // Step backward 1
    bool h2 = ms.x >= px+65 && ms.x <= px+110 && ms.y >= py+10 && ms.y <= py+45;
    DrawButton(px+65, py+10, 45, 40, "<", h2);
    if (h2 && pressed) { replaySys.stepBackward(); replayBoard = replaySys.getBoardAtStep(replaySys.getCurrentStep()); }

    // Auto play toggle
    bool h3 = ms.x >= px+120 && ms.x <= px+200 && ms.y >= py+10 && ms.y <= py+45;
    DrawButton(px+120, py+10, 80, 40, replayAutoTimer > 0 ? "▶" : "▐▐", h3);
    if (h3 && pressed) replayAutoTimer = (replayAutoTimer > 0) ? 0 : 1.0f;

    // Step forward 1
    bool h4 = ms.x >= px+210 && ms.x <= px+255 && ms.y >= py+10 && ms.y <= py+45;
    DrawButton(px+210, py+10, 45, 40, ">", h4);
    if (h4 && pressed) { replaySys.stepForward(); replayBoard = replaySys.getBoardAtStep(replaySys.getCurrentStep()); }

    // >> Step forward (to end)
    bool h5 = ms.x >= px+265 && ms.x <= px+310 && ms.y >= py+10 && ms.y <= py+45;
    DrawButton(px+265, py+10, 45, 40, ">>", h5);
    if (h5 && pressed) { replaySys.setStep(replaySys.getTotalSteps()); replayBoard = replaySys.getBoardAtStep(replaySys.getCurrentStep()); }

    // Step indicator
    char stepText[32];
    snprintf(stepText, sizeof(stepText), "%d / %d", replaySys.getCurrentStep(), replaySys.getTotalSteps());
    DrawTextEx(gameFont, stepText, {(float)(px+330), (float)(py+20)}, 24, 0, t.textMain);
}

// ======== Game Over Popup ========
void DrawGameOverPopup(const Game& g) {
    GameTheme t = getActiveTheme();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Color{0,0,0,150});

    int pw = 400, ph = 280, px = (SCREEN_W-pw)/2, py = (SCREEN_H-ph)/2;
    DrawRectangleRounded({(float)px, (float)py, (float)pw, (float)ph}, 0.1f, 10, Color{30,35,50,255});

    Cell w = g.getBoard().getWinner();
    const char* rt; Color rc;
    if (w == CELL_BLACK) { rt = "黑方获胜！"; rc = t.blackPiece; }
    else if (w == CELL_WHITE) { rt = "白方获胜！"; rc = t.whitePiece; }
    else { rt = "平局！"; rc = t.textDim; }

    DrawTextEx(gameFont, rt, {(float)(px+120), (float)(py+30)}, 42, 0, rc);

    char sc[30]; snprintf(sc, sizeof(sc), "黑 %d : %d 白", g.getBlackScore(), g.getWhiteScore());
    DrawTextEx(gameFont, sc, {(float)(px+120), (float)(py+80)}, 30, 0, t.textMain);
    DrawTextEx(gameFont, g.getStatusMessage().c_str(), {(float)(px+50), (float)(py+120)}, 20, 0, t.textAccent);

    Vector2 ms = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // Save replay button
    bool h0 = ms.x >= px+130 && ms.x <= px+270 && ms.y >= py+155 && ms.y <= py+180;
    DrawRectangleRounded({(float)(px+130), (float)(py+155), 140, 25}, 0.2f, 6, Color{50,70,50,255});
    DrawTextEx(gameFont, "保存棋谱", {(float)(px+160), (float)(py+157)}, 22, 0, t.textAccent);
    if (h0 && pressed) {
        // Copy game's recorded data to the playback replay system and save
        replaySys.setData(game.getReplay().getData());
        if (replaySys.saveToFile("record.oth"))
            DrawTextEx(gameFont, "已保存 record.oth", {(float)(px+130), (float)(py+200)}, 20, 0, GREEN_);
    }

    int by = py + 195;
    bool h1 = (ms.x >= px+30 && ms.x <= px+180 && ms.y >= by && ms.y <= by+45);
    DrawButton(px+30, by, 150, 45, "返回菜单", h1);
    if (h1 && pressed) { game.resetToMenu(); showMenu = true; showReplayUI = false; }

    bool h2 = (ms.x >= px+220 && ms.x <= px+370 && ms.y >= by && ms.y <= by+45);
    DrawButton(px+220, by, 150, 45, "再来一局", h2);
    if (h2 && pressed) {
        if (g.getMode() == GameMode::PVAI) {
            game.startPvAI(g.getHumanColor(), g.getAIDifficulty());
        } else if (g.getMode() == GameMode::PVP) {
            game.startPvP();
        }
    }
}

void DrawHints(float y) {
    DrawTextEx(gameFont, "Z=悔棋  R=认输  ESC=菜单",
               {60, (float)(SCREEN_H - 34)}, 18, 0, Color{150,150,150,100});
}

// ======== Animations ========
void UpdateAnims(float dt) {
    for (auto& a : flipAnims) {
        float dur = a.isPlacement ? 0.2f : 0.35f;
        a.progress += dt / dur;
    }
    while (!flipAnims.empty() && flipAnims.front().progress >= 1.0f)
        flipAnims.pop_front();
}

// ======== Network Handling ========
void HandleNetwork() {
    // Check for disconnection during active game
    if (game.getMode() == GameMode::NETWORK && netSession.isConnected()) {
        // Quick check: verify socket is alive
        NetMessage dummy;
        if (!netSession.recvMessage(dummy, 1) && netSession.isConnected() == false) {
            game.resetToMenu();
            showMenu = true;
            return;
        }
    }
    if (game.getMode() != GameMode::NETWORK || !netSession.isConnected()) return;

    NetMessage msg;
    if (netSession.recvMessage(msg, 50)) {
        switch (msg.type) {
            case NetMessage::MSG_MOVE:
                if (game.getState() == GameState::PLAYING)
                    game.playMove(msg.row, msg.col);
                break;
            case NetMessage::MSG_UNDO_REQUEST:
                game.undoMove();
                netSession.sendUndoResponse(true);
                break;
            case NetMessage::MSG_UNDO_ACCEPT:
                game.undoMove();
                break;
            case NetMessage::MSG_RESIGN:
                game.resign();
                break;
            case NetMessage::MSG_HEARTBEAT:
                // Respond to ping
                break;
            default:
                break;
        }
    }

    // Send heartbeat every 3s
    netHeartbeatTimer += GetFrameTime();
    if (netHeartbeatTimer > 3.0f) {
        netSession.sendHeartbeat();
        netHeartbeatTimer = 0;
    }
}

// ======== Main ========
int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, WINDOW_TITLE);
    SetTargetFPS(60);
    InitAudioDevice();
    SetExitKey(0); // Handle ESC ourselves

    // Load font with CJK support
    std::string fp = GetApplicationDirectory();
    fp += "assets/fonts/NotoSansSC-Regular.otf";

    // Load only the specific glyphs we need (to keep the font atlas manageable)
    std::vector<int> cps;
    for (int i = 32; i <= 126; i++) cps.push_back(i);
    // Specific CJK characters used in the UI
    int neededChars[] = {19968,19978,20013,20027,20154,20214,20301,20313,20320,20363,20445,20505,20808,20837,20856,20877,20986,20987,21019,21040,21097,21152,21160,21333,21462,21475,21512,22238,22256,22312,22320,22336,22812,22823,23376,23384,23458,23478,23545,23616,24050,24179,24182,24314,24328,24335,24453,24605,24724,25103,25112,25143,25151,25163,25214,25509,25773,25918,25968,25991,26041,26080,26263,26368,26408,26410,26412,26426,26469,26827,27169,27425,27493,27861,28040,28216,28857,29609,30333,30340,30424,31245,31471,31561,31616,32423,32463,32476,32511,32593,32622,32771,32988,33258,33719,33756,33853,35748,35753,35774,35831,35889,36136,36339,36755,36798,36807,36820,36830,36864,38388,38590,39064,39640,40657};
    for (int cp : neededChars) cps.push_back(cp);
    // Common symbols
    for (int i = 0x3000; i <= 0x303F; i++) cps.push_back(i); // CJK Symbols
    for (int i = 0xFF00; i <= 0xFFEF; i++) cps.push_back(i); // Fullwidth forms

    gameFont = LoadFontEx(fp.c_str(), 48, cps.data(), (int)cps.size());
    if (gameFont.texture.id == 0) {
        // Fallback: check alternate path
        fp = "assets/fonts/NotoSansSC-Regular.otf";
        gameFont = LoadFontEx(fp.c_str(), 48, cps.data(), (int)cps.size());
        if (gameFont.texture.id == 0) {
            TraceLog(LOG_WARNING, "Font not found at any path, using default");
            gameFont = GetFontDefault();
        }
    }

    // Load sounds
    std::string sd = GetApplicationDirectory();
    sd += "assets/sounds/";
    sndPlace = LoadSound((sd + "click.wav").c_str());
    sndHover = LoadSound((sd + "hover.wav").c_str());

    // Load background
    std::string ip = GetApplicationDirectory();
    ip += "assets/textures/bg_splash.jpg";
    Image bgImg = LoadImage(ip.c_str());
    if (bgImg.data != nullptr) { texBg = LoadTextureFromImage(bgImg); UnloadImage(bgImg); }

    // Load theme textures
    LoadThemeTextures();

    // Load icon
    std::string iconPath = GetApplicationDirectory();
    iconPath += "reference/ece-othello/images/icon.ico";
    Image icn = LoadImage(iconPath.c_str());
    if (icn.data != nullptr) { SetWindowIcon(icn); UnloadImage(icn); }

    bool gameOverShown = false;

    while (!WindowShouldClose() && !exitRequested) {
        // Input
        // Handle text input for network IP
        if (netInputActive) {
            int c = GetCharPressed();
            while (c > 0) {
                if (netInputBuffer.size() < 20 && ((c >= '0' && c <= '9') || c == '.')) {
                    netInputBuffer += (char)c;
                }
                c = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !netInputBuffer.empty()) {
                netInputBuffer.pop_back();
            }
        }

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (showNetSetup) {
                if (netSetupStep == 0) {
                    netSession.disconnect(); showNetSetup = false; netSetupStep = 0;
                } else {
                    netSession.disconnect(); netSetupStep = 0; netInputActive = false;
                }
                continue;
            }
            if (showMenu) { exitRequested = true; break; }
            if (showReplayUI) { showReplayUI = false; showMenu = true; continue; }
            game.resetToMenu();
            showMenu = true;
        }

        if (!showMenu && !showReplayUI && game.getState() == GameState::PLAYING) {
            if (IsKeyPressed(KEY_Z)) game.undoMove();
            if (IsKeyPressed(KEY_R)) game.resign();
        }

        if (!showMenu && !showReplayUI) {
            Vector2 ms = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                int r, c;
                if (p2c((int)ms.x, (int)ms.y, r, c)) {
                    int oldCnt = game.getMoveCount();
                    game.handleClick(r, c);
                    if (game.getMoveCount() > oldCnt && game.getMode() == GameMode::NETWORK) {
                        netSession.sendMove(r, c);
                    }
                }
            }
        }

        // Update
        game.update();
        UpdateAnims(GetFrameTime());

        // Detect board changes → trigger flip/placement animations
        if (!showMenu && !showReplayUI) {
            const Board& curBoard = game.getBoard();
            int curCount = curBoard.getMoveCount();
            if (curCount > lastMoveCount) {
                for (int r = 0; r < BOARD_SIZE; r++) {
                    for (int c = 0; c < BOARD_SIZE; c++) {
                        Cell p = prevBoard.at(r, c);
                        Cell q = curBoard.at(r, c);
                        if (p == q) continue;
                        if (p == CELL_EMPTY && q != CELL_EMPTY) {
                            // Newly placed piece
                            flipAnims.push_back({r, c, 0.0f, q == CELL_BLACK, true});
                        } else if (p != CELL_EMPTY && q != CELL_EMPTY && p != q) {
                            // Flipped piece
                            flipAnims.push_back({r, c, 0.0f, p == CELL_BLACK, false});
                        }
                    }
                }
                lastMoveCount = curCount;
            } else if (curCount < lastMoveCount) {
                // Game was reset (new game / undo to 0)
                lastMoveCount = curCount;
            }
            prevBoard = curBoard;
        }

        // Handle network
        HandleNetwork();

        // Handle replay auto-play
        if (replayAutoTimer > 0 && showReplayUI && replaySys.isPlaying()) {
            replayAutoTimer -= GetFrameTime();
            if (replayAutoTimer <= 0) {
                replaySys.stepForward();
                replayBoard = replaySys.getBoardAtStep(replaySys.getCurrentStep());
                if (replaySys.getCurrentStep() >= replaySys.getTotalSteps())
                    replayAutoTimer = 0;
                else
                    replayAutoTimer = 0.5f;
            }
        }

        if (!showReplayUI && game.getState() == GameState::GAME_OVER && !gameOverShown) {
            gameOverShown = true;
        }
        if (game.getState() == GameState::PLAYING) gameOverShown = false;

        // Render
        BeginDrawing();
        GameTheme t = getActiveTheme();
        ClearBackground(t.menuBg);

        if (showNetSetup) {
            DrawNetSetupScreen();
        } else if (showMenu) {
            DrawMenu();
        } else {
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, t.menuBg);
            if (texBg.id != 0) DrawTexturePro(texBg, {0,0,(float)texBg.width,(float)texBg.height}, {0,0,(float)SCREEN_W,(float)SCREEN_H}, {0,0}, 0, Color{255,255,255,(unsigned char)t.bgAlpha});

            const Board& board = showReplayUI ? replayBoard : game.getBoard();
            DrawBoard(board);
            DrawPanel(game);

            if (showReplayUI) {
                DrawReplayControls();
            } else {
                DrawHints(SCREEN_H - 30);

                if (game.getState() == GameState::GAME_OVER)
                    DrawGameOverPopup(game);

                if (game.isAIThinking()) {
                    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Color{0,0,0,60});
                    Vector2 ats = MeasureTextEx(gameFont, "AI 思考中...", 30, 0);
                    DrawTextEx(gameFont, "AI 思考中...", {(float)(SCREEN_W/2 - ats.x/2), (float)(SCREEN_H/2 - ats.y/2)}, 36, 0, YELLOW_);
                }
            }
        }

        DrawFPS(SCREEN_W - 80, 10);
        EndDrawing();
    }

    // Cleanup
    if (texBg.id != 0) UnloadTexture(texBg);
    if (texTileLight.id != 0) { UnloadTexture(texTileLight); UnloadTexture(texTileDark); }
    if (texPieceBlack.id != 0) { UnloadTexture(texPieceBlack); UnloadTexture(texPieceWhite); }
    UnloadFont(gameFont);
    UnloadSound(sndPlace);
    UnloadSound(sndHover);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
