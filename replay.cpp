#include "replay.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

void ReplayData::clear() {
    blackName.clear();
    whiteName.clear();
    date.clear();
    winner = CELL_EMPTY;
    blackScore = 0;
    whiteScore = 0;
    entries.clear();
}

ReplaySystem::ReplaySystem() : recording(false), playing(false), currentStep(0), moveCounter(0) {}

void ReplaySystem::startRecording(const std::string& bName, const std::string& wName) {
    data.clear();
    data.blackName = bName;
    data.whiteName = wName;
    // Current timestamp
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    data.date = buf;
    recording = true;
    moveCounter = 0;
}

void ReplaySystem::recordMove(int row, int col, Cell color) {
    if (!recording) return;
    ReplayEntry e;
    e.moveNumber = ++moveCounter;
    e.pos = {row, col};
    e.color = color;
    e.isPass = false;
    data.entries.push_back(e);
}

void ReplaySystem::recordPass(Cell color) {
    if (!recording) return;
    ReplayEntry e;
    e.moveNumber = ++moveCounter;
    e.pos = {-1, -1};
    e.color = color;
    e.isPass = true;
    data.entries.push_back(e);
}

void ReplaySystem::stopRecording(Cell winner, int bScore, int wScore) {
    recording = false;
    data.winner = winner;
    data.blackScore = bScore;
    data.whiteScore = wScore;
}

bool ReplaySystem::saveToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << "#OTHELLO\n";
    f << "#Black: " << data.blackName << "\n";
    f << "#White: " << data.whiteName << "\n";
    if (data.winner == CELL_BLACK)
        f << "#Result: Black+" << (data.blackScore - data.whiteScore) << "\n";
    else if (data.winner == CELL_WHITE)
        f << "#Result: White+" << (data.whiteScore - data.blackScore) << "\n";
    else
        f << "#Result: Draw\n";
    f << "#Date: " << data.date << "\n";

    int num = 0;
    for (const auto& e : data.entries) {
        num++;
        if (e.isPass) {
            f << num << ". --";
            if (e.color == CELL_BLACK) f << "  (黑方 pass)";
            else f << "  (白方 pass)";
            f << "\n";
        } else {
            char colL = 'A' + e.pos.col;
            f << num << ". " << colL << (e.pos.row + 1) << "\n";
        }
    }

    // Final score line
    f << "#Final: Black " << data.blackScore << ", White " << data.whiteScore << "\n";
    return true;
}

bool ReplaySystem::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;

    data.clear();
    recording = false;
    moveCounter = 0;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;

        if (line.rfind("#OTHELLO", 0) == 0) continue;
        else if (line.rfind("#Black:", 0) == 0)
            data.blackName = line.substr(7);
        else if (line.rfind("#White:", 0) == 0)
            data.whiteName = line.substr(7);
        else if (line.rfind("#Result:", 0) == 0) {
            if (line.find("Black+") != std::string::npos) data.winner = CELL_BLACK;
            else if (line.find("White+") != std::string::npos) data.winner = CELL_WHITE;
            else data.winner = CELL_EMPTY;
        }
        else if (line.rfind("#Date:", 0) == 0)
            data.date = line.substr(6);
        else if (line.rfind("#Final:", 0) == 0) {
            size_t bPos = line.find("Black ");
            size_t wPos = line.find("White ");
            if (bPos != std::string::npos) data.blackScore = std::stoi(line.substr(bPos + 6));
            if (wPos != std::string::npos) data.whiteScore = std::stoi(line.substr(wPos + 6));
        }
        else if (isdigit(line[0])) {
            // Parse move: "1. d3" or "1. --"
            size_t dot = line.find(". ");
            if (dot == std::string::npos) continue;
            std::string move = line.substr(dot + 2);
            if (move.empty()) continue;

            ReplayEntry e;
            e.moveNumber = ++moveCounter;

            if (move[0] == '-' && move[1] == '-') {
                e.isPass = true;
                e.pos = {-1, -1};
                // Guess color: black moves on odd numbers
                e.color = (moveCounter % 2 == 1) ? CELL_BLACK : CELL_WHITE;
            } else {
                e.isPass = false;
                e.pos.col = toupper(move[0]) - 'A';
                e.pos.row = std::stoi(move.substr(1)) - 1;
                e.color = (moveCounter % 2 == 1) ? CELL_BLACK : CELL_WHITE;
            }
            data.entries.push_back(e);
        }
    }

    return !data.entries.empty();
}

void ReplaySystem::startPlayback() {
    playing = true;
    currentStep = 0;
}

void ReplaySystem::stopPlayback() {
    playing = false;
}

void ReplaySystem::setStep(int step) {
    currentStep = std::max(0, std::min((int)data.entries.size(), step));
}

void ReplaySystem::stepForward() {
    if (currentStep < (int)data.entries.size())
        currentStep++;
}

void ReplaySystem::stepBackward() {
    if (currentStep > 0)
        currentStep--;
}

Board ReplaySystem::getBoardAtStep(int step) const {
    Board b;
    // Apply moves up to `step`
    for (int i = 0; i < step && i < (int)data.entries.size(); i++) {
        const auto& e = data.entries[i];
        if (!e.isPass && b.isValidMove(e.pos.row, e.pos.col, e.color)) {
            b.applyMove(e.pos.row, e.pos.col, e.color);
        } else if (!e.isPass) {
            // Move might not be valid if board state changed - skip
            break;
        }
        // Pass: just continue (Board handles pass internally)
    }
    return b;
}

std::string ReplaySystem::getStatusText() const {
    if (!playing) return "未在播放";
    std::ostringstream oss;
    oss << "回放: " << currentStep << " / " << data.entries.size();
    return oss.str();
}
