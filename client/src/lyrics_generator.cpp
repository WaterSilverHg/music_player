#include "lyrics_generator.h"
#include <fstream>
#include <iomanip>
#include <sstream>

bool LyricsGenerator::saveToLRC(const std::vector<LyricLine>& lyrics, const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }

    // LRC 文件头
    file << "[ar:Unknown Artist]\n";
    file << "[ti:Generated Lyrics]\n";
    file << "[by:Lyrics Generator]\n";
    file << "[re:Player]\n";
    file << "[ve:1.0]\n\n";

    // 歌词内容
    for (const auto& line : lyrics) {
        int minutes = static_cast<int>(line.start_time) / 60;
        int seconds = static_cast<int>(line.start_time) % 60;
        int centiseconds = static_cast<int>((line.start_time - static_cast<int>(line.start_time)) * 100);

        file << "[" << std::setfill('0') << std::setw(2) << minutes << ":"
             << std::setw(2) << seconds << "." << std::setw(2) << centiseconds << "]"
             << line.text << "\n";
    }

    file.close();
    return true;
}
