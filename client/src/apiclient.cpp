// apiclient.cpp: 服务端 API 客户端实现

#include "apiclient.h"
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

ApiClient* ApiClient::s_instance = nullptr;

ApiClient* ApiClient::instance() {
    if (!s_instance) {
        s_instance = new ApiClient();
    }
    return s_instance;
}

ApiClient::ApiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_heartbeatTimer(new QTimer(this))
{
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ApiClient::onHeartbeat);
}

void ApiClient::init(const QString& baseUrl) {
    m_baseUrl = baseUrl;
    while (m_baseUrl.endsWith('/')) m_baseUrl.chop(1);
    qDebug() << "[ApiClient] Initialized, base URL:" << m_baseUrl;

    // 启动心跳（10 秒间隔，降低服务器负载）
    m_heartbeatTimer->start(10000);
    checkStatus();
}

// ---- 通用请求 ----

QNetworkReply* ApiClient::apiGet(const QString& path) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return m_nam->get(req);
}

QNetworkReply* ApiClient::apiPost(const QString& path, const QByteArray& data, const QString& contentType) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    return m_nam->post(req, data);
}

QNetworkReply* ApiClient::apiDelete(const QString& path) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    return m_nam->deleteResource(req);
}

QJsonObject ApiClient::parseResponse(const QByteArray& data, bool& ok) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[ApiClient] JSON parse error:" << err.errorString();
        ok = false;
        return {};
    }
    ok = true;
    return doc.object();
}

// ---- 心跳 ----

void ApiClient::onHeartbeat() {
    checkStatus();
}

void ApiClient::onStatusReply(QNetworkReply* reply) {
    bool wasConnected = m_connected;

    if (reply->error() == QNetworkReply::NoError) {
        bool ok;
        QJsonObject json = parseResponse(reply->readAll(), ok);
        if (ok && json["code"].toInt(200) == 200) {
            m_connected = true;
            if (!wasConnected) emit connected();
            emit statusReceived(true, json["data"].toObject()["song_count"].toInt(0));
        } else {
            m_connected = false;
            if (wasConnected) emit disconnected();
        }
    } else {
        m_connected = false;
        if (wasConnected) emit disconnected();
    }
    reply->deleteLater();
}

// ---- API 实现 ----

void ApiClient::checkStatus() {
    QNetworkReply* reply = apiGet("/api/status");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onStatusReply(reply);
    });
}

void ApiClient::uploadFile(const QString& localFilePath) {
    uploadFile(localFilePath, QString(), QString(), QString(), QString());
}

