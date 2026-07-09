#include "board.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cassert>

// Standard Othello position weight table
// Higher values = more desirable positions
// Corners (100) are most valuable, C-squares (-50) are traps
const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE] = {
    { 100, -50,  20,   5,   5,  20, -50, 100 },
    { -50, -70,  -4,   1,   1,  -4, -70, -50 },
    {  20,  -4,   2,   2,   2,   2,  -4,  20 },
    {   5,   1,   2,  -3,  -3,   2,   1,   5 },
    {   5,   1,   2,  -3,  -3,   2,   1,   5 },
    {  20,  -4,   2,   2,   2,   2,  -4,  20 },
    { -50, -70,  -4,   1,   1,  -4, -70, -50 },
    { 100, -50,  20,   5,   5,  20, -50, 100 }
};

// 8 directions: N, NE, E, SE, S, SW, W, NW
static const int DR[8] = {-1, -1,  0,  1, 1, 1, 0, -1};
static const int DC[8] = { 0,  1,  1,  1, 0,-1,-1, -1};

Board::Board() : blackCount(0), whiteCount(0), emptyNeighbors(0) {
    for (int i = 0; i < 3; i++) movesCached[i] = false;
    reset();
}

Board::Board(const Board& other) {
    std::memcpy(grid, other.grid, sizeof(grid));
    blackCount = other.blackCount;
    whiteCount = other.whiteCount;
    emptyNeighbors = other.emptyNeighbors;
    history = other.history;
    for (int i = 0; i < 3; i++) {
        movesCached[i] = other.movesCached[i];
        cachedMoves[i] = other.cachedMoves[i];
    }
}

void Board::reset() {
    std::memset(grid, 0, sizeof(grid));
    blackCount = 2;
    whiteCount = 2;
    history.clear();
    for (int i = 0; i < 3; i++) movesCached[i] = false;

    // Standard 4-center starting position
    grid[3][3] = CELL_BLACK;  grid[3][4] = CELL_WHITE;
    grid[4][3] = CELL_WHITE;  grid[4][4] = CELL_BLACK;

    // Initialize empty neighbors
    emptyNeighbors = 0;
    for (int r = 2; r <= 5; r++)
        for (int c = 2; c <= 5; c++)
            if (grid[r][c] == CELL_EMPTY)
                emptyNeighbors |= (1ULL << (r * 8 + c));
}

Cell Board::at(int row, int col) const {
    if (!isInBounds(row, col)) return CELL_EMPTY;
    return grid[row][col];
}

int Board::count(Cell color) const {
    return (color == CELL_BLACK) ? blackCount : whiteCount;
}

bool Board::isInBounds(int row, int col) const {
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

bool Board::checkDirection(int row, int col, Cell color, int dr, int dc) const {
    Cell opp = opposite(color);
    int r = row + dr, c = col + dc;
    if (!isInBounds(r, c)) return false;
    if (grid[r][c] != opp) return false; // adjacent must be opponent
    // Scan further
    r += dr; c += dc;
    while (isInBounds(r, c)) {
        if (grid[r][c] == color) return true;  // found friendly piece - valid direction
        if (grid[r][c] == CELL_EMPTY) return false;  // empty hole - invalid
        r += dr; c += dc;
    }
    return false; // hit edge without finding friendly
}

bool Board::isValidMove(int row, int col, Cell color) const {
    if (!isInBounds(row, col)) return false;
    if (grid[row][col] != CELL_EMPTY) return false;
    for (int d = 0; d < 8; d++)
        if (checkDirection(row, col, color, DR[d], DC[d]))
            return true;
    return false;
}

std::vector<Point> Board::getFlipped(int row, int col, Cell color) const {
    std::vector<Point> result;
    Cell opp = opposite(color);
    for (int d = 0; d < 8; d++) {
        if (!checkDirection(row, col, color, DR[d], DC[d])) continue;
        // Collect all opponent pieces in this direction until we hit friendly
        int r = row + DR[d], c = col + DC[d];
        while (isInBounds(r, c) && grid[r][c] == opp) {
            result.push_back({r, c});
            r += DR[d]; c += DC[d];
        }
    }
    return result;
}

void Board::computeValidMoves(Cell color) const {
    int idx = (int)color;
    if (idx < 0 || idx > 2) return;
    if (movesCached[idx]) return;
    cachedMoves[idx].clear();

    // Optimization: only check empty neighbors
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (grid[r][c] == CELL_EMPTY && testEmptyBit(r, c)) {
                if (isValidMove(r, c, color)) {
                    cachedMoves[idx].push_back({r, c});
                }
            }
        }
    }
    movesCached[idx] = true;
}

bool Board::hasValidMoves(Cell color) const {
    computeValidMoves(color);
    return !cachedMoves[(int)color].empty();
}

std::vector<Point> Board::getValidMoves(Cell color) const {
    computeValidMoves(color);
    return cachedMoves[(int)color];
}

