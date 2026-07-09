#include "game.h"
#include <cassert>

Game::Game() : mode(GameMode::MENU), state(GameState::PLAYING),
               currentPlayer(CELL_BLACK), humanColor(CELL_BLACK),
               aiThinking(false), undoCount(0), localColor(CELL_BLACK) {
}

void Game::startPvP() {
    board.reset();
    mode = GameMode::PVP;
    state = GameState::PLAYING;
    currentPlayer = CELL_BLACK;
    humanColor = CELL_BLACK;
    aiThinking = false;
    undoCount = 0;
    replay.startRecording("玩家1", "玩家2");
    setStatus("黑方回合 - 点击棋盘落子");
}

void Game::startPvAI(Cell playerColor, AIDifficulty aiDifficulty) {
    board.reset();
    mode = GameMode::PVAI;
    state = GameState::PLAYING;
    currentPlayer = CELL_BLACK;
    humanColor = playerColor;
    aiThinking = false;
    undoCount = 0;
    ai.setDifficulty(aiDifficulty);
    replay.startRecording("玩家", ai.getName());

    if (currentPlayer != humanColor) {
        setStatus("AI 思考中...");
        triggerAIMove();
    } else {
        setStatus("你的回合 - 点击棋盘落子");
    }
}

void Game::startNetwork(Cell localColor) {
    board.reset();
    mode = GameMode::NETWORK;
    state = GameState::PLAYING;
    currentPlayer = CELL_BLACK;
    humanColor = localColor;
    this->localColor = localColor;
    aiThinking = false;
    undoCount = 0;
    setStatus("网络对战模式 - 等待对手...");
}

bool Game::playMove(int row, int col) {
    if (state != GameState::PLAYING) return false;
    if (!board.isValidMove(row, col, currentPlayer)) return false;

    board.applyMove(row, col, currentPlayer);
    replay.recordMove(row, col, currentPlayer);
    undoCount = 0; // Reset undo counter on successful move

    // Generate board coordinates string (e.g., "E4")
    char coord[3] = {(char)('A' + col), (char)('1' + row), 0};
    std::string playerName = (currentPlayer == CELL_BLACK) ? "黑方" : "白方";
    setStatus(playerName + std::string("落子 ") + coord);

    switchTurn();
    return true;
}

void Game::passTurn() {
    if (state != GameState::PLAYING) return;

    // Only allow pass when no valid moves
    if (board.hasValidMoves(currentPlayer)) return;

    std::string playerName = (currentPlayer == CELL_BLACK) ? "黑方" : "白方";
    setStatus(playerName + " 无合法位置，跳过回合");

    board.makePass(currentPlayer);
    switchTurn();
}

bool Game::undoMove() {
    if (state != GameState::PLAYING) return false;
    if (board.getMoveCount() == 0) return false;
    if (undoCount >= MAX_UNDO) {
        setStatus("已达到最大悔棋次数 (" + std::to_string(MAX_UNDO) + ")");
        return false;
    }

    // In PvAI mode, undo both AI's move and player's move
    // (AI always responds to a player move, even if it's just a pass)
    if (mode == GameMode::PVAI) {
        int toUndo = std::min(2, board.getMoveCount());
        for (int i = 0; i < toUndo; i++)
            board.undoLastMove();
        currentPlayer = humanColor;
    } else {
        // PvP or Network mode: undo just the last move
        board.undoLastMove();
        currentPlayer = opposite(currentPlayer);
    }

    undoCount++;
    setStatus("悔棋 (剩余 " + std::to_string(MAX_UNDO - undoCount) + " 次)");
    return true;
}

void Game::resign() {
    if (state != GameState::PLAYING) return;
    Cell winner = opposite(currentPlayer);
    std::string winnerName = (winner == CELL_BLACK) ? "黑方" : "白方";
    state = GameState::GAME_OVER;
    setStatus(winnerName + " 获胜！(" + std::to_string(board.count(winner))
              + " - " + std::to_string(board.count(opposite(winner))) + ")");
}

