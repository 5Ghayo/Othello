#include "ai.h"
#include <algorithm>
#include <random>
#include <climits>
#include <cstdlib>
#include <thread>

AI::AI(AIDifficulty d) : difficulty(d) {}

std::string AI::getName() const {
    switch (difficulty) {
        case AIDifficulty::EASY:   return "简单 AI";
        case AIDifficulty::MEDIUM: return "中级 AI";
        case AIDifficulty::HARD:   return "高级 AI";
    }
    return "AI";
}

// ========== EASY: Random + Corner Preference ==========

Point AI::easyMove(const Board& board, Cell color) {
    auto moves = board.getValidMoves(color);
    if (moves.empty()) return {-1, -1};

    // Check for corner moves first
    std::vector<Point> corners;
    for (const auto& m : moves)
        if ((m.row == 0 || m.row == 7) && (m.col == 0 || m.col == 7))
            corners.push_back(m);
    if (!corners.empty()) {
        std::mt19937 rng((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<size_t> dist(0, corners.size() - 1);
        return corners[dist(rng)];
    }

    // Otherwise random
    std::mt19937 rng((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    return moves[dist(rng)];
}

// ========== Evaluation Functions ==========

int AI::evalPieceCount(const Board& board, Cell color) {
    Cell opp = opposite(color);
    int mine = board.count(color);
    int theirs = board.count(opp);
    // In endgame (close to full board), weight piece count heavily
    int total = mine + theirs;
    if (total > 50) {
        return (mine - theirs) * 50;
    }
    return mine - theirs;
}

int AI::evalPositionWeights(const Board& board, Cell color) {
    Cell opp = opposite(color);
    int score = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board.at(r, c) == color) score += POSITION_WEIGHTS[r][c];
            else if (board.at(r, c) == opp) score -= POSITION_WEIGHTS[r][c];
        }
    return score;
}

int AI::evalMobility(const Board& board, Cell color) {
    Cell opp = opposite(color);
    int myMoves = (int)board.getValidMoves(color).size();
    int oppMoves = (int)board.getValidMoves(opp).size();
    if (myMoves + oppMoves == 0) return 0;
    return 15 * (myMoves - oppMoves);
}

int AI::evalStability(const Board& board, Cell color) {
    Cell opp = opposite(color);
    return 25 * (board.countStable(color) - board.countStable(opp));
}

int AI::evaluate(const Board& board, Cell color) {
    int score = 0;
    score += evalPositionWeights(board, color);
    score += evalMobility(board, color);

    // Add endgame piece evaluation
    int total = board.getTotalPieces();
    if (total > 48) {
        score += evalPieceCount(board, color);
    }

    // Stability (expensive, only use on higher difficulties selectively)
    if (total > 40) {
        score += evalStability(board, color);
    }

    return score;
}

// ========== Move Ordering ==========

void AI::orderMoves(std::vector<Point>& moves, const Board& board, Cell color) {
    // Simple heuristic: sort corners first, then edges, then by weight
    std::sort(moves.begin(), moves.end(),
        [&board, color](const Point& a, const Point& b) {
            auto weightFn = [](const Point& p) {
                if ((p.row == 0 || p.row == 7) && (p.col == 0 || p.col == 7)) return 1000;
                if (p.row == 0 || p.row == 7 || p.col == 0 || p.col == 7) return 100;
                return POSITION_WEIGHTS[p.row][p.col];
            };
            return weightFn(a) > weightFn(b);
        });
}

// ========== Alpha-Beta Core ==========

int AI::alphaBeta(const Board& board, int depth, int alpha, int beta,
                  bool maximizing, Cell aiColor, Cell turnColor,
                  std::chrono::steady_clock::time_point deadline) {
    // Check timeout
    if (std::chrono::steady_clock::now() > deadline)
        return evaluate(board, aiColor);

    // Terminal: game over
    if (board.isGameOver()) {
        Cell winner = board.getWinner();
        if (winner == aiColor) return 100000 + (64 - board.getTotalPieces()); // faster win = better
        if (winner == opposite(aiColor)) return -100000 - (64 - board.getTotalPieces());
        return 0; // draw
    }

    // Terminal: depth reached
    if (depth == 0)
        return evaluate(board, aiColor);

    // Check if current player has valid moves
    auto moves = board.getValidMoves(turnColor);
    if (moves.empty()) {
        // Must pass
        return alphaBeta(board, depth - 1, alpha, beta, !maximizing, aiColor, opposite(turnColor), deadline);
    }

    // Order moves for better pruning
    orderMoves(moves, board, turnColor);

    if (maximizing) {
        int maxEval = INT_MIN;
        for (const auto& m : moves) {
            Board copy(board);
            copy.applyMove(m.row, m.col, turnColor);
            int eval = alphaBeta(copy, depth - 1, alpha, beta, false, aiColor, opposite(turnColor), deadline);
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break; // Beta cutoff
        }
        return maxEval;
    } else {
        int minEval = INT_MAX;
        for (const auto& m : moves) {
            Board copy(board);
            copy.applyMove(m.row, m.col, turnColor);
            int eval = alphaBeta(copy, depth - 1, alpha, beta, true, aiColor, opposite(turnColor), deadline);
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) break; // Alpha cutoff
        }
        return minEval;
    }
}

// ========== MEDIUM: Fixed depth Alpha-Beta ==========

Point AI::mediumMove(const Board& board, Cell color) {
    auto moves = board.getValidMoves(color);
    if (moves.empty()) return {-1, -1};

    int depth = 4;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);

    Point bestMove = moves[0];
    int bestScore = INT_MIN;

    for (const auto& m : moves) {
        Board copy(board);
        copy.applyMove(m.row, m.col, color);
        int score = alphaBeta(copy, depth - 1, INT_MIN, INT_MAX, false, color, opposite(color), deadline);
        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
    }

    return bestMove;
}

// ========== HARD: Iterative Deepening Alpha-Beta ==========

Point AI::hardMove(const Board& board, Cell color) {
    auto moves = board.getValidMoves(color);
    if (moves.empty()) return {-1, -1};

    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::milliseconds(1500);

    Point bestMove = moves[0];
    orderMoves(moves, board, color);

    // Iterative deepening: start with depth 2, increase until timeout
    for (int depth = 2; depth <= 64; depth++) {
        if (std::chrono::steady_clock::now() > deadline) break;

        bool completed = true;
        int bestScore = INT_MIN;

        for (const auto& m : moves) {
            if (std::chrono::steady_clock::now() > deadline) {
                completed = false;
                break;
            }
            Board copy(board);
            copy.applyMove(m.row, m.col, color);
            int score = alphaBeta(copy, depth - 1, INT_MIN, INT_MAX, false, color, opposite(color), deadline);
            if (score > bestScore) {
                bestScore = score;
                bestMove = m;
            }
        }
        // If we completed this depth, re-sort moves by the latest scores
        if (completed && depth > 2) {
            // Deeper search completed - this is our new best
        }
    }

    return bestMove;
}

// ========== Public Interface ==========

Point AI::findBestMove(const Board& board, Cell color) {
    switch (difficulty) {
        case AIDifficulty::EASY:   return easyMove(board, color);
        case AIDifficulty::MEDIUM: return mediumMove(board, color);
        case AIDifficulty::HARD:   return hardMove(board, color);
    }
    return mediumMove(board, color);
}