MoveRecord Board::applyMove(int row, int col, Cell color) {
    assert(isInBounds(row, col));
    assert(grid[row][col] == CELL_EMPTY);
    assert(color == CELL_BLACK || color == CELL_WHITE);

    MoveRecord record;
    record.pos = {row, col};
    record.color = color;
    record.flipped = getFlipped(row, col, color);
    record.prevBlackCount = blackCount;
    record.prevWhiteCount = whiteCount;
    record.wasPass = false;

    // Place the piece
    grid[row][col] = color;
    clearEmptyBit(row, col);
    if (color == CELL_BLACK) blackCount++; else whiteCount++;

    // Flip opponent pieces
    Cell opp = opposite(color);
    for (const Point& p : record.flipped) {
        grid[p.row][p.col] = color;
        if (color == CELL_BLACK) { blackCount++; whiteCount--; }
        else { whiteCount++; blackCount--; }
    }

    // Update empty neighbors around the placed piece
    updateEmptyNeighbors(row, col);
    // Also update empty neighbors around flipped pieces
    for (const Point& p : record.flipped)
        updateEmptyNeighbors(p.row, p.col);

    // Invalidate cached moves
    for (int i = 0; i < 3; i++) movesCached[i] = false;

    history.push_back(record);
    return record;
}

MoveRecord Board::makePass(Cell color) {
    MoveRecord record;
    record.pos = {-1, -1};
    record.color = color;
    record.prevBlackCount = blackCount;
    record.prevWhiteCount = whiteCount;
    record.wasPass = true;
    history.push_back(record);
    return record;
}

void Board::undoMove(const MoveRecord& record) {
    if (record.wasPass) {
        // Just remove from history
        if (!history.empty()) history.pop_back();
        for (int i = 0; i < 3; i++) movesCached[i] = false;
        return;
    }

    // Remove piece we placed
    grid[record.pos.row][record.pos.col] = CELL_EMPTY;
    if (record.color == CELL_BLACK) blackCount--; else whiteCount--;

    // Restore flipped pieces to opponent color
    Cell opp = opposite(record.color);
    for (const Point& p : record.flipped)
        grid[p.row][p.col] = opp;

    // Restore counts
    blackCount = record.prevBlackCount;
    whiteCount = record.prevWhiteCount;

    // Remove from history
    if (!history.empty()) history.pop_back();

    // Recompute empty neighbors from scratch (safest)
    emptyNeighbors = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (grid[r][c] != CELL_EMPTY) updateEmptyNeighbors(r, c);
        }
    }

    for (int i = 0; i < 3; i++) movesCached[i] = false;
}

void Board::undoLastMove() {
    if (history.empty()) return;
    MoveRecord rec = history.back(); // make a copy before reference goes stale
    undoMove(rec);
}
void Board::updateEmptyNeighbors(int row, int col) {
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++)
            if (dr != 0 || dc != 0)
                setEmptyBit(row + dr, col + dc);
}

bool Board::isGameOver() const {
    return !hasValidMoves(CELL_BLACK) && !hasValidMoves(CELL_WHITE);
}

Cell Board::getWinner() const {
    if (!isGameOver()) return CELL_EMPTY;
    if (blackCount > whiteCount) return CELL_BLACK;
    if (whiteCount > blackCount) return CELL_WHITE;
    return CELL_EMPTY; // draw
}

int Board::getTotalPieces() const {
    return blackCount + whiteCount;
}

static void sweepDirection(const Board& board, bool stable[BOARD_SIZE][BOARD_SIZE],
                            Cell color, int startR, int startC, int dr, int dc) {
    int r = startR, c = startC;
    bool allMine = true;
    while (board.isInBounds(r, c)) {
        if (board.at(r, c) != color) { allMine = false; break; }
        r += dr; c += dc;
    }
    if (!allMine) return;
    // All cells in this line from start to edge are ours - they're stable
    r = startR; c = startC;
    while (board.isInBounds(r, c) && board.at(r, c) == color) {
        stable[r][c] = stable[r][c] && true; // Keep accumulating
        r += dr; c += dc;
    }
}

int Board::countStable(Cell color) const {
    // Simple stable disc detection:
    // A disc is stable if it can never be flipped.
    // Corner-based approach: start from filled corners, BFS outward
    bool stable[BOARD_SIZE][BOARD_SIZE] = {false};

    // Check corners
    struct { int r, c; } corners[4] = {{0,0},{0,7},{7,0},{7,7}};
    for (auto& cn : corners) {
        if (grid[cn.r][cn.c] != color) continue;
        // Corner is ours - BFS along filled rows/cols
        stable[cn.r][cn.c] = true;

        // Sweep along row from corner
        sweepDirection(*this, stable, color, cn.r, cn.c, 0, (cn.c == 0 ? 1 : -1));
        // Sweep along col from corner
        sweepDirection(*this, stable, color, cn.r, cn.c, (cn.r == 0 ? 1 : -1), 0);
        // Sweep along diagonal
        sweepDirection(*this, stable, color, cn.r, cn.c,
                       (cn.r == 0 ? 1 : -1), (cn.c == 0 ? 1 : -1));
    }

    // Count stable discs of our color
    // Also check edges for stability (edge discs that connect to corners)
    // This is a simplified version - full Othello stability analysis is complex

    int count = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (grid[r][c] == color && stable[r][c])
                count++;
    return count;
}

std::string Board::toString() const {
    std::ostringstream oss;
    oss << "  A B C D E F G H\n";
    for (int r = 0; r < BOARD_SIZE; r++) {
        oss << (r + 1) << " ";
        for (int c = 0; c < BOARD_SIZE; c++) {
            char ch = '.';
            if (grid[r][c] == CELL_BLACK) ch = 'B';
            else if (grid[r][c] == CELL_WHITE) ch = 'W';
            oss << ch << (c < 7 ? ' ' : '\n');
        }
    }
    oss << "Black: " << blackCount << "  White: " << whiteCount;
    return oss.str();
}
