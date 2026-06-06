#ifndef LYRICS_GENERATOR_H
#define LYRICS_GENERATOR_H

#include <string>
#include <vector>

struct LyricLine {
    std::string text;
    double start_time;  // 秒
    double end_time;    // 秒
};

// Lyrics generation is now handled by server
// This class only provides LRC format utilities for client-side display
class LyricsGenerator {
public:
    // Save lyrics to LRC file format
    static bool saveToLRC(const std::vector<LyricLine>& lyrics, const std::string& outputPath);
};

#endif
