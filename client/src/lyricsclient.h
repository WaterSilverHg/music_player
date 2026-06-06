// lyricsclient.h: Python歌词服务器API客户端
// 单例模式，用于连接歌词生成服务

#pragma once

#include "global.h"

class LyricsClient : public QObject {
    Q_OBJECT

public:
    static LyricsClient* instance();

    // 初始化
    void init(const QString& baseUrl);

    // 连接状态
    bool isConnected() const { return m_connected; }
    QString baseUrl() const { return m_baseUrl; }

    // ---- API 调用 ----

    // 健康检查
    void healthCheck();

    // 上传音频文件并生成歌词
    void uploadAndGenerateLyrics(const QString& localFilePath, const QString& modelSize = "base");

    // 获取歌词
    void getLyrics(const QString& fileId);

    // 删除歌词
    void deleteLyrics(const QString& fileId);

signals:
    // 连接状态变化
    void connected();
    void disconnected();

    // 健康检查
    void healthReceived(bool ok, bool whisperAvailable, bool modelLoaded);

    // 歌词生成
    void uploadProgress(qint64 sent, qint64 total);
    void lyricsGenerated(bool success, const QString& lrc, double duration, 
                         const QString& language, const QString& message);

    // 歌词获取
    void lyricsReceived(bool success, const QString& lrc);

    // 歌词删除
    void lyricsDeleted(bool success);

    // 通用错误
    void errorOccurred(const QString& errorMsg);

private slots:
    void onHealthReply(QNetworkReply* reply);

private:
    explicit LyricsClient(QObject* parent = nullptr);
    static LyricsClient* s_instance;

    QNetworkAccessManager* m_nam;
    QString m_baseUrl;
    bool m_connected = false;

    // 通用 GET 请求
    QNetworkReply* apiGet(const QString& path);
    // 通用 POST 请求（multipart）
    QNetworkReply* apiPostMultipart(const QString& path, QHttpMultiPart* multiPart);
    // 通用 DELETE 请求
    QNetworkReply* apiDelete(const QString& path);

    // 解析 JSON
    static QJsonObject parseJson(const QByteArray& data, bool& ok);
};
