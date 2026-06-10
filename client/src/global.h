#ifndef HEAD_H
#define HEAD_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QFileDialog>
#include <QDirIterator>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QList>
#include <QStringList>
#include <QAudioOutput>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QListWidget>
#include <QInputDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <QThread>
#include <QFuture>
#include <QtConcurrent>
#include <QUuid>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

// =====================================================
// 公共配置常量（上线时需修改的变量）
// =====================================================

// 默认服务器配置常量（上线时修改）
// 注意：与 config_manager.h 中的 ServerConfig struct 区分
namespace DefaultServerConfig {
    // API 服务器地址（上线时修改为实际服务器 IP）
    constexpr const char* DEFAULT_API_HOST = "127.0.0.1";
    constexpr int DEFAULT_API_PORT = 8080;
    
    // 歌词服务器端口
    constexpr int DEFAULT_LYRICS_PORT = 8080;
    
    // 超时设置（毫秒）
    constexpr int API_TIMEOUT = 30000;        // API 请求超时
    constexpr int HEARTBEAT_INTERVAL = 5000;  // 心跳间隔
}

// 默认客户端配置常量
namespace DefaultClientConfig {
    // 上传文件大小限制（字节）
    constexpr qint64 MAX_UPLOAD_SIZE = 30 * 1024 * 1024;  // 30MB
    
    // 播放连续失败最大次数
    constexpr int MAX_CONSECUTIVE_FAILURES = 3;
    
    // 加载歌曲超时时间（毫秒）
    constexpr int LOAD_SONG_TIMEOUT = 10000;
}

// 默认歌词生成配置常量
namespace DefaultLyricsConfig {
    // 歌词片段合并的最大时间间隔（秒）
    constexpr double MERGE_MAX_GAP = 0.12;
    
    // 歌词片段的最小长度（字符）
    constexpr int MERGE_MIN_LENGTH = 3;
}

#endif // HEAD_H
