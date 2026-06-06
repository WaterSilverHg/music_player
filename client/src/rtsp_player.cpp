// rtsp_player.cpp: FFmpeg + QAudioSink RTSP 播放器实现
// 所有 QAudioSink 操作都在主线程中执行，避免跨线程问题

#include "rtsp_player.h"
#include <QDebug>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

using namespace std::chrono;

// ---- 构造 / 析构 ----

RtspPlayer::RtspPlayer(QObject* parent)
    : QObject(parent)
{
    // 连接信号槽：工作线程发送 PCM 数据，主线程写入 QAudioSink
    connect(this, &RtspPlayer::audioDataReady, this, &RtspPlayer::onAudioDataReady, Qt::QueuedConnection);
}

RtspPlayer::~RtspPlayer()
{
    stop();
}

// ---- 播放控制 ----

void RtspPlayer::play(const QString& rtspUrl, double durationSecs)
{
    // 停止当前播放
    stop();

    // 保存预估时长（如果提供的话，用于 RTSP 流没有时长信息时）
    if (durationSecs > 0) {
        m_duration = static_cast<qint64>(durationSecs * 1000.0);
        emit durationChanged(m_duration.load());
    } else {
        m_duration = 0;  // 将在流打开后从 RTSP 获取真实时长
    }

    // 在主线程初始化音频输出
    initAudioSink();

    m_running = true;
    m_pauseRequested = false;
    m_seekTarget = -1;
    m_position = 0;

    setState(1);  // Playing

    // 启动工作线程（FFmpeg 拉流 + 解码）
    m_thread = std::make_unique<std::thread>(&RtspPlayer::workerLoop, this, rtspUrl);
}

void RtspPlayer::initAudioSink()
{
    // 配置音频格式（与服务端 AAC 输出对齐）
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    // 创建 QAudioSink（Qt 6 音频输出）
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->deleteLater();
    }
    m_audioSink = new QAudioSink(format, this);
    m_audioSink->setBufferSize(44100 * 2 * 2 / 10);  // ~100ms buffer

    // 启动音频输出
    m_audioDevice = m_audioSink->start();
    qDebug() << "[RtspPlayer] Audio sink initialized, buffer size:" << m_audioSink->bufferSize();
}

void RtspPlayer::onAudioDataReady(const QByteArray& pcmData)
{
    // 在主线程写入音频数据
    if (m_audioDevice && m_audioSink && !m_pauseRequested.load()) {
        m_audioDevice->write(pcmData);
    }
}

void RtspPlayer::pause()
{
    if (!m_running) return;
    m_pauseRequested = true;
    if (m_audioSink) m_audioSink->suspend();
}

void RtspPlayer::resume()
{
    if (!m_running) return;
    m_pauseRequested = false;
    if (m_audioSink) m_audioSink->resume();
}

void RtspPlayer::stop()
{
    m_running = false;
    m_pauseRequested = false;

    // 非阻塞停止：设置标志让工作线程自行退出，不等待
    // join() 会阻塞主线程，改用延迟清理
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->deleteLater();
        m_audioSink = nullptr;
        m_audioDevice = nullptr;
    }

    setState(0);  // Stopped
}

void RtspPlayer::seek(qint64 positionMs)
{
    m_seekTarget = positionMs;
}

// ---- 私有方法 ----

void RtspPlayer::setState(int s)
{
    if (m_state.exchange(s) != s) {
        emit stateChanged(s);
    }
}

