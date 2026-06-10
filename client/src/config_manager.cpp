// config_manager.cpp: 配置文件管理器实现

#include "config_manager.h"

ConfigManager& ConfigManager::instance() {
    static ConfigManager mgr;
    return mgr;
}

QString ConfigManager::computeConfigPath() const {
#ifdef _WIN32
    // 测试环境：exe 同级目录
    return QCoreApplication::applicationDirPath() + "/config.ini";
#else
    // 生产环境：AppData
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData + "/config.ini";
#endif
}

QString ConfigManager::configFilePath() const {
    return m_configPath;
}

QString ConfigManager::defaultConfigContent() const {
    return QString(
        "[server]\n"
        "host=%1\n"
        "http_port=%2\n"
        "rtsp_port=554\n"
        "lyrics_port=%3\n"
        "\n"
        "[local]\n"
        "music_dir=./music\n"
        "auto_scan=true\n"
    ).arg(DefaultServerConfig::DEFAULT_API_HOST)
     .arg(DefaultServerConfig::DEFAULT_API_PORT)
     .arg(DefaultServerConfig::DEFAULT_LYRICS_PORT);
}

void ConfigManager::parseIniContent(const QString& content) {
    // 设置默认值（仅当配置文件中未指定时使用）
    server.host = DefaultServerConfig::DEFAULT_API_HOST;
    server.httpPort = DefaultServerConfig::DEFAULT_API_PORT;
    server.rtspPort = 554;
    server.lyricsPort = DefaultServerConfig::DEFAULT_LYRICS_PORT;
    local.musicDir = "./music";
    local.autoScan = true;

    QString currentSection;
    QStringList lines = content.split('\n');

    for (auto& line : lines) {
        QString trimmed = line.trimmed();

        // 跳过注释和空行
        if (trimmed.isEmpty() || trimmed.startsWith(';') || trimmed.startsWith('#'))
            continue;

        // 匹配节
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            currentSection = trimmed.mid(1, trimmed.length() - 2).trimmed();
            continue;
        }

        // 匹配 key=value
        int eqPos = trimmed.indexOf('=');
        if (eqPos < 0) continue;

        QString key = trimmed.left(eqPos).trimmed();
        QString value = trimmed.mid(eqPos + 1).trimmed();

        if (currentSection == "server") {
            if (key == "host") server.host = value;
            else if (key == "http_port") server.httpPort = value.toInt();
            else if (key == "rtsp_port") server.rtspPort = value.toInt();
            else if (key == "lyrics_port") server.lyricsPort = value.toInt();
        } else if (currentSection == "local") {
            if (key == "music_dir") local.musicDir = value;
            else if (key == "auto_scan") local.autoScan = (value.toLower() == "true" || value == "1");
        }
    }

    // 验证端口范围
    if (server.httpPort < 1 || server.httpPort > 65535) server.httpPort = 8080;
    if (server.rtspPort < 1 || server.rtspPort > 65535) server.rtspPort = 554;
    if (server.lyricsPort < 1 || server.lyricsPort > 65535) server.lyricsPort = 8080;
}

bool ConfigManager::load() {
    m_configPath = computeConfigPath();

    // 如果文件不存在，自动生成
    if (!QFile::exists(m_configPath)) {
        qDebug() << "[Config] 配置文件不存在，自动生成:" << m_configPath;
        QFile file(m_configPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << defaultConfigContent();
            file.close();
            qDebug() << "[Config] 默认配置已生成";
        }
    }

    // 读取配置
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[Config] 无法打开配置文件:" << m_configPath;
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    parseIniContent(content);

    qDebug() << "[Config] 配置已加载:" << m_configPath;
    qDebug() << "  Server:" << server.httpBaseUrl();
    qDebug() << "  RTSP port:" << server.rtspPort;
    qDebug() << "  Lyrics port:" << server.lyricsPort;
    qDebug() << "  Local dir:" << local.musicDir;

    return true;
}

bool ConfigManager::save() {
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[Config] 无法保存配置文件:" << m_configPath;
        return false;
    }

    QString content = QString(
        "[server]\n"
        "host=%1\n"
        "http_port=%2\n"
        "rtsp_port=%3\n"
        "lyrics_port=%4\n"
        "\n"
        "[local]\n"
        "music_dir=%5\n"
        "auto_scan=%6\n"
    ).arg(server.host)
     .arg(server.httpPort)
     .arg(server.rtspPort)
     .arg(server.lyricsPort)
     .arg(local.musicDir)
     .arg(local.autoScan ? "true" : "false");

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    file.close();

    qDebug() << "[Config] 配置已保存:" << m_configPath;
    return true;
}
