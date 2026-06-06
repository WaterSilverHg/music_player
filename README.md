# 音乐播放器 (Music Player)

一个基于 Qt6 和 FastAPI 的现代化音乐播放器，支持本地音乐播放和服务器端音乐点播。

## ✨ 功能特点

### 客户端功能
- 🎵 **本地音乐播放**：支持 MP3、MP4、FLAC、WAV 等多种音频格式
- 🌐 **服务器音乐点播**：通过 HTTP 流式播放服务器端音乐
- 📋 **独立播放列表**：本地和服务器模式拥有独立的播放列表
- 🎤 **歌词显示**：实时同步显示歌词，自动滚动到当前行
- ⏯️ **播放控制**：支持播放、暂停、上一首、下一首、随机播放、循环播放
- 🔊 **音量控制**：支持音量调节
- ⏱️ **进度控制**：支持拖动进度条跳转
- 📁 **文件管理**：支持添加单个文件或整个文件夹
- 🔍 **搜索功能**：支持在服务器模式下搜索音乐
- 📥 **下载功能**：支持从服务器下载音乐到本地

### 服务器功能
- 📤 **文件上传**：支持上传音乐文件（最大 500MB）
- 🎶 **歌词生成**：使用 Whisper AI 自动生成歌词
- 📡 **HTTP 流式播放**：支持断点续传和随机访问
- 💾 **元数据管理**：自动管理歌曲信息和歌词状态
- 🔄 **异步任务**：歌词生成为后台任务，不阻塞主服务

## 🛠️ 技术栈

### 客户端
- **框架**: Qt 6.8+ (Core, Widgets, Multimedia, Network)
- **语言**: C++17
- **媒体播放**: QMediaPlayer
- **构建工具**: CMake

### 服务器
- **框架**: FastAPI
- **语言**: Python 3.10+
- **歌词生成**: OpenAI Whisper
- **音频处理**: pydub (ffmpeg)
- **构建工具**: Uvicorn

## 🚀 快速开始

### 环境要求

**客户端**
- Qt 6.8+
- CMake 3.16+
- Windows/Linux/macOS

**服务器**
- Python 3.10+（必须）
- FFmpeg（用于音频处理）

### 服务器端部署

1. **克隆项目**
```bash
git clone https://github.com/WaterSilverHg/music_player.git
cd music_player/server
```

2. **安装依赖（自动安装脚本）**
```bash
# Linux/Mac
chmod +x install_lyrics_service.sh
./install_lyrics_service.sh

# Windows
install_lyrics_service.bat
```

安装脚本会自动：
- 检查 Python 3.10+ 是否已安装
- 创建虚拟环境
- 安装所有依赖（PyTorch、Whisper、FastAPI 等）

3. **手动安装 FFmpeg**
```bash
# Ubuntu/Debian
sudo apt update && sudo apt install ffmpeg

# macOS (Homebrew)
brew install ffmpeg

# Windows
# 下载 FFmpeg 并添加到 PATH
```

4. **启动服务器**
```bash
# Linux/Mac
chmod +x start.sh
./start.sh

# Windows
start.bat
```

服务器默认运行在 `http://localhost:8080`

### 客户端编译与运行

1. **打开项目**
```bash
cd music_player/client
```

2. **使用 Qt Creator 打开项目**
- 打开 `CMakeLists.txt`
- 配置构建套件（需要 Qt 6.8+）
- 构建项目

3. **运行客户端**
- 构建后运行生成的可执行文件
- 首次运行会自动创建 `config.ini` 配置文件

## ⚙️ 配置说明

### 客户端配置 (config.ini)
```ini
[Server]
url=http://127.0.0.1:8080
lyrics_port=8080

[Local]
music_dir=./music
```

### 服务器配置 (config.ini)
```ini
[api]
secret=your_secret_key

[general]
host=127.0.0.1
http_port=8080

[whisper]
# Whisper 模型名称：tiny, base, small, medium, large
model_size=base
model_dir=./models

[lyrics]
# 歌词片段合并的最大时间间隔（秒）
merge_max_gap=0.12
# 歌词片段的最小长度（字符）
merge_min_length=3
# 是否启用歌词自动生成
enabled=true
```

## 📖 使用说明

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

### 上传音乐到服务器
使用 API 上传：
```bash
curl -X POST "http://localhost:8080/api/upload" \
  -F "file=@your_music.mp3"
```

### 生成歌词
上传音乐后，服务器会自动在后台生成歌词。生成状态可通过 API 查询：
```bash
curl "http://localhost:8080/api/lyrics/status/{file_id}"
```

## 🔌 API 接口

### 文件管理
- `GET /api/files` - 获取服务器文件列表
- `POST /api/upload` - 上传音乐文件
- `DELETE /api/file/{file_id}` - 删除文件

### 播放接口
- `GET /api/play/{file_id}` - 获取播放 URL
- `GET /stream/{file_id}` - 流式播放

### 歌词接口
- `GET /api/lyrics/{file_id}` - 获取歌词
- `GET /api/lyrics/status/{file_id}` - 获取歌词生成状态

### 搜索接口
- `GET /api/search?q={keyword}` - 搜索歌曲

## 📁 项目结构

```
music_player/
├── client/                    # Qt 客户端
│   ├── src/                  # 源代码
│   ├── resources/            # 资源文件（样式表）
│   ├── picture/              # 图标资源
│   └── CMakeLists.txt        # CMake 配置
├── server/                   # FastAPI 服务器
│   ├── main.py               # 主入口
│   ├── config.ini            # 配置文件
│   ├── install_lyrics_service.sh  # Linux 安装脚本
│   ├── install_lyrics_service.bat # Windows 安装脚本
│   ├── start.sh              # Linux 启动脚本
│   └── start.bat             # Windows 启动脚本
│   ├── server_files/         # 上传的音乐文件
│   ├── lyrics/               # 生成的歌词文件
│   └── models/               # Whisper 模型
└── README.md                 # 项目说明
```

## 📝 开发说明

### 客户端开发
- 使用 Qt Creator 打开项目
- 修改 `mainwindow.ui` 设计界面
- 逻辑代码在 `src/mainwindow.cpp`

### 服务器开发
- 修改 `main.py` 添加新功能
- 配置文件 `config.ini` 支持热修改
- 模型配置在 `[whisper]` 部分

## 📄 许可证

MIT License

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

项目地址：https://github.com/WaterSilverHg/music_player