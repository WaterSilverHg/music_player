// config_manager.h: 配置文件管理器
// 读取/生成/保存 config.ini
// 测试环境 (_WIN32): 放在 exe 同级目录
// 生产环境: 放在 AppData 目录

#pragma once

#include "global.h"

struct ServerConfig {
    QString host;
    int httpPort;  // Python API服务器端口
    int rtspPort;   // ZLMediaKit RTSP端口
    int lyricsPort; // Python歌词服务器端口

    QString httpBaseUrl() const {
        return QString("http://%1:%2").arg(host).arg(httpPort);
    }

    QString lyricsBaseUrl() const {
        return QString("http://%1:%2").arg(host).arg(lyricsPort);
    }
};

struct LocalConfig {
    QString musicDir;
    bool autoScan;
};

class ConfigManager {
public:
    static ConfigManager& instance();

    // 加载配置（无文件时自动生成）
    bool load();

    // 保存配置
    bool save();

    // 配置路径
    QString configFilePath() const;

    // 配置数据
    ServerConfig server;
    LocalConfig local;

private:
    ConfigManager() = default;
    QString m_configPath;

    // 计算配置文件路径
    QString computeConfigPath() const;

    // 生成默认配置
    QString defaultConfigContent() const;

    // 解析 INI 内容
    void parseIniContent(const QString& content);
};
