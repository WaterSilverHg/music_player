// rtsp_player.h: 基于 FFmpeg + QAudioSink 的 RTSP 拉流播放器
// 替代 QMediaPlayer 在服务器模式下的 RTSP 播放，支持 seek 和状态管理

#pragma once

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QByteArray>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

class RtspPlayer : public QObject {
    Q_OBJECT
public:
    explicit RtspPlayer(QObject* parent = nullptr);
    ~RtspPlayer();

    // 播放控制
    void play(const QString& rtspUrl, double durationSecs = 0.0);
    void pause();
    void resume();
    void stop();
    void seek(qint64 positionMs);

    // 状态查询
    qint64 position() const { return m_position.load(); }
    qint64 duration() const { return m_duration.load(); }
    int state() const { return m_state.load(); }  // 0=Stopped, 1=Playing, 2=Paused
    QAudioSink* audioSink() const { return m_audioSink; }

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void stateChanged(int state);       // 0=Stopped, 1=Playing, 2=Paused
    void errorOccurred(const QString& errorMsg);
    void audioDataReady(const QByteArray& pcmData);  // PCM 数据就绪，在主线程写入

private slots:
    void onAudioDataReady(const QByteArray& pcmData);  // 在主线程写入 QAudioSink

private:
    void workerLoop(const QString& rtspUrl);
    void setState(int s);
    void initAudioSink();  // 在主线程初始化音频输出

    // QAudioSink (Qt 6 音频输出)
    QAudioSink* m_audioSink = nullptr;
    QIODevice* m_audioDevice = nullptr;  // 音频写入设备

    // 工作线程
    std::unique_ptr<std::thread> m_thread;

    // 原子状态
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_pauseRequested{false};
    std::atomic<qint64> m_seekTarget{-1};  // -1 = no seek pending, ms
    std::atomic<qint64> m_position{0};
    std::atomic<qint64> m_duration{0};
    std::atomic<int> m_state{0};  // 0=Stopped, 1=Playing, 2=Paused

    std::mutex m_mutex;
};
