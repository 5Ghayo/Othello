#ifndef OTHELLO_BOARD_H
#define OTHELLO_BOARD_H

#include <cstdint>
#include <vector>
#include <string>

constexpr int BOARD_SIZE = 8;

enum Cell : uint8_t {
    CELL_EMPTY = 0,
    CELL_BLACK = 1,
    CELL_WHITE = 2
};

inline Cell opposite(Cell c) {
    return (c == CELL_BLACK) ? CELL_WHITE : (c == CELL_WHITE) ? CELL_BLACK : CELL_EMPTY;
}

struct Point {
    int row, col;
    bool operator==(const Point& o) const { return row == o.row && col == o.col; }
};

// Records a single move for undo purposes
struct MoveRecord {
    Point pos;
    Cell color;
    std::vector<Point> flipped;
    int prevBlackCount;
    int prevWhiteCount;
    bool wasPass; // true = this record represents a pass (no actual move)
};

class Board {
public:
    Board();
    Board(const Board& other); // deep copy for AI search

    void reset();

    // Queries
    Cell at(int row, int col) const;
    int count(Cell color) const;
    bool isInBounds(int row, int col) const;

    // Valid moves
    bool isValidMove(int row, int col, Cell color) const;
    bool hasValidMoves(Cell color) const;
    std::vector<Point> getValidMoves(Cell color) const;

    // Move execution
    MoveRecord applyMove(int row, int col, Cell color);
    MoveRecord makePass(Cell color);
    void undoMove(const MoveRecord& record);
    void undoLastMove();

    // Game state
    bool isGameOver() const;
    Cell getWinner() const;
    int getTotalPieces() const;

    // AI evaluation helpers
    int countStable(Cell color) const;

    // Debug
    std::string toString() const;

    int getMoveCount() const { return (int)history.size(); }

private:
    Cell grid[BOARD_SIZE][BOARD_SIZE];
    int blackCount;
    int whiteCount;
    std::vector<MoveRecord> history;

    // Direction scanning helpers
    bool checkDirection(int row, int col, Cell color, int dr, int dc) const;
    std::vector<Point> getFlipped(int row, int col, Cell color) const;

    // 64-bit emptyNeighbors optimization
    void updateEmptyNeighbors(int row, int col);
    uint64_t emptyNeighbors;

    inline void setEmptyBit(int row, int col) {
        if (isInBounds(row, col) && grid[row][col] == CELL_EMPTY)
            emptyNeighbors |= (1ULL << (row * 8 + col));
    }
    inline void clearEmptyBit(int row, int col) {
        emptyNeighbors &= ~(1ULL << (row * 8 + col));
    }
    inline bool testEmptyBit(int row, int col) const {
        return (emptyNeighbors >> (row * 8 + col)) & 1;
    }

    // Cached valid moves (recomputed only when board changes)
    void computeValidMoves(Cell color) const;
    mutable bool movesCached[3];
    mutable std::vector<Point> cachedMoves[3];
};

// Standard Othello position weights (from ECE-Othello reference)
extern const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Stable disc detection
bool isStableDisc(const Board& board, int row, int col, Cell color);
void floodFillStable(const Board& board, bool stable[BOARD_SIZE][BOARD_SIZE], Cell color);

#endif // OTHELLO_BOARD_H
