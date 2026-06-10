# Music Player Lyrics Service

基于 FastAPI + Whisper 的音频转歌词服务。

## 功能特性

- 支持多种音频格式：MP3、MP4、M4A、FLAC、WAV、OGG、WMA
- 使用 Whisper 模型进行高精度语音识别
- 支持长音频分段处理（超过 10 分钟自动分段）
- 生成带时间戳的 LRC 格式歌词
- 自动检测音频语言
- 支持 CORS 跨域访问

## 安装步骤

### 1. 安装依赖

```bash
# 安装 Python 依赖
pip install -r requirements.txt

# 安装 ffmpeg（必需）
# Windows: 下载并添加到 PATH
# macOS: brew install ffmpeg
# Linux: sudo apt-get install ffmpeg
```

### 2. 安装 Whisper 模型

首次运行时会自动下载模型，也可以手动下载：

```bash
# 下载基础模型（约 1GB）
python -c "from faster_whisper import WhisperModel; model = WhisperModel('base', download_root='./models')"

# 可选模型大小：tiny, base, small, medium, large
# 越大的模型精度越高，但速度越慢
```

## 启动服务

```bash
# 开发模式
python main.py

# 生产模式（使用 uvicorn）
uvicorn main:app --host 0.0.0.0 --port 8080 --workers 4
```

服务启动后访问：http://localhost:8080/docs 查看 API 文档

## API 接口

### POST /upload_lyrics

上传音频文件并生成歌词。

**请求参数：**
- `audio`: 音频文件（multipart/form-data）
- `model_size`: 模型大小（可选，默认 base）

**响应示例：**
```json
{
    "status": "success",
    "lrc": "[00:00.00]Hello\n[00:02.34]World",
    "duration": 180.5,
    "language": "en",
    "message": "歌词生成成功"
}
```

### GET /lyrics/{file_id}

获取已生成的歌词。

### DELETE /lyrics/{file_id}

删除歌词文件。

### GET /health

健康检查接口。

## 配置说明

### config.ini 配置文件

```ini
[api]
secret=your_secret_key

[general]
host=0.0.0.0  # 生产环境改为 0.0.0.0 允许外部访问
http_port=8080

[whisper]
model_size=base
model_dir=./models

[lyrics]
merge_max_gap=0.12
merge_min_length=3
enabled=true
```

**上线部署时需修改：**
- `general/host`：生产环境改为 `0.0.0.0` 允许外部访问
- `api/secret`：修改为强随机密钥

### 环境变量（可选）

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| UPLOAD_DIR | uploads/ | 临时上传目录 |
| LYRICS_DIR | lyrics/ | 歌词存储目录 |
| MUSIC_DIR | ../music/ | 音乐文件目录 |
| MAX_FILE_SIZE | 500MB | 最大文件大小 |
| MAX_DURATION | 10 分钟 | 最大处理时长 |

## 注意事项

1. 首次运行会下载 Whisper 模型，需要网络连接
2. 建议使用 GPU 加速以提高转录速度
3. 长音频处理可能需要较长时间，请耐心等待
4. 确保 ffmpeg 已正确安装并添加到系统 PATH
5. 生产环境部署时请修改 `config.ini` 中的 `host` 为 `0.0.0.0`

## 可选：人声分离

如果需要更好的识别效果，可以使用 demucs 进行人声分离：

```bash
# 安装 demucs
pip install demucs

# 使用示例
python -m demucs.separate -n htdemucs --two-stems=vocals input.mp3 -o output/
```

在代码中集成人声分离：

```python
def extract_vocals(input_path: str, output_path: str):
    """使用 demucs 提取人声"""
    from demucs.pretrained import get_model
    from demucs.apply import apply_model
    
    model = get_model('htdemucs')
    wav = load_audio(input_path)
    sources = apply_model(model, wav)
    save_audio(output_path, sources['vocals'])
```
