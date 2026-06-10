#!/bin/bash
# 音乐播放器服务器启动脚本 (Linux)

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "   Music Player Server Startup"
echo "========================================"
echo

# 设置工作目录为脚本所在目录
cd "$(dirname "$0")"

# 检查 Python 3.10 是否已安装（强制使用 python3.10）
echo -e "${YELLOW}[1/3] Checking Python 3.10...${NC}"
if ! command -v python3.10 &> /dev/null; then
    echo -e "${RED}[ERROR] Python 3.10 not found!${NC}"
    echo
    echo "Please install Python 3.10:"
    echo "  Ubuntu/Debian: sudo apt update && sudo apt install python3.10"
    echo "  CentOS/RHEL: sudo yum install python3.10"
    echo "  macOS (Homebrew): brew install python@3.10"
    echo
    echo "Or run the installation script:"
    echo "  ./install_lyrics_service.sh"
    exit 1
fi

PYTHON_VERSION=$(python3.10 --version | cut -d' ' -f2)
echo -e "${GREEN}Python $PYTHON_VERSION found.${NC}"
echo

# 检查虚拟环境是否存在
echo -e "${YELLOW}[2/3] Checking virtual environment...${NC}"
if [ ! -d "venv" ]; then
    echo -e "${RED}[ERROR] Virtual environment 'venv' not found!${NC}"
    echo
    echo "Please run the installation script first:"
    echo "  chmod +x install_lyrics_service.sh"
    echo "  ./install_lyrics_service.sh"
    exit 1
fi

echo -e "${GREEN}Virtual environment found.${NC}"
echo

# 激活虚拟环境并启动服务器
echo -e "${YELLOW}[3/3] Starting server...${NC}"
source venv/bin/activate

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Starting Music Player Server..."
python3 main.py