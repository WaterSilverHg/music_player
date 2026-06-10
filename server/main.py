import os
import uuid
import shutil
import json
import subprocess
import threading
import asyncio
import configparser
from datetime import datetime
from pathlib import Path
from typing import Optional, Tuple, List, Dict, Any
from concurrent.futures import ThreadPoolExecutor

from fastapi import FastAPI, File, UploadFile, HTTPException, Query, Response, Form
from fastapi.responses import FileResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware

# 使用原版 Whisper
try:
    import whisper
    WHISPER_AVAILABLE = True
except ImportError:
    WHISPER_AVAILABLE = False

app = FastAPI(title="Music Player Lyrics Service", version="1.0.0")

# CORS 配置
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 加载配置文件（使用 UTF-8 编码）
config = configparser.ConfigParser()
config.read("config.ini", encoding="utf-8")

# Whisper 配置
WHISPER_MODEL_SIZE = config.get("whisper", "model_size", fallback="base")
WHISPER_MODEL_DIR = Path(config.get("whisper", "model_dir", fallback="./models"))

# 歌词配置
LYRICS_ENABLED = config.getboolean("lyrics", "enabled", fallback=True)
LYRICS_MERGE_MAX_GAP = config.getfloat("lyrics", "merge_max_gap", fallback=0.12)
LYRICS_MERGE_MIN_LENGTH = config.getint("lyrics", "merge_min_length", fallback=3)

# 服务器配置
HTTP_PORT = config.getint("general", "http_port", fallback=8080)

# 配置
UPLOAD_DIR = Path("uploads")
LYRICS_DIR = Path("lyrics")
COVERS_DIR = Path("covers")
MUSIC_DIR = Path("../music")
SERVER_FILES_DIR = Path("server_files")
MAX_FILE_SIZE = 500 * 1024 * 1024  # 500MB
MAX_DURATION = 10 * 60  # 10分钟
SUPPORTED_FORMATS = {"mp3", "mp4", "m4a", "flac", "wav", "ogg", "wma"}
SEGMENT_DURATION = 300  # 每段5分钟

# 确保目录存在
UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
SERVER_FILES_DIR = Path("server_files")
SERVER_FILES_DIR.mkdir(parents=True, exist_ok=True)
LYRICS_DIR.mkdir(parents=True, exist_ok=True)
MUSIC_DIR.mkdir(parents=True, exist_ok=True)
METADATA_FILE = Path("metadata.json")

# 后台执行器
EXECUTOR = ThreadPoolExecutor(max_workers=2)

# 异步生成歌词的后台任务
def generate_lyrics_async(file_id: str, file_path: str):
    """在后台异步生成歌词"""
    try:
        # 更新状态为生成中（同时更新 files 和 songs）
        metadata = load_metadata()
        if file_id in metadata.get("files", {}):
            metadata["files"][file_id]["lyrics_status"] = "generating"
            metadata["songs"] = list(metadata["files"].values())
            save_metadata(metadata)
        
        print(f"[{datetime.now()}] Starting lyrics generation for {file_id}")
        
        # 转录音频（使用配置文件中的模型）
        result = transcribe_audio(file_path, WHISPER_MODEL_SIZE)
        
        # 合并短片段（使用配置文件中的参数）
        if result.get("segments"):
            result["segments"] = merge_segments(result["segments"], 
                                                max_gap=LYRICS_MERGE_MAX_GAP, 
                                                min_len=LYRICS_MERGE_MIN_LENGTH)
        
        # 生成 LRC
        lrc_text = generate_lrc(result)
        
        # 保存 LRC 文件
        lrc_file = LYRICS_DIR / f"{file_id}.lrc"
        with open(lrc_file, "w", encoding="utf-8") as f:
            f.write(lrc_text)
        
        # 更新状态为已生成（同时更新 files 和 songs）
        metadata = load_metadata()
        if file_id in metadata.get("files", {}):
            metadata["files"][file_id]["lyrics_status"] = "generated"
            metadata["songs"] = list(metadata["files"].values())
            save_metadata(metadata)
        
        print(f"[{datetime.now()}] Lyrics generated for {file_id}")
        
    except Exception as e:
        print(f"[{datetime.now()}] Error generating lyrics for {file_id}: {e}")
        # 更新状态为错误（同时更新 files 和 songs）
        metadata = load_metadata()
        if file_id in metadata.get("files", {}):
            metadata["files"][file_id]["lyrics_status"] = "error"
            metadata["songs"] = list(metadata["files"].values())
            save_metadata(metadata)

