// apiclient.h: 服务端 API 客户端
// 单例模式，基于 QNetworkAccessManager 的异步 HTTP 通信

#pragma once

#include "global.h"

struct RemoteSongInfo {
    QString id;
    QString name;
    QString title;
    QString artist;
    QString album;
    double duration = 0.0;
    QString format;
    qint64 size = 0;
    bool hasCover = false;
    QString lyricsStatus;
    QString fileStatus;
    QString uploadTime;

    static RemoteSongInfo fromJson(const QJsonObject& obj) {
        RemoteSongInfo info;
        info.id = obj["id"].toString();
        info.name = obj["name"].toString();
        info.title = obj["title"].toString();
        info.artist = obj["artist"].toString();
        info.album = obj["album"].toString();
        info.duration = obj["duration"].toDouble();
        info.format = obj["format"].toString();
        info.size = static_cast<qint64>(obj["size"].toDouble());
        info.hasCover = obj["has_cover"].toBool();
        info.lyricsStatus = obj["lyrics_status"].toString();
        info.fileStatus = obj["file_status"].toString();
        info.uploadTime = obj["upload_time"].toString();
        return info;
    }
};

class ApiClient : public QObject {
    Q_OBJECT

public:
    static ApiClient* instance();

    // 从 ConfigManager 初始化
    void init(const QString& baseUrl);

    // 连接状态
    bool isConnected() const { return m_connected; }
    QString baseUrl() const { return m_baseUrl; }

    // ---- API 调用 ----

    // 检查服务器状态
    void checkStatus();

    // 上传文件
    void uploadFile(const QString& localFilePath);

    // 下载文件
    void downloadFile(const QString& remoteId, const QString& savePath);

    // 列出服务器文件
    void listServerFiles();

    // 搜索服务器文件
    void searchServer(const QString& query);

    // 删除服务器文件
    void deleteServerFile(const QString& remoteId);

    // 请求播放（返回 RTSP URL）
    void requestPlay(const QString& remoteId);

    // 停止播放
    void requestStop(const QString& remoteId);

    // 下载歌词
    void downloadLyrics(const QString& remoteId);

    // 下载封面
    void downloadCover(const QString& remoteId);

signals:
    // 连接状态变化
    void connected();
    void disconnected();

    // 状态检查
    void statusReceived(bool ok, int songCount);

    // 上传
    void uploadProgress(qint64 sent, qint64 total);
    void uploadFinished(bool success, const QString& fileId, const QString& fileName);

    // 下载
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(bool success, const QString& localPath);

    // 文件列表
    void serverFilesReceived(const QList<RemoteSongInfo>& files);

    // 搜索结果
    void searchResultReceived(const QList<RemoteSongInfo>& results);

    // 删除结果
    void deleteFinished(bool success, const QString& remoteId);

    // 播放请求
    void streamUrlReady(const QString& remoteId, const QString& rtspUrl);
    void stopFinished(bool success, const QString& remoteId);

    // 歌词下载
    void lyricsReady(const QString& remoteId, const QByteArray& lrcData);
    void lyricsStatus(const QString& remoteId, const QString& status);

    // 封面下载
    void coverReady(const QString& remoteId, const QByteArray& imageData);

    // 通用错误
    void errorOccurred(const QString& errorMsg);

private slots:
    void onHeartbeat();
    void onStatusReply(QNetworkReply* reply);

private:
    explicit ApiClient(QObject* parent = nullptr);
    static ApiClient* s_instance;

    QNetworkAccessManager* m_nam;
    QTimer* m_heartbeatTimer;
    QString m_baseUrl;
    bool m_connected = false;

    // 通用 GET 请求
    QNetworkReply* apiGet(const QString& path);
    // 通用 POST 请求
    QNetworkReply* apiPost(const QString& path, const QByteArray& data, const QString& contentType);
    // 通用 DELETE 请求
    QNetworkReply* apiDelete(const QString& path);

    // 解析 API 响应 JSON
    static QJsonObject parseResponse(const QByteArray& data, bool& ok);
};