void ApiClient::uploadFile(const QString& localFilePath, const QString& title,
                           const QString& artist, const QString& album,
                           const QString& coverPath) {
    QFileInfo fi(localFilePath);
    if (!fi.exists()) {
        emit errorOccurred(QString("文件不存在: %1").arg(localFilePath));
        emit uploadFinished(false, "", fi.fileName());
        return;
    }

    // 30MB 大小限制
    constexpr qint64 maxUploadSize = 30 * 1024 * 1024;
    if (fi.size() > maxUploadSize) {
        emit errorOccurred(QString("文件过大 (%1 MB)，最大支持 30 MB")
                               .arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 1));
        emit uploadFinished(false, "", fi.fileName());
        return;
    }

    // 构建 multipart 请求
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 添加音乐文件
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName()));

    QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(localFilePath).name();
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);

    QFile* file = new QFile(localFilePath);
    file->open(QIODevice::ReadOnly);
    filePart.setBodyDevice(file);
    file->setParent(multiPart);  // multiPart 销毁时自动删除 file
    multiPart->append(filePart);

    // 添加元数据
    if (!title.isEmpty()) {
        QHttpPart titlePart;
        titlePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           "form-data; name=\"title\"");
        titlePart.setBody(title.toUtf8());
        multiPart->append(titlePart);
    }

    if (!artist.isEmpty()) {
        QHttpPart artistPart;
        artistPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            "form-data; name=\"artist\"");
        artistPart.setBody(artist.toUtf8());
        multiPart->append(artistPart);
    }

    if (!album.isEmpty()) {
        QHttpPart albumPart;
        albumPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           "form-data; name=\"album\"");
        albumPart.setBody(album.toUtf8());
        multiPart->append(albumPart);
    }

    // 添加封面图片（如果有）
    if (!coverPath.isEmpty()) {
        QFileInfo coverFi(coverPath);
        if (coverFi.exists()) {
            QHttpPart coverPart;
            coverPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                               QString("form-data; name=\"cover\"; filename=\"%1\"").arg(coverFi.fileName()));
            
            QString coverMimeType = mimeDb.mimeTypeForFile(coverPath).name();
            coverPart.setHeader(QNetworkRequest::ContentTypeHeader, coverMimeType);

            QFile* coverFile = new QFile(coverPath);
            coverFile->open(QIODevice::ReadOnly);
            coverPart.setBodyDevice(coverFile);
            coverFile->setParent(multiPart);
            multiPart->append(coverPart);
        }
    }

    QNetworkRequest req(QUrl(m_baseUrl + "/api/upload"));
    QNetworkReply* reply = m_nam->post(req, multiPart);
    multiPart->setParent(reply);  // reply 销毁时自动删除 multiPart

    connect(reply, &QNetworkReply::uploadProgress, this,
            [this](qint64 sent, qint64 total) { emit uploadProgress(sent, total); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, fileName = fi.fileName()]() {
        if (reply->error() == QNetworkReply::NoError) {
            bool ok;
            QJsonObject json = parseResponse(reply->readAll(), ok);
            if (ok) {
                QString id = json.value("id").toString();
                qDebug() << "[ApiClient] Upload success:" << fileName << "->" << id;
                emit uploadFinished(true, id, fileName);
            } else {
                emit uploadFinished(false, "", fileName);
            }
        } else {
            qWarning() << "[ApiClient] Upload failed:" << reply->errorString();
            emit uploadFinished(false, "", fileName);
        }
        reply->deleteLater();
    });
}

void ApiClient::downloadFile(const QString& remoteId, const QString& savePath) {
    QNetworkReply* reply = apiGet("/api/download/" + remoteId);

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, savePath]() {
        // 流式写入
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            file.write(reply->readAll());
            file.close();
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 recv, qint64 total) { emit downloadProgress(recv, total); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, savePath]() {
        emit downloadFinished(reply->error() == QNetworkReply::NoError, savePath);
        reply->deleteLater();
    });
}

void ApiClient::listServerFiles() {
    QNetworkReply* reply = apiGet("/api/files");

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray raw = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(raw);
            QList<RemoteSongInfo> files;

            if (doc.isArray()) {
                // /api/files 直接返回数组
                QJsonArray arr = doc.array();
                for (const QJsonValue &val : arr) {
                    files.append(RemoteSongInfo::fromJson(val.toObject()));
                }
            } else if (doc.isObject()) {
                // 或包装在 {"data": [...]} 中
                QJsonObject obj = doc.object();
                QJsonArray arr = obj["data"].toArray();
                if (arr.isEmpty()) {
                    // 也可能是 顶层 data 不存在，fields 直接在顶层
                    QJsonArray topArr = obj["files"].toArray();
                    for (const QJsonValue &val : topArr) {
                        files.append(RemoteSongInfo::fromJson(val.toObject()));
                    }
                } else {
                    for (const QJsonValue &val : arr) {
                        files.append(RemoteSongInfo::fromJson(val.toObject()));
                    }
                }
            }

            qDebug() << "[ApiClient] Files received:" << files.size();
            emit serverFilesReceived(files);
        } else {
            qWarning() << "[ApiClient] listFiles failed:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void ApiClient::searchServer(const QString& query) {
    QNetworkReply* reply = apiGet("/api/search?q=" + QUrl::toPercentEncoding(query));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray raw = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(raw);
            QList<RemoteSongInfo> results;

            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const QJsonValue &val : arr) {
                    results.append(RemoteSongInfo::fromJson(val.toObject()));
                }
            } else if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QJsonArray arr = obj["data"].toArray();
                for (const QJsonValue &val : arr) {
                    results.append(RemoteSongInfo::fromJson(val.toObject()));
                }
            }

            qDebug() << "[ApiClient] Search results:" << results.size();
            emit searchResultReceived(results);
        }
        reply->deleteLater();
    });
}

