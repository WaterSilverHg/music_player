# Music Player Client

基于 Qt6 的跨平台音乐播放器客户端。

## 功能特性

- 🎵 本地音乐播放（支持 MP3、MP4、FLAC、WAV 等格式）
- 🌐 服务器音乐点播（HTTP 流式播放）
- 📋 独立播放列表管理（本地/服务器模式）
- 🎤 实时歌词显示与同步
- ⏯️ 完整的播放控制（播放、暂停、上一首、下一首、随机、循环）
- 🔍 搜索功能
- 📥 下载服务器音乐
- 📤 上传音乐到服务器

## 技术栈

- **框架**: Qt 6.8+ (Core, Widgets, Multimedia, Network)
- **语言**: C++17
- **媒体播放**: QMediaPlayer
- **构建工具**: CMake
- **网络通信**: QNetworkAccessManager

## 编译与运行

### 环境要求

- Qt 6.8+
- CMake 3.16+
- FFmpeg（用于音频解码）
- Windows/Linux/macOS

### 编译步骤

1. **打开项目**
```bash
cd music_player/client
```

2. **使用 Qt Creator 打开**
- 打开 `CMakeLists.txt`
- 配置构建套件（需要 Qt 6.8+）
- 构建项目

3. **命令行编译**
```bash
# 创建构建目录
mkdir build && cd build

# 配置（Debug 模式）
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build . --config Debug

# 运行
./Debug/MusicPlayerClient  # Linux/Mac
Debug\MusicPlayerClient.exe  # Windows
```

## 配置说明

### config.ini 配置文件

首次运行时会自动创建配置文件：

```ini
[Server]
url=http://127.0.0.1:8080
lyrics_port=8080

[Local]
music_dir=./music
```

**上线部署时需修改：**
- `Server/url`：修改为实际服务器 IP 地址和端口

### 常量配置 (global.h)

```cpp
// 服务器配置（上线时修改）
namespace DefaultServerConfig {
    constexpr const char* DEFAULT_API_HOST = "127.0.0.1";  // 改为实际服务器 IP
    constexpr int DEFAULT_API_PORT = 8080;
    constexpr int DEFAULT_LYRICS_PORT = 8080;
    
    // 超时设置
    constexpr int API_TIMEOUT = 30000;        // API 请求超时
    constexpr int HEARTBEAT_INTERVAL = 5000;  // 心跳间隔
}

// 客户端配置
namespace DefaultClientConfig {
    constexpr qint64 MAX_UPLOAD_SIZE = 30 * 1024 * 1024;  // 上传大小限制
    constexpr int MAX_CONSECUTIVE_FAILURES = 3;          // 最大连续失败次数
    constexpr int LOAD_SONG_TIMEOUT = 10000;             // 加载超时
}
```

**上线部署时需修改：**
- `DefaultServerConfig::DEFAULT_API_HOST`：修改为实际服务器 IP 地址

## 使用说明

### 模式切换

- **本地模式**：播放本地音乐文件
- **服务器模式**：播放服务器端音乐

切换模式时会自动清空当前播放列表并加载对应模式的歌曲。

### 添加本地音乐

1. 切换到本地模式
2. 点击菜单栏「文件」→「添加文件」或「添加文件夹」

### 播放服务器音乐

1. 切换到服务器模式
2. 等待服务器连接成功
3. 点击播放列表中的歌曲

### 上传音乐

1. 切换到服务器模式
2. 点击菜单栏「文件」→「上传文件」
3. 填写歌曲信息（标题、歌手、专辑等）
4. 选择封面图片（可选）

### 搜索音乐

在服务器模式下，使用搜索框搜索音乐。

## 项目结构

```
client/
├── src/                    # 源代码
│   ├── main.cpp           # 程序入口
│   ├── mainwindow.cpp/h   # 主窗口
│   ├── mainwindow.ui      # 主窗口 UI
│   ├── apiclient.cpp/h    # API 客户端
│   ├── config_manager.cpp/h  # 配置管理器
│   ├── global.h           # 全局头文件（公共常量）
│   ├── Playlist.cpp/h     # 播放列表
│   ├── lyrics_parser.cpp/h  # 歌词解析器
│   ├── upload_dialog.cpp/h  # 上传对话框
│   └── upload_dialog.ui   # 上传对话框 UI
├── resources/             # 资源文件
│   └── styles.qss         # 样式表
├── picture/               # 图标资源
└── CMakeLists.txt         # CMake 配置
```

## 核心模块

### ApiClient
- 基于 `QNetworkAccessManager` 的异步 HTTP 通信
- 单例模式，提供所有服务器 API 调用
- 自动心跳保活

### ConfigManager
- 管理 `config.ini` 配置文件
- 自动创建默认配置
- 支持热加载

### MainWindow
- 主窗口逻辑
- 播放控制
- 播放列表管理
- 歌词显示

### Playlist
- 播放列表数据模型
- 支持添加、删除、重命名
- 文件持久化

## 开发说明

### 添加新 API

在 `ApiClient` 中添加新方法：

```cpp
// apiclient.h
void newApiCall(const QString& param);

// apiclient.cpp
void ApiClient::newApiCall(const QString& param) {
    QNetworkReply* reply = apiGet("/api/new/" + param);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 处理响应
    });
}
```

### 修改 UI

1. 编辑 `mainwindow.ui` 设计界面
2. 在 `mainwindow.cpp` 中添加逻辑
3. 在 `resources/styles.qss` 中调整样式

## 注意事项

1. 首次运行会自动创建 `config.ini` 配置文件
2. 播放列表数据保存在 `playlists/` 目录
3. 服务器模式需要网络连接
4. 上传文件最大 30MB

## 调试技巧

- Debug 模式下会输出详细日志到控制台
- 使用 `qDebug()` 宏添加调试信息
- 网络请求日志在 `ApiClient` 中

## 常见问题

**Q: 无法连接服务器？**
A: 检查 `config.ini` 中的服务器地址是否正确，确保服务器已启动。

**Q: 播放失败？**
A: 检查文件格式是否支持，确保 FFmpeg 已正确安装。

**Q: 歌词不显示？**
A: 检查歌词文件是否存在，格式是否正确（LRC 格式）。
