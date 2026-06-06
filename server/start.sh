#!/bin/bash
# 音乐播放器服务器启动脚本 (Linux)

# 设置工作目录为脚本所在目录
cd "$(dirname "$0")"

# 检查虚拟环境是否存在
if [ ! -d "venv" ]; then
    echo "错误：虚拟环境目录 venv 不存在！"
    echo "请先创建虚拟环境并安装依赖："
    echo "python3 -m venv venv"
    echo "source venv/bin/activate"
    echo "pip install -r requirements.txt"
    exit 1
fi

# 激活虚拟环境
source venv/bin/activate

# 检查 Python 是否可用
if ! command -v python3 &> /dev/null; then
    echo "错误：未找到 python3！"
    exit 1
fi

# 启动服务器
echo "[$(date '+%Y-%m-%d %H:%M:%S')] 启动音乐播放器服务器..."
python3 main.py