void ApiClient::deleteServerFile(const QString& remoteId) {
    QNetworkReply* reply = apiDelete("/api/files/" + remoteId);

    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteId]() {
        emit deleteFinished(reply->error() == QNetworkReply::NoError, remoteId);
        reply->deleteLater();
    });
}

void ApiClient::requestPlay(const QString& remoteId) {
    qDebug() << "[ApiClient] Requesting play for remote ID:" << remoteId;
    QNetworkReply* reply = apiGet("/api/play/" + remoteId);

    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteId]() {
        QString httpUrl;
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray raw = reply->readAll();
            qDebug() << "[ApiClient] Play response raw:" << raw;
            bool ok;
            QJsonObject json = parseResponse(raw, ok);
            if (ok) {
                qDebug() << "[ApiClient] Play response JSON:" << json;
                int code = json["code"].toInt(200);
                if (code == 404 || code == 500) {
                    qWarning() << "[ApiClient] Play request failed with code:" << code;
                    emit streamUrlReady(remoteId, "");
                } else {
                    // 尝试多种可能的字段名和位置
                    QJsonObject dataObj = json["data"].toObject();
                    if (dataObj.contains("http_url")) {
                        httpUrl = dataObj["http_url"].toString();
                    } else if (dataObj.contains("stream_url")) {
                        httpUrl = dataObj["stream_url"].toString();
                    } else if (dataObj.contains("url")) {
                        httpUrl = dataObj["url"].toString();
                    }
                    
                    if (httpUrl.isEmpty() && json.contains("http_url")) {
                        httpUrl = json["http_url"].toString();
                    }
                    if (httpUrl.isEmpty() && json.contains("stream_url")) {
                        httpUrl = json["stream_url"].toString();
                    }
                    
                    qDebug() << "[ApiClient] Extracted HTTP URL:" << httpUrl;
                    emit streamUrlReady(remoteId, httpUrl);
                }
            } else {
                qWarning() << "[ApiClient] Failed to parse play response";
                emit streamUrlReady(remoteId, "");
            }
        } else {
            qWarning() << "[ApiClient] Play request network error:" << reply->errorString();
            emit streamUrlReady(remoteId, "");
            emit errorOccurred(QString("播放请求失败: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void ApiClient::requestStop(const QString& remoteId) {
    qDebug() << "[ApiClient] Requesting stop for remote ID:" << remoteId;
    QNetworkReply* reply = apiGet("/api/stop/" + remoteId);

    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteId]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[ApiClient] Stop request successful for:" << remoteId;
        } else {
            qWarning() << "[ApiClient] Stop request failed:" << reply->errorString();
        }
        emit stopFinished(reply->error() == QNetworkReply::NoError, remoteId);
        reply->deleteLater();
    });
}

void ApiClient::downloadLyrics(const QString& remoteId) {
    QNetworkReply* reply = apiGet("/api/lyrics/" + remoteId);

    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            // 尝试解析为JSON（Python服务器返回JSON格式）
            bool ok;
            QJsonObject json = parseResponse(data, ok);
            if (ok && json.contains("data")) {
                QJsonObject dataObj = json["data"].toObject();
                QString lrc = dataObj["lyrics"].toString();  // 服务器返回的字段名是 "lyrics"
                QString status = dataObj["status"].toString();
                
                if (!lrc.isEmpty()) {
                    emit lyricsReady(remoteId, lrc.toUtf8());
                } else {
                    // 歌词未生成，发送状态
                    emit lyricsStatus(remoteId, status.isEmpty() ? "none" : status);
                }
            } else {
                // 直接返回原始内容（可能是纯LRC文本）
                emit lyricsReady(remoteId, data);
            }
        } else {
            // 网络错误，发送状态
            emit lyricsStatus(remoteId, "none");
        }
        reply->deleteLater();
    });
}

void ApiClient::downloadCover(const QString& remoteId) {
    QNetworkReply* reply = apiGet("/api/cover/" + remoteId);

    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteId]() {
        if (reply->error() == QNetworkReply::NoError) {
            emit coverReady(remoteId, reply->readAll());
        }
        reply->deleteLater();
    });
}
