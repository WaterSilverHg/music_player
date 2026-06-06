#include "audio_extractor.h"

AudioExtractor::AudioExtractor()
    : formatContext_(nullptr), codecContext_(nullptr), swrContext_(nullptr) {
    avformat_network_init();
}

AudioExtractor::~AudioExtractor() {
    cleanup();
    avformat_network_deinit();
}

bool AudioExtractor::extractToWav(const std::string& inputPath, const std::string& outputPath) {
    qDebug()<<"start";
    auto samples = extractAudioSamples(inputPath);
    qDebug()<<"1";
    if (samples->empty()) {
        return false;
    }
    qDebug()<<"1";
    // 简单的 WAV 文件写入
    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        std::cerr << "无法创建输出文件: " << outputPath << std::endl;
            return false;
    }
    qDebug()<<"1";

    // WAV 头（16kHz, 32-bit float, 单声道）
    struct WavHeader {
        char chunkId[4] = {'R','I','F','F'};
        uint32_t chunkSize;
        char format[4] = {'W','A','V','E'};
        char subchunk1Id[4] = {'f','m','t',' '};
        uint32_t subchunk1Size = 16;
        uint16_t audioFormat = 3; // IEEE float
        uint16_t numChannels = 1;
        uint32_t sampleRate = 16000;
        uint32_t byteRate = 16000 * 4;
        uint16_t blockAlign = 4;
        uint16_t bitsPerSample = 32;
        char subchunk2Id[4] = {'d','a','t','a'};
        uint32_t subchunk2Size;
    } header;

    header.chunkSize = 36 + samples->size() * sizeof(float);
    header.subchunk2Size = samples->size() * sizeof(float);

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(samples->data()), samples->size() * sizeof(float));

    std::cout << "音频提取完成: " << outputPath << std::endl;
        return true;
}

std::shared_ptr<std::vector<float>> AudioExtractor::extractAudioSamples(const std::string& inputPath) {
        std::shared_ptr<std::vector<float>> samples(new std::vector<float>());

    // 打开输入文件
    if (avformat_open_input(&formatContext_, inputPath.c_str(), nullptr, nullptr) < 0) {
//        std::cerr << "无法打开输入文件: " << inputPath << std::endl;
        qDebug() << "无法打开输入文件: " << inputPath;
        return samples;
    }

    if (avformat_find_stream_info(formatContext_, nullptr) < 0) {
//        std::cerr << "无法获取流信息" << std::endl;
            qDebug()<<"无法获取流信息";
        return samples;
    }

    // 查找音频流
    int audioStreamIndex = -1;
    for (int i = 0; i < formatContext_->nb_streams; i++) {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }

    if (audioStreamIndex == -1) {
        //std::cerr << "未找到音频流" << std::endl;
        qDebug()<<"未找到音频流";
            return samples;
    }

    if (!setupCodec(audioStreamIndex)) {
        return samples;
    }

    if (!setupResampler()) {
        return samples;
    }

    if (!decodeAudioFrames(*samples.get())) {
        return samples;
    }

    qDebug() << "提取了 " << samples->size() << " 个音频样本";
    return samples;
}

bool AudioExtractor::setupCodec(int audioStreamIndex) {
    AVCodecParameters* codecParams = formatContext_->streams[audioStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        std::cerr << "不支持的解码器" << std::endl;
            return false;
    }

    codecContext_ = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext_, codecParams);

    if (avcodec_open2(codecContext_, codec, nullptr) < 0) {
        std::cerr << "无法打开解码器" << std::endl;
            return false;
    }

    return true;
}

bool AudioExtractor::setupResampler() {
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 1);  // 1 个声道 = 单声道

    // 使用 swr_alloc_set_opts2 一次性设置所有参数
    int ret = swr_alloc_set_opts2(&swrContext_,
                                  &out_ch_layout,           // 输出声道布局
                                  AV_SAMPLE_FMT_FLT,        // 输出采样格式
                                  16000,                    // 输出采样率
                                  &codecContext_->ch_layout, // 输入声道布局
                                  codecContext_->sample_fmt, // 输入采样格式
                                  codecContext_->sample_rate, // 输入采样率
                                  0, nullptr);

    // 清理输出的声道布局
    av_channel_layout_uninit(&out_ch_layout);

    if (ret < 0) {
            std::cerr << "无法设置重采样选项: " << ret << std::endl;
                swr_free(&swrContext_);
            return false;
    }

    if (swr_init(swrContext_) < 0) {
            std::cerr << "无法初始化重采样器" << std::endl;
            swr_free(&swrContext_);
            return false;
    }

    return true;
}

bool AudioExtractor::decodeAudioFrames(std::vector<float>& samples) {
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (av_read_frame(formatContext_, packet) >= 0) {
        if (packet->stream_index == formatContext_->streams[0]->index) {
            if (avcodec_send_packet(codecContext_, packet) == 0) {
                while (avcodec_receive_frame(codecContext_, frame) == 0) {
                    // 重采样
                    float* resampledData = nullptr;
                    int outSamples = swr_get_out_samples(swrContext_, frame->nb_samples);
                    av_samples_alloc((uint8_t**)&resampledData, nullptr, 1, outSamples, AV_SAMPLE_FMT_FLT, 0);

                    int converted = swr_convert(swrContext_,
                                                (uint8_t**)&resampledData, outSamples,
                                                (const uint8_t**)frame->data, frame->nb_samples);

                    if (converted > 0) {
                        samples.insert(samples.end(), resampledData, resampledData + converted);
                    }

                    av_freep(&resampledData);
                }
            }
        }
        av_packet_unref(packet);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    return true;
}

void AudioExtractor::cleanup() {
    if (swrContext_) {
        swr_free(&swrContext_);
    }
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
    }
    if (formatContext_) {
        avformat_close_input(&formatContext_);
    }
}