void RtspPlayer::workerLoop(const QString& streamUrl)
{
    qDebug() << "[RtspPlayer] Starting worker loop for URL:" << streamUrl;
    
    // ================================================================
    // 1. 打开媒体流（支持 RTSP 和 HTTP）
    // ================================================================
    AVFormatContext* fmtCtx = nullptr;
    AVDictionary* opts = nullptr;
    
    // 根据 URL 协议设置不同选项
    bool isRtsp = streamUrl.startsWith("rtsp://", Qt::CaseInsensitive);
    
    if (isRtsp) {
        // RTSP 特定选项
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "stimeout",  "10000000", 0);  // 10s 超时
        av_dict_set(&opts, "max_delay", "500000",   0);
        qDebug() << "[RtspPlayer] Opening RTSP stream...";
    } else {
        // HTTP 流选项
        av_dict_set(&opts, "timeout", "10000000", 0);  // 10s 超时
        qDebug() << "[RtspPlayer] Opening HTTP stream...";
    }

    if (avformat_open_input(&fmtCtx, streamUrl.toUtf8().constData(), nullptr, &opts) < 0) {
        av_dict_free(&opts);
        qWarning() << "[RtspPlayer] Failed to open stream";
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("无法打开媒体流"));
        }, Qt::QueuedConnection);
        setState(0);
        m_running = false;
        return;
    }
    av_dict_free(&opts);
    qDebug() << "[RtspPlayer] Stream opened successfully";

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        qWarning() << "[RtspPlayer] Failed to get stream info";
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("无法获取流信息"));
        }, Qt::QueuedConnection);
        setState(0);
        m_running = false;
        return;
    }
    qDebug() << "[RtspPlayer] Stream info retrieved successfully";

    // ================================================================
    // 获取总时长（从 AVFormatContext 读取）
    // ================================================================
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        // 总时长单位为微秒，除以 AV_TIME_BASE 得到秒
        double durationSec = static_cast<double>(fmtCtx->duration) / AV_TIME_BASE;
        m_duration = static_cast<qint64>(durationSec * 1000);  // 转换为毫秒
        qDebug() << "[RtspPlayer] Stream duration:" << m_duration << "ms (" << durationSec << "s)";
        QMetaObject::invokeMethod(this, [this]() {
            emit durationChanged(m_duration.load());
        }, Qt::QueuedConnection);
    } else {
        qDebug() << "[RtspPlayer] Stream duration not available";
    }

    // ================================================================
    // 2. 查找音频流
    // ================================================================
    int audioIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIdx = static_cast<int>(i);
            break;
        }
    }
    if (audioIdx < 0) {
        avformat_close_input(&fmtCtx);
        qWarning() << "[RtspPlayer] No audio stream found";
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("流中没有音频轨道"));
        }, Qt::QueuedConnection);
        setState(0);
        m_running = false;
        return;
    }
    qDebug() << "[RtspPlayer] Audio stream found at index:" << audioIdx;

    // ================================================================
    // 3. 设置解码器
    // ================================================================
    AVCodecParameters* codecPar = fmtCtx->streams[audioIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        qWarning() << "[RtspPlayer] Unsupported codec:" << codecPar->codec_id;
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("不支持的音频编码格式"));
        }, Qt::QueuedConnection);
        setState(0);
        m_running = false;
        return;
    }
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecPar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        qWarning() << "[RtspPlayer] Failed to open codec";
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("无法打开音频解码器"));
        }, Qt::QueuedConnection);
        setState(0);
        m_running = false;
        return;
    }
    qDebug() << "[RtspPlayer] Codec opened successfully:" << codec->name;

    qDebug() << "[RtspPlayer] Audio codec:" << avcodec_get_name(codecPar->codec_id)
             << codecPar->sample_rate << "Hz" << codecPar->ch_layout.nb_channels << "ch";

    // ================================================================
    // 4. 设置重采样器 → S16 / 44100Hz / Stereo（QAudioSink 格式）
    // ================================================================
    constexpr int OUT_SAMPLE_RATE = 44100;
    constexpr int OUT_CHANNELS   = 2;

    SwrContext* swr = nullptr;
    AVChannelLayout inChLayout  = codecCtx->ch_layout;
    AVChannelLayout outChLayout = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&swr,
        &outChLayout, AV_SAMPLE_FMT_S16, OUT_SAMPLE_RATE,
        &inChLayout,  codecCtx->sample_fmt, codecCtx->sample_rate,
        0, nullptr);
    if (!swr || swr_init(swr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("无法初始化音频重采样器"));
        }, Qt::QueuedConnection);
        setState(0);
        m_running = false;
        return;
    }

    // ================================================================
    // 5. 主循环：读帧 → 解码 → 重采样 → 发送 PCM 数据到主线程
    // ================================================================
    AVPacket* pkt  = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    uint64_t totalSamples = 0;
    qint64 lastEmitPos = 0;
    int outSampleRate = OUT_SAMPLE_RATE;
    int consecutiveErrors = 0;

    qDebug() << "[RtspPlayer] Starting playback loop...";

    // 发送一小段静音数据，激活音频设备
    {
        constexpr int dummySamples = 1024;  // 约 23ms 静音
        int dummyLen = dummySamples * OUT_CHANNELS * 2;
        QByteArray dummy(dummyLen, 0);
        emit audioDataReady(dummy);
    }

    while (m_running) {
        // --- 处理 seek 请求 ---
        qint64 seekTarget = m_seekTarget.exchange(-1);
        if (seekTarget >= 0) {
            qDebug() << "[RtspPlayer] Seeking to:" << seekTarget << "ms";
            AVRational tb = fmtCtx->streams[audioIdx]->time_base;
            int64_t seekTs = av_rescale_q(seekTarget, AVRational{1, 1000}, tb);
            int ret = av_seek_frame(fmtCtx, audioIdx, seekTs,
                                    AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
            if (ret >= 0) {
                avcodec_flush_buffers(codecCtx);
                totalSamples = static_cast<uint64_t>(seekTarget) * outSampleRate / 1000;
                // 在主线程重置音频缓冲区
                QMetaObject::invokeMethod(this, [this]() {
                    if (m_audioSink && m_audioDevice) {
                        m_audioSink->reset();
                        m_audioDevice = m_audioSink->start();
                    }
                }, Qt::QueuedConnection);
                qDebug() << "[RtspPlayer] Seek completed";
            } else {
                qWarning() << "[RtspPlayer] Seek failed";
            }
        }

        // --- 处理暂停 ---
        if (m_pauseRequested) {
            setState(2);  // Paused
            std::this_thread::sleep_for(milliseconds(100));
            continue;
        } else {
            setState(1);  // Playing
        }

        // --- 读取数据包 ---
        int ret = av_read_frame(fmtCtx, pkt);
        if (ret < 0) {
            constexpr int MAX_CONSECUTIVE_ERRORS = 10;
            consecutiveErrors++;

            if (ret == AVERROR_EOF) {
                qDebug() << "[RtspPlayer] Stream ended (EOF), stopping playback";
                // EOF 表示流正常结束，直接退出循环
                break;
            } else {
                qDebug() << "[RtspPlayer] Read error:" << ret
                         << "(" << consecutiveErrors << "/" << MAX_CONSECUTIVE_ERRORS << ")";
            }

            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit errorOccurred(QStringLiteral("RTSP 连接断开，连续读取失败"));
                }, Qt::QueuedConnection);
                break;
            }

            std::this_thread::sleep_for(milliseconds(100));
            continue;
        }

        // 成功读取，重置连续失败计数
        consecutiveErrors = 0;

        if (pkt->stream_index == audioIdx) {
            avcodec_send_packet(codecCtx, pkt);
            while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                // 重采样 → S16
                uint8_t* outData = nullptr;
                int outSamples = swr_get_out_samples(swr, frame->nb_samples);
                av_samples_alloc(&outData, nullptr, OUT_CHANNELS,
                                 outSamples, AV_SAMPLE_FMT_S16, 0);
                int actualSamples = swr_convert(swr, &outData, outSamples,
                                                (const uint8_t**)frame->data,
                                                frame->nb_samples);

                if (actualSamples > 0 && !m_pauseRequested) {
                    int dataLen = actualSamples * OUT_CHANNELS * 2;  // channels × 2 bytes (s16)
                    // 通过信号将 PCM 数据发送到主线程
                    QByteArray pcmData(reinterpret_cast<const char*>(outData), dataLen);
                    emit audioDataReady(pcmData);
                    totalSamples += actualSamples;
                }
                av_freep(&outData);
            }
        }
        av_packet_unref(pkt);

        // --- 更新位置（每 ~100ms 发射一次信号） ---
        qint64 posMs = static_cast<qint64>(totalSamples) * 1000 / outSampleRate;
        m_position = posMs;
        if (posMs - lastEmitPos >= 100 || posMs < lastEmitPos) {
            // 位置前进超过 100ms，或者位置倒退了，都发射信号
            emit positionChanged(posMs);
            lastEmitPos = posMs;
        }
    }

    // ================================================================
    // 6. 清理
    // ================================================================
    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    // 在主线程停止音频输出
    QMetaObject::invokeMethod(this, [this]() {
        if (m_audioSink) {
            m_audioSink->stop();
        }
    }, Qt::QueuedConnection);

    // 清理线程对象（在工作线程中执行）
    if (m_thread && m_thread->get_id() == std::this_thread::get_id()) {
        m_thread.reset();
    }

    qDebug() << "[RtspPlayer] Worker thread stopped";
}