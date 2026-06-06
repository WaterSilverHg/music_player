// lyricsclient.cpp: Python歌词服务器API客户端实现

#include "lyricsclient.h"

LyricsClient* LyricsClient::s_instance = nullptr;

LyricsClient* LyricsClient::instance() {
    if (!s_instance) {
        s_instance = new LyricsClient();
    }
    return s_instance;
}

LyricsClient::LyricsClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void LyricsClient::init(const QString& baseUrl) {
    m_baseUrl = baseUrl;
    while (m_baseUrl.endsWith('/')) m_baseUrl.chop(1);
    qDebug() << "[LyricsClient] Initialized, base URL:" << m_baseUrl;
    
    // 启动健康检查
    healthCheck();
}

// ---- 通用请求 ----

QNetworkReply* LyricsClient::apiGet(const QString& path) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return m_nam->get(req);
}

QNetworkReply* LyricsClient::apiPostMultipart(const QString& path, QHttpMultiPart* multiPart) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    return m_nam->post(req, multiPart);
}

QNetworkReply* LyricsClient::apiDelete(const QString& path) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    return m_nam->deleteResource(req);
}

QJsonObject LyricsClient::parseJson(const QByteArray& data, bool& ok) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[LyricsClient] JSON parse error:" << err.errorString();
        ok = false;
        return {};
    }
    ok = true;
    return doc.object();
}

// ---- 健康检查 ----

void LyricsClient::healthCheck() {
    QNetworkReply* reply = apiGet("/health");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHealthReply(reply);
    });
}

void LyricsClient::onHealthReply(QNetworkReply* reply) {
    bool wasConnected = m_connected;
    
    if (reply->error() == QNetworkReply::NoError) {
        bool ok;
        QJsonObject json = parseJson(reply->readAll(), ok);
        if (ok) {
            bool whisperAvailable = json["whisper_available"].toBool(false);
            bool modelLoaded = json["model_loaded"].toBool(false);
            m_connected = true;
            
            if (!wasConnected) emit connected();
            emit healthReceived(true, whisperAvailable, modelLoaded);
        } else {
            m_connected = false;
            if (wasConnected) emit disconnected();
        }
    } else {
        m_connected = false;
        if (wasConnected) emit disconnected();
        qWarning() << "[LyricsClient] Health check failed:" << reply->errorString();
    }
    reply->deleteLater();
}

// ---- 上传音频并生成歌词 ----

void LyricsClient::uploadAndGenerateLyrics(const QString& localFilePath, const QString& modelSize) {
    QFileInfo fi(localFilePath);
    if (!fi.exists()) {
        emit errorOccurred(QString("文件不存在: %1").arg(localFilePath));
        emit lyricsGenerated(false, "", 0.0, "", "文件不存在");
        return;
    }

    // 构建 multipart 请求
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 文件部分
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"audio\"; filename=\"%1\"").arg(fi.fileName()));

    QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(localFilePath).name();
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);

    QFile* file = new QFile(localFilePath);
    file->open(QIODevice::ReadOnly);
    filePart.setBodyDevice(file);
    file->setParent(multiPart);

    multiPart->append(filePart);

    // 模型大小部分
    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"model_size\"");
    modelPart.setBody(modelSize.toUtf8());
    multiPart->append(modelPart);

    // 发送请求
    QNetworkReply* reply = apiPostMultipart("/upload_lyrics", multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::uploadProgress, this,
            [this](qint64 sent, qint64 total) { emit uploadProgress(sent, total); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, fileName = fi.fileName()]() {
        if (reply->error() == QNetworkReply::NoError) {
            bool ok;
            QJsonObject json = parseJson(reply->readAll(), ok);
            if (ok) {
                QString status = json["status"].toString();
                QString lrc = json["lrc"].toString();
                double duration = json["duration"].toDouble();
                QString language = json["language"].toString();
                QString message = json["message"].toString();
                
                if (status == "success") {
                    qDebug() << "[LyricsClient] Lyrics generated for:" << fileName;
                    emit lyricsGenerated(true, lrc, duration, language, message);
                } else {
                    qWarning() << "[LyricsClient] Lyrics generation failed:" << message;
                    emit lyricsGenerated(false, "", 0.0, "", message);
                }
            } else {
                emit lyricsGenerated(false, "", 0.0, "", "响应解析失败");
            }
        } else {
            qWarning() << "[LyricsClient] Upload failed:" << reply->errorString();
            emit lyricsGenerated(false, "", 0.0, "", reply->errorString());
        }
        reply->deleteLater();
    });
}

// ---- 获取歌词 ----

void LyricsClient::getLyrics(const QString& fileId) {
    QNetworkReply* reply = apiGet("/lyrics/" + fileId);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]() {
        if (reply->error() == QNetworkReply::NoError) {
            bool ok;
            QJsonObject json = parseJson(reply->readAll(), ok);
            if (ok) {
                QString lrc = json["lrc"].toString();
                emit lyricsReceived(true, lrc);
            } else {
                emit lyricsReceived(false, "");
            }
        } else {
            qWarning() << "[LyricsClient] Get lyrics failed:" << reply->errorString();
            emit lyricsReceived(false, "");
        }
        reply->deleteLater();
    });
}

// ---- 删除歌词 ----

void LyricsClient::deleteLyrics(const QString& fileId) {
    QNetworkReply* reply = apiDelete("/lyrics/" + fileId);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]() {
        bool success = (reply->error() == QNetworkReply::NoError);
        if (success) {
            qDebug() << "[LyricsClient] Lyrics deleted:" << fileId;
        } else {
            qWarning() << "[LyricsClient] Delete lyrics failed:" << reply->errorString();
        }
        emit lyricsDeleted(success);
        reply->deleteLater();
    });
}
