#ifndef OTHELLO_THEMES_H
#define OTHELLO_THEMES_H

#include <raylib.h>
#include <string>

struct GameTheme {
    std::string name;
    // Board
    Color boardBg;
    Color boardLine;
    Color boardBorder;
    // UI panels
    Color panelBg;
    Color menuBg;
    // Game pieces
    Color blackPiece;
    Color blackBorder;
    Color blackHighlight;
    Color whitePiece;
    Color whiteBorder;
    Color whiteHighlight;
    // Hints & interaction
    Color hintDot;
    Color hoverPrev;
    // Text
    Color textMain;
    Color textAccent;
    Color textDim;
    // Background brightness (for splash image)
    int bgAlpha;
    // Whether to use texture tiles for the board
    bool useBoardTexture;
    // Whether pieces use textures
    bool usePieceTexture;
};

// Classic green board (default)
inline GameTheme themeClassicGreen() {
    return {
        "经典绿",
        {0, 100, 50, 255},      // boardBg
        {0, 80, 40, 255},       // boardLine
        {0, 60, 30, 255},       // boardBorder
        {30, 30, 40, 200},      // panelBg
        {20, 25, 35, 255},      // menuBg
        {30, 30, 30, 255},      // blackPiece
        {10, 10, 10, 255},      // blackBorder
        {60, 60, 60, 60},       // blackHighlight
        {230, 230, 230, 255},   // whitePiece
        {200, 200, 200, 255},   // whiteBorder
        {255, 255, 255, 80},    // whiteHighlight
        {200, 200, 200, 80},    // hintDot
        {200, 200, 200, 40},    // hoverPrev
        {255, 255, 255, 255},   // textMain
        {255, 255, 0, 255},     // textAccent
        {128, 128, 128, 255},   // textDim
        25,                     // bgAlpha
        false,                  // useBoardTexture
        false                   // usePieceTexture
    };
}

// Wooden board theme
inline GameTheme themeWood() {
    return {
        "木质",
        {101, 67, 33, 255},     // boardBg
        {80, 50, 20, 255},      // boardLine
        {60, 35, 15, 255},      // boardBorder
        {40, 30, 25, 200},      // panelBg
        {25, 20, 15, 255},      // menuBg
        {25, 25, 25, 255},      // blackPiece
        {10, 10, 10, 255},      // blackBorder
        {50, 50, 50, 60},       // blackHighlight
        {245, 240, 230, 255},   // whitePiece (warm white)
        {200, 190, 170, 255},   // whiteBorder
        {255, 250, 240, 80},    // whiteHighlight
        {180, 160, 130, 80},    // hintDot
        {180, 160, 130, 40},    // hoverPrev
        {255, 245, 230, 255},   // textMain
        {255, 215, 0, 255},     // textAccent
        {160, 140, 110, 255},   // textDim
        30,                     // bgAlpha
        true,                   // useBoardTexture
        false                   // usePieceTexture
    };
}

// Dark modern theme
inline GameTheme themeDark() {
    return {
        "暗夜",
        {20, 40, 60, 255},      // boardBg (deep blue)
        {40, 60, 80, 255},      // boardLine
        {15, 30, 50, 255},      // boardBorder
        {15, 18, 25, 200},      // panelBg
        {10, 12, 18, 255},      // menuBg
        {10, 10, 15, 255},      // blackPiece (near black)
        {5, 5, 10, 255},        // blackBorder
        {40, 40, 50, 60},       // blackHighlight
        {200, 210, 230, 255},   // whitePiece (slightly blue white)
        {160, 170, 200, 255},   // whiteBorder
        {220, 230, 255, 80},    // whiteHighlight
        {100, 140, 200, 80},    // hintDot (blue glow)
        {100, 140, 200, 40},    // hoverPrev
        {200, 215, 235, 255},   // textMain
        {100, 180, 255, 255},   // textAccent (cyan)
        {80, 100, 130, 255},    // textDim
        15,                     // bgAlpha
        false,                  // useBoardTexture
        true                    // usePieceTexture (blue glow pieces)
    };
}

inline const char** getThemeNames() {
    static const char* names[] = {"经典绿", "木质", "暗夜"};
    return names;
}

inline constexpr int getThemeCount() { return 3; }

inline GameTheme getTheme(int index) {
    switch (index) {
        case 0: return themeClassicGreen();
        case 1: return themeWood();
        case 2: return themeDark();
        default: return themeClassicGreen();
    }
}

#endif