def load_metadata() -> Dict[str, Any]:
    if METADATA_FILE.exists():
        try:
            with open(METADATA_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"[{datetime.now()}] Error loading metadata: {e}")
    return {"files": {}, "songs": []}

def save_metadata(metadata: Dict[str, Any]) -> None:
    try:
        with open(METADATA_FILE, "w", encoding="utf-8") as f:
            json.dump(metadata, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"[{datetime.now()}] Error saving metadata: {e}")

# Whisper 模型缓存（线程安全）
whisper_model = None
model_lock = __import__('threading').Lock()

def init_whisper(model_size: str = "base") -> bool:
    """初始化 Whisper 模型，线程安全，支持切换模型尺寸"""
    global whisper_model
    if not WHISPER_AVAILABLE:
        return False
    
    with model_lock:
        # 如果当前模型不是请求的尺寸，则重新加载
        if whisper_model is None or getattr(whisper_model, 'model_size', None) != model_size:
            print(f"[{datetime.now()}] Loading Whisper model: {model_size}")
            try:
                print(f"[{datetime.now()}] Loading Whisper model: {model_size} from {WHISPER_MODEL_DIR}")
                whisper_model = whisper.load_model(model_size, download_root=str(WHISPER_MODEL_DIR))
                whisper_model.model_size = model_size  # 记录当前模型大小
                print(f"[{datetime.now()}] Whisper model loaded successfully")
                return True
            except Exception as e:
                import traceback
                print(f"[{datetime.now()}] Failed to load Whisper model: {e}")
                print(f"[{datetime.now()}] Full error traceback: {traceback.format_exc()}")
                return False
    return True

