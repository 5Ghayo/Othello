#ifndef OTHELLO_GAME_H
#define OTHELLO_GAME_H

#include "board.h"
#include "ai.h"
#include "replay.h"

enum class GameMode {
    MENU,
    PVAI,
    PVP,
    NETWORK,
};

enum class GameState {
    PLAYING,
    GAME_OVER,
};

class Game {
public:
    Game();
    ~Game() = default;

    // Start a new game
    void startPvP();
    void startPvAI(Cell playerColor, AIDifficulty aiDifficulty);
    void startNetwork(Cell localColor);

    // Core game loop functions
    void update();
    void handleClick(int row, int col);

    // Game actions
    bool playMove(int row, int col);
    void passTurn();
    bool undoMove();
    void resign();

    // Getters
    GameMode getMode() const { return mode; }
    GameState getState() const { return state; }
    const Board& getBoard() const { return board; }
    Cell getCurrentPlayer() const { return currentPlayer; }
    Cell getHumanColor() const { return humanColor; }
    AIDifficulty getAIDifficulty() const { return ai.getDifficulty(); }
    ReplaySystem& getReplay() { return replay; }
    bool isAIThinking() const { return aiThinking; }
    const std::string& getStatusMessage() const { return statusMessage; }

    // Score display
    int getBlackScore() const { return board.count(CELL_BLACK); }
    int getWhiteScore() const { return board.count(CELL_WHITE); }
    int getMoveCount() const { return board.getMoveCount(); }

    // Undo tracker
    int getUndoCount() const { return undoCount; }
    static constexpr int MAX_UNDO = 5;

    // Reset to menu
    void resetToMenu() { mode = GameMode::MENU; aiThinking = false; }

private:
    Board board;
    AI ai;
    GameMode mode;
    GameState state;
    Cell currentPlayer;
    Cell humanColor;    // Human's color in PvAI mode
    bool aiThinking;
    int undoCount;
    std::string statusMessage;

    // For network mode (placeholder)
    Cell localColor;
    ReplaySystem replay;

    // Internal helpers
    void switchTurn();
    void checkGameOver();
    void setStatus(const std::string& msg);
    void triggerAIMove();
};

#endif // OTHELLO_GAME_H
