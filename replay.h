#ifndef OTHELLO_REPLAY_H
#define OTHELLO_REPLAY_H

#include "board.h"
#include <string>
#include <vector>
#include <ctime>

// .oth file format:
// #OTHELLO
// #Black: name
// #White: name
// #Result: Black+12
// #Date: 2026-07-09 14:30
// 1. d3
// 2. c5
// ...

struct ReplayEntry {
    int moveNumber;
    Point pos;
    Cell color;
    bool isPass;
};

struct ReplayData {
    std::string blackName;
    std::string whiteName;
    std::string date;
    Cell winner;
    int blackScore;
    int whiteScore;
    std::vector<ReplayEntry> entries;

    void clear();
};

class ReplaySystem {
public:
    ReplaySystem();

    // Recording
    void startRecording(const std::string& blackName, const std::string& whiteName);
    void recordMove(int row, int col, Cell color);
    void recordPass(Cell color);
    void stopRecording(Cell winner, int blackScore, int whiteScore);
    bool isRecording() const { return recording; }

    // Save/Load
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

    // Playback
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const { return playing; }

    // Step control
    int getCurrentStep() const { return currentStep; }
    int getTotalSteps() const { return (int)data.entries.size(); }
    void setStep(int step);
    void stepForward();
    void stepBackward();

    // Get the board state at the current step
    Board getBoardAtStep(int step) const;

    // Get raw data
    const ReplayData& getData() const { return data; }
    void setData(const ReplayData& d) { data = d; }

    // Get formatted status
    std::string getStatusText() const;

private:
    ReplayData data;
    bool recording;
    bool playing;
    int currentStep;
    int moveCounter;
};

#endif