void Game::switchTurn() {
    checkGameOver();
    if (state == GameState::GAME_OVER) return;

    currentPlayer = opposite(currentPlayer);

    // Check if current player has valid moves
    if (!board.hasValidMoves(currentPlayer)) {
        // Check if the other player also has no moves (game over)
        if (!board.hasValidMoves(opposite(currentPlayer))) {
            checkGameOver();
            return;
        }
        // Current player must pass
        std::string name = (currentPlayer == CELL_BLACK) ? "黑方" : "白方";
        setStatus(name + " 无合法位置，自动跳过");
        board.makePass(currentPlayer);
        currentPlayer = opposite(currentPlayer);
    }

    // Trigger AI if it's AI's turn
    if (mode == GameMode::PVAI && currentPlayer != humanColor) {
        triggerAIMove();
    } else {
        std::string name = (currentPlayer == CELL_BLACK) ? "黑方" : "白方";
        std::string turnType = (mode == GameMode::PVAI && currentPlayer == humanColor)
                             ? "你的回合" : name + " 回合";
        setStatus(turnType + " - 点击棋盘落子");
    }
}

void Game::checkGameOver() {
    if (board.isGameOver()) {
        state = GameState::GAME_OVER;
        replay.stopRecording(board.getWinner(), board.count(CELL_BLACK), board.count(CELL_WHITE));
        Cell winner = board.getWinner();
        if (winner == CELL_BLACK) {
            setStatus("黑方获胜！ " + std::to_string(board.count(CELL_BLACK))
                      + " - " + std::to_string(board.count(CELL_WHITE)));
        } else if (winner == CELL_WHITE) {
            setStatus("白方获胜！ " + std::to_string(board.count(CELL_WHITE))
                      + " - " + std::to_string(board.count(CELL_BLACK)));
        } else {
            setStatus("平局！ " + std::to_string(board.count(CELL_BLACK))
                      + " - " + std::to_string(board.count(CELL_WHITE)));
        }
    }
}

void Game::setStatus(const std::string& msg) {
    statusMessage = msg;
}

void Game::update() {
    // Check if AI is still thinking
    if (aiThinking) {
        // AI move is done synchronously in current implementation
        // This will be updated when we add async AI
    }
}

void Game::handleClick(int row, int col) {
    if (state != GameState::PLAYING) return;
    if (mode == GameMode::MENU) return;

    // In PvAI mode, only accept clicks on human's turn
    if (mode == GameMode::PVAI && currentPlayer != humanColor) {
        setStatus("AI 思考中，请稍候...");
        return;
    }

    playMove(row, col);
}

void Game::triggerAIMove() {
    // Handle pass for AI
    if (!board.hasValidMoves(currentPlayer)) {
        std::string name = (currentPlayer == CELL_BLACK) ? "黑方" : "白方";
        setStatus(name + " 无合法位置，跳过");
        board.makePass(currentPlayer);
        currentPlayer = opposite(currentPlayer);
        if (!board.hasValidMoves(currentPlayer)) {
            checkGameOver();
            return;
        }
        if (currentPlayer == humanColor) {
            setStatus("你的回合");
            return;
        }
    }

    aiThinking = true;
    setStatus("AI 思考中...");

    Point move = ai.findBestMove(board, currentPlayer);

    if (move.row >= 0 && move.col >= 0) {
        char coord[3] = {(char)('A' + move.col), (char)('1' + move.row), 0};
        std::string name = (currentPlayer == CELL_BLACK) ? "黑方(AI)" : "白方(AI)";
        board.applyMove(move.row, move.col, currentPlayer);
        replay.recordMove(move.row, move.col, currentPlayer);
        setStatus(name + " 落子 " + coord);
    }

    aiThinking = false;
    undoCount = 0;

    // After AI move, check game state
    checkGameOver();
    if (state == GameState::GAME_OVER) return;

    // Switch to human's turn
    currentPlayer = opposite(currentPlayer);

    // Check if human has valid moves
    if (!board.hasValidMoves(currentPlayer)) {
        if (!board.hasValidMoves(opposite(currentPlayer))) {
            checkGameOver();
            return;
        }
        setStatus("你无合法位置，自动跳过");
        board.makePass(currentPlayer);
        currentPlayer = opposite(currentPlayer);
        triggerAIMove(); // AI continues
        return;
    }

    setStatus("你的回合 - 点击棋盘落子");
}
