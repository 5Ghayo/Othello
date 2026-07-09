#ifndef OTHELLO_AI_H
#define OTHELLO_AI_H

#include "board.h"
#include <chrono>

// Three AI difficulty levels
enum class AIDifficulty {
    EASY,     // Random valid move + corner priority
    MEDIUM,   // Minimax with Alpha-Beta pruning, depth 4
    HARD      // Alpha-Beta + iterative deepening (depth 2-10) + full evaluation
};

class AI {
public:
    AI(AIDifficulty difficulty = AIDifficulty::MEDIUM);

    // Set difficulty level
    void setDifficulty(AIDifficulty d) { difficulty = d; }
    AIDifficulty getDifficulty() const { return difficulty; }

    // Find the best move for the given color on the given board
    // Returns {-1, -1} if no valid moves
    Point findBestMove(const Board& board, Cell color);

    // Get the name of the current AI
    std::string getName() const;

    // Evaluate a board position from the perspective of 'color'
    // Higher = better for color
    static int evaluate(const Board& board, Cell color);

private:
    AIDifficulty difficulty;

    // Easy AI: choose a random valid move with corner preference
    Point easyMove(const Board& board, Cell color);

    // Medium AI: Alpha-Beta with limited depth
    Point mediumMove(const Board& board, Cell color);

    // Hard AI: iterative deepening Alpha-Beta with timeout
    Point hardMove(const Board& board, Cell color);

    // Minimax with Alpha-Beta pruning
    // Returns evaluation score from maximizing player's perspective
    int alphaBeta(const Board& board, int depth, int alpha, int beta,
                  bool maximizing, Cell aiColor, Cell turnColor,
                  std::chrono::steady_clock::time_point deadline);

    // Evaluation components
    static int evalPieceCount(const Board& board, Cell color);
    static int evalPositionWeights(const Board& board, Cell color);
    static int evalMobility(const Board& board, Cell color);
    static int evalStability(const Board& board, Cell color);

    // Move ordering (heuristic): sort moves so better ones are searched first
    void orderMoves(std::vector<Point>& moves, const Board& board, Cell color);
};

#endif // OTHELLO_AI_H