def format_timestamp(seconds: float) -> str:
    """将秒转换为 LRC 时间戳格式 [mm:ss.xx]"""
    minutes = int(seconds // 60)
    secs = int(seconds % 60)
    centiseconds = int((seconds % 1) * 100)
    return f"[{minutes:02d}:{secs:02d}.{centiseconds:02d}]"

def get_audio_duration(file_path: str) -> float:
    """获取音频时长（使用 ffprobe）"""
    try:
        result = subprocess.run(
            ["ffprobe", "-v", "quiet", "-print_format", "json", "-show_streams", file_path],
            capture_output=True,
            text=True,
            timeout=30
        )
        info = json.loads(result.stdout)
        for stream in info.get("streams", []):
            if stream.get("codec_type") == "audio":
                return float(stream.get("duration", 0))
    except Exception as e:
        print(f"[{datetime.now()}] Error getting duration: {e}")
    return 0

def split_audio(file_path: str, segment_duration: int = SEGMENT_DURATION) -> List[Tuple[str, float]]:
    """
    分割音频文件
    返回: 分割后的文件列表 [(split_file_path, offset_seconds), ...]
          如果不需要分割，返回空列表
    """
    duration = get_audio_duration(file_path)
    if duration <= segment_duration or duration == 0:
        return []  # 不需要分割
    
    segments = []
    base_name = os.path.splitext(os.path.basename(file_path))[0]
    temp_dir = UPLOAD_DIR / f"split_{uuid.uuid4().hex[:8]}"
    temp_dir.mkdir(exist_ok=True)
    
    try:
        for i in range(0, int(duration), segment_duration):
            output_file = temp_dir / f"{base_name}_part_{i}.wav"
            start_time = i
            end_time = min(i + segment_duration, duration)
            
            cmd = [
                "ffmpeg", "-i", file_path,
                "-ss", str(start_time),
                "-to", str(end_time),
                "-acodec", "pcm_s16le",
                "-ar", "16000",
                "-ac", "1",
                str(output_file),
                "-y", "-v", "quiet"
            ]
            
            subprocess.run(cmd, timeout=120)
            
            if output_file.exists():
                segments.append((str(output_file), float(start_time)))
        
        return segments
    except Exception as e:
        print(f"[{datetime.now()}] Error splitting audio: {e}")
        # 清理已创建的临时文件
        if temp_dir.exists():
            shutil.rmtree(temp_dir, ignore_errors=True)
        return []

def transcribe_audio(file_path: str, model_size: str = "base") -> Dict[str, Any]:
    """转录音频文件，返回标准化结果（使用原版 Whisper）"""
    if not init_whisper(model_size):
        raise RuntimeError("Whisper 模型初始化失败")
    
    result = {
        "language": "",
        "duration": get_audio_duration(file_path),
        "segments": []
    }
    
    # 使用锁保护模型调用，防止并发冲突
    with model_lock:
        try:
            # 使用原版 Whisper 转录（关闭单词时间戳，使用句子级输出）
            result_raw = whisper_model.transcribe(
                file_path,
                word_timestamps=False,  # 改为 False，避免过度细分
                verbose=False
            )
            
            result["language"] = result_raw.get("language", "")
            result["segments"] = result_raw.get("segments", [])
            
        except Exception as e:
            print(f"[{datetime.now()}] Error transcribing audio: {e}")
            raise
    
    return result

def merge_segments(segments, max_gap=0.3, min_len=2):
    """
    合并时间间隔小于 max_gap 秒的相邻片段，且合并后的文本长度至少为 min_len 个字符。
    返回合并后的片段列表。
    """
    if not segments:
        return []
    
    merged = []
    current = segments[0].copy()
    
    for seg in segments[1:]:
        gap = seg["start"] - current["end"]
        # 时间间隔小则合并
        if gap < max_gap:
            current["text"] += seg["text"]
            current["end"] = seg["end"]
        else:
            # 检查当前片段是否有效
            if len(current["text"].strip()) >= min_len:
                merged.append(current)
            current = seg.copy()
    
    # 最后一个片段
    if len(current["text"].strip()) >= min_len:
        merged.append(current)
    
    return merged

def clean_text(text):
    """清理文本中的乱码和控制字符"""
    import re
    # 允许中日韩文字、英文字母、数字、常见标点
    # \u4e00-\u9fa5: 中文
    # \u3040-\u309F: 日文平假名
    # \u30A0-\u30FF: 日文片假名
    # \uAC00-\uD7AF: 韩文
    # a-zA-Z0-9: 英文和数字
    # 空格和常见标点符号
    cleaned = re.sub(r'[^\u4e00-\u9fa5\u3040-\u309F\u30A0-\u30FF\uAC00-\uD7AFa-zA-Z0-9 　、。，．！？【】（）「」『』・\-\']', '', text)
    return cleaned.strip()

def generate_lrc(transcription: Dict[str, Any]) -> str:
    """从转录结果生成 LRC 格式歌词"""
    lrc_lines = []
    
    # 添加元信息
    if transcription.get("language"):
        lrc_lines.append(f"[language:{transcription['language']}]")
    if transcription.get("duration"):
        lrc_lines.append(f"[duration:{transcription['duration']:.2f}]")
    
    # 添加歌词内容
    for segment in transcription.get("segments", []):
        if not segment.get("text") or segment["text"].strip() == "":
            continue
        
        start_time = segment["start"]
        text = clean_text(segment["text"].strip())  # 清理乱码字符
        
        if not text:  # 清理后文本为空，跳过
            continue
        
        # 尝试按单词分割（如果有单词时间戳）
        if segment.get("words") and len(segment["words"]) > 0:
            for word_info in segment["words"]:
                word_start = word_info.get("start", start_time)
                word_text = clean_text(word_info.get("word", "").strip())  # 清理乱码字符
                if word_text:
                    timestamp = format_timestamp(word_start)
                    lrc_lines.append(f"{timestamp}{word_text}")
        else:
            timestamp = format_timestamp(start_time)
            lrc_lines.append(f"{timestamp}{text}")
    
    return "\n".join(lrc_lines)

@app.get("/lyrics/{file_id}")
async def get_lyrics(file_id: str):
    """获取已生成的歌词（统一接口）"""
    lrc_file = LYRICS_DIR / f"{file_id}.lrc"
    if not lrc_file.exists():
        raise HTTPException(status_code=404, detail="歌词文件不存在")

    with open(lrc_file, "r", encoding="utf-8") as f:
        lrc_text = f.read()

    return {
        "code": 200,
        "message": "success",
        "data": {
            "lrc": lrc_text,
            "status": "generated"
        },
        "lrc": lrc_text,  # 保持向后兼容
        "status": "generated"  # 保持向后兼容
    }

@app.delete("/lyrics/{file_id}")
async def delete_lyrics(file_id: str):
    """删除歌词文件"""
    lrc_file = LYRICS_DIR / f"{file_id}.lrc"
    if not lrc_file.exists():
        raise HTTPException(status_code=404, detail="歌词文件不存在")
    
    os.remove(lrc_file)
    return {"status": "success", "message": "歌词文件已删除"}

@app.get("/health")
async def health_check():
    """健康检查"""
    return {
        "status": "healthy",
        "whisper_available": WHISPER_AVAILABLE,
        "model_loaded": whisper_model is not None
    }

# ============================================
# 文件管理 API (ApiClient 对应接口)
# ============================================

@app.get("/api/status")
async def api_status():
    """服务器状态检查"""
    metadata = load_metadata()
    return {
        "code": 200,
        "message": "success",
        "data": {
            "song_count": len(metadata.get("songs", [])),
            "status": "running"
        }
    }

@app.post("/api/upload")
async def api_upload(file: UploadFile = File(...), 
                    title: str = Form(None), 
                    artist: str = Form(None), 
                    album: str = Form(None),
                    cover: UploadFile = File(None)):
    """上传音乐文件到服务器（自动生成歌词）"""
    try:
        # 验证文件格式
        file_ext = file.filename.split(".")[-1].lower() if "." in file.filename else ""
        if not file_ext or file_ext not in SUPPORTED_FORMATS:
            return {"code": 400, "message": f"不支持的文件格式: {file_ext}"}
        
        # 检查文件名是否重复
        metadata = load_metadata()
        for song_id, song_info in metadata["files"].items():
            if song_info["name"] == file.filename:
                return {"code": 400, "message": f"文件已存在: {file.filename}"}
        
        # 生成文件ID
        file_id = uuid.uuid4().hex[:12]
        
        # 保存文件（流式保存，避免一次性加载到内存）
        server_file = SERVER_FILES_DIR / f"{file_id}.{file_ext}"
        file_size = 0
        with open(server_file, "wb") as f:
            while chunk := await file.read(8192):  # 每次读取8KB
                f.write(chunk)
                file_size += len(chunk)
        
        # 获取音频时长
        duration = get_audio_duration(str(server_file))
        
        # 保存封面图片（如果有）
        has_cover = False
        if cover and cover.filename:
            cover_ext = cover.filename.split(".")[-1].lower()
            if cover_ext in ["jpg", "jpeg", "png", "gif", "bmp"]:
                cover_path = COVERS_DIR / f"{file_id}.{cover_ext}"
                with open(cover_path, "wb") as f:
                    while chunk := await cover.read(8192):
                        f.write(chunk)
                has_cover = True
        
        # 更新元数据
        metadata = load_metadata()
        
        # 使用用户提供的元数据，否则使用默认值
        song_title = title.strip() if title else os.path.splitext(file.filename)[0]
        song_artist = artist.strip() if artist else "未知艺术家"
        song_album = album.strip() if album else "未知专辑"
        
        song_info = {
            "id": file_id,
            "name": file.filename,
            "title": song_title,
            "artist": song_artist,
            "album": song_album,
            "duration": duration,
            "format": file_ext,
            "size": file_size,
            "has_cover": has_cover,
            "lyrics_status": "pending",  # 等待生成
            "file_status": "ready",
            "upload_time": datetime.now().isoformat()
        }
        # 统一更新 files 和 songs，确保数据一致
        metadata["files"][file_id] = song_info
        metadata["songs"] = list(metadata["files"].values())  # songs 从 files 同步
        save_metadata(metadata)
        
        print(f"[{datetime.now()}] File uploaded: {file.filename} -> {file_id}")
        
        # 在后台异步生成歌词（仅当 Whisper 可用时）
        if WHISPER_AVAILABLE:
            EXECUTOR.submit(generate_lyrics_async, file_id, str(server_file))
            print(f"[{datetime.now()}] Lyrics generation started for {file_id}")
        else:
            print(f"[{datetime.now()}] Whisper not available, skipping lyrics generation")
        
        return {"code": 200, "message": "success", "id": file_id}
    
    except Exception as e:
        print(f"[{datetime.now()}] Upload error: {e}")
        return {"code": 500, "message": str(e)}

@app.get("/api/files")
async def api_list_files():
    """列出服务器所有文件"""
    metadata = load_metadata()
    songs = metadata.get("songs", [])
    return {"code": 200, "message": "success", "data": songs}

@app.get("/api/search")
async def api_search(q: str = Query(..., min_length=1)):
    """搜索服务器文件"""
    metadata = load_metadata()
    query_lower = q.lower()
    results = []
    
    for song in metadata.get("songs", []):
        if (query_lower in song.get("title", "").lower() or
            query_lower in song.get("artist", "").lower() or
            query_lower in song.get("album", "").lower() or
            query_lower in song.get("name", "").lower()):
            results.append(song)
    
    return {"code": 200, "message": "success", "data": results}

@app.delete("/api/files/{file_id}")
async def api_delete_file(file_id: str):
    """删除服务器文件"""
    metadata = load_metadata()
    
    if file_id not in metadata.get("files", {}):
        return {"code": 404, "message": "文件不存在"}
    
    # 获取文件路径并删除
    song = metadata["files"][file_id]
    file_path = SERVER_FILES_DIR / f"{file_id}.{song['format']}"
    
    if file_path.exists():
        os.remove(file_path)
    
    # 从元数据中移除
    del metadata["files"][file_id]
    metadata["songs"] = list(metadata["files"].values())
    save_metadata(metadata)
    
    # 同时删除歌词文件
    lrc_file = LYRICS_DIR / f"{file_id}.lrc"
    if lrc_file.exists():
        os.remove(lrc_file)
    
    print(f"[{datetime.now()}] File deleted: {file_id}")
    
    return {"code": 200, "message": "success"}

@app.get("/api/play/{file_id}")
async def api_play(file_id: str):
    """请求播放，返回 HTTP 点播 URL"""
    metadata = load_metadata()
    
    print(f"[{datetime.now()}] API play called for file_id: {file_id}")
    
    if file_id not in metadata.get("files", {}):
        print(f"[{datetime.now()}] File not found in metadata: {file_id}")
        return {"code": 404, "message": "文件不存在"}
    
    song = metadata["files"][file_id]
    
    # 获取文件路径
    file_ext = song.get("format", "mp3")
    file_path = SERVER_FILES_DIR / f"{file_id}.{file_ext}"
    
    print(f"[{datetime.now()}] File path: {file_path}")
    
    if not file_path.exists():
        print(f"[{datetime.now()}] File not exists: {file_path}")
        # 文件不存在，删除缓存中的记录
        print(f"[{datetime.now()}] Removing invalid file from metadata: {file_id}")
        del metadata["files"][file_id]
        # 同步更新 songs 列表
        metadata["songs"] = list(metadata["files"].values())
        save_metadata(metadata)
        return {"code": 404, "message": "文件不存在，已从缓存中删除"}
    
    # 返回 HTTP URL，客户端可以直接访问 /stream/{file_id} 进行流式播放
    http_url = f"http://127.0.0.1:8080/stream/{file_id}"
    
    print(f"[{datetime.now()}] Play request: {file_id} -> {http_url}")
    
    # 返回 HTTP URL
    response = {
        "code": 200,
        "message": "success",
        "data": {
            "http_url": http_url,
            "file_id": file_id,
            "file_name": song["name"]
        }
    }
    print(f"[{datetime.now()}] Returning response: {response}")
    
    return response

@app.get("/api/stop/{file_id}")
async def api_stop(file_id: str):
    """停止播放（HTTP 流式播放无需特殊停止操作）"""
    print(f"[{datetime.now()}] Stop request: {file_id}")
    return {"code": 200, "message": "success"}

@app.get("/stream/{file_id}")
async def stream_file(file_id: str):
    """HTTP 流式播放音频文件"""
    metadata = load_metadata()
    
    if file_id not in metadata.get("files", {}):
        raise HTTPException(status_code=404, detail="文件不存在")
    
    song = metadata["files"][file_id]
    file_ext = song.get("format", "mp3")
    file_path = SERVER_FILES_DIR / f"{file_id}.{file_ext}"
    
    if not file_path.exists():
        raise HTTPException(status_code=404, detail="文件不存在")
    
    # 根据文件扩展名设置正确的 MIME 类型
    mime_types = {
        "mp3": "audio/mpeg",
        "mp4": "audio/mp4",
        "m4a": "audio/mp4",
        "flac": "audio/flac",
        "wav": "audio/wav",
        "ogg": "audio/ogg",
        "wma": "audio/x-ms-wma"
    }
    
    media_type = mime_types.get(file_ext, "application/octet-stream")
    
    print(f"[{datetime.now()}] Streaming file: {file_id} ({media_type})")
    
    return FileResponse(
        path=file_path,
        media_type=media_type,
        filename=song["name"]
    )

@app.get("/api/download/{file_id}")
async def api_download(file_id: str):
    """下载服务器文件"""
    metadata = load_metadata()
    
    if file_id not in metadata.get("files", {}):
        raise HTTPException(status_code=404, detail="文件不存在")
    
    song = metadata["files"][file_id]
    file_path = SERVER_FILES_DIR / f"{file_id}.{song['format']}"
    
    if not file_path.exists():
        raise HTTPException(status_code=404, detail="文件不存在")
    
    print(f"[{datetime.now()}] Download request: {file_id}")
    
    return FileResponse(
        path=file_path,
        filename=song["name"],
        media_type="application/octet-stream"
    )

@app.get("/api/cover/{file_id}")
async def api_cover(file_id: str):
    """下载封面（如果存在）"""
    raise HTTPException(status_code=404, detail="暂无封面")

@app.get("/api/lyrics/{file_id}")
async def api_lyrics(file_id: str):
    """获取歌词（ApiClient使用）"""
    # 首先检查是否有已生成的歌词
    lrc_file = LYRICS_DIR / f"{file_id}.lrc"
    if lrc_file.exists():
        with open(lrc_file, "r", encoding="utf-8") as f:
            lrc_text = f.read()
        return {
            "code": 200,
            "message": "success",
            "data": {"lyrics": lrc_text, "status": "generated"}
        }

    # 检查文件是否存在
    metadata = load_metadata()
    if file_id not in metadata.get("files", {}):
        return {
            "code": 404,
            "message": "文件不存在",
            "data": {"lyrics": "", "status": "none"}
        }

    return {
        "code": 200,
        "message": "歌词未生成",
        "data": {"lyrics": "", "status": "none"}
    }

if __name__ == "__main__":
    import uvicorn
    import logging
    
    print(f"[{datetime.now()}] Starting Lyrics Service...")
    
    # 配置日志，过滤掉心跳等 INFO 级别的 uvicorn 日志
    log_config = {
        "version": 1,
        "disable_existing_loggers": False,
        "formatters": {
            "default": {
                "format": "%(asctime)s - %(levelname)s - %(message)s",
            },
        },
        "handlers": {
            "console": {
                "class": "logging.StreamHandler",
                "formatter": "default",
                "stream": "ext://sys.stdout",
            },
        },
        "loggers": {
            "uvicorn": {"level": "WARNING"},  # 只显示 WARNING 及以上级别
            "uvicorn.access": {"level": "WARNING"},
            "uvicorn.error": {"level": "INFO"},
        },
        "root": {
            "level": "INFO",
            "handlers": ["console"],
        },
    }
    
    # 启动时预加载 Whisper 模型
    if WHISPER_AVAILABLE and LYRICS_ENABLED:
        print(f"[{datetime.now()}] Preloading Whisper model: {WHISPER_MODEL_SIZE}")
        try:
            if init_whisper(WHISPER_MODEL_SIZE):
                print(f"[{datetime.now()}] Whisper model loaded successfully")
            else:
                print(f"[{datetime.now()}] Warning: Failed to preload Whisper model")
                print(f"[{datetime.now()}] Lyrics generation will be disabled")
        except Exception as e:
            print(f"[{datetime.now()}] Warning: Could not preload Whisper model: {e}")
            print(f"[{datetime.now()}] Lyrics generation will be disabled")
    else:
        if not WHISPER_AVAILABLE:
            print(f"[{datetime.now()}] Warning: Whisper not installed, lyrics generation will fail")
        else:
            print(f"[{datetime.now()}] Lyrics generation is disabled in config")
    
    uvicorn.run(app, host="0.0.0.0", port=HTTP_PORT, log_config=log_config)
