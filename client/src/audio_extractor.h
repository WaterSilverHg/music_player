#ifndef AUDIO_EXTRACTOR_H
#define AUDIO_EXTRACTOR_H

#include"global.h"

class AudioExtractor {
public:
    AudioExtractor();
    ~AudioExtractor();

    bool extractToWav(const std::string& inputPath, const std::string& outputPath);
    std::shared_ptr<std::vector<float>> extractAudioSamples(const std::string& inputPath);

private:
    AVFormatContext* formatContext_;
    AVCodecContext* codecContext_;
    SwrContext* swrContext_;

    bool setupCodec(int audioStreamIndex);
    bool setupResampler();
    bool decodeAudioFrames(std::vector<float>& samples);
    void cleanup();
};

#endif
