#!/bin/bash
# Lyrics Service Dependency Installer for Linux
# =============================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "   Lyrics Service Dependency Installer"
echo "========================================"
echo

# 1. 检查 Python 3.10+
echo -e "[1/7] ${YELLOW}Checking Python 3.10+...${NC}"
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}[ERROR] Python 3 not found.${NC}"
    echo "Please install Python 3.10+ from https://www.python.org/downloads/"
    exit 1
fi

PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
PYTHON_MAJOR=$(echo $PYTHON_VERSION | cut -d'.' -f1)
PYTHON_MINOR=$(echo $PYTHON_VERSION | cut -d'.' -f2)

if [ $PYTHON_MAJOR -lt 3 ] || ([ $PYTHON_MAJOR -eq 3 ] && [ $PYTHON_MINOR -lt 10 ]); then
    echo -e "${RED}[ERROR] Python version $PYTHON_VERSION is too old.${NC}"
    echo "Please install Python 3.10 or higher."
    exit 1
fi

echo -e "${GREEN}Python $PYTHON_VERSION found.${NC}"
echo

# 2. 创建虚拟环境
echo -e "[2/7] ${YELLOW}Creating virtual environment...${NC}"
if [ -d "venv" ]; then
    echo "Removing existing venv..."
    rm -rf venv
fi

python3 -m venv venv
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] Failed to create virtual environment.${NC}"
    exit 1
fi

echo -e "${GREEN}Virtual environment created.${NC}"
echo

# 设置镜像源（使用清华源）
export PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple
export PIP_TRUSTED_HOST=pypi.tuna.tsinghua.edu.cn

# 3. 升级 pip, setuptools, wheel
echo -e "[3/7] ${YELLOW}Upgrading pip, setuptools, wheel...${NC}"
venv/bin/python -m pip install --upgrade pip setuptools wheel
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] Upgrade failed, continuing anyway...${NC}"
fi
echo

# 4. 安装 PyTorch (CPU 版)
echo -e "[4/7] ${YELLOW}Installing PyTorch 2.1.2 (CPU)...${NC}"
venv/bin/python -m pip install torch==2.1.2 --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] PyTorch installation failed.${NC}"
    exit 1
fi

echo -e "${GREEN}PyTorch installed.${NC}"
echo

# 4.5 安装 NumPy（必须 <2，兼容 PyTorch）
echo -e "[4.5/7] ${YELLOW}Installing numpy (compatible with PyTorch)...${NC}"
venv/bin/python -m pip install "numpy<2" --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] numpy installation failed, but continuing...${NC}"
fi
echo

# 5. 安装 openai-whisper
echo -e "[5/7] ${YELLOW}Installing openai-whisper...${NC}"
venv/bin/python -m pip install openai-whisper --no-build-isolation --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] openai-whisper installation failed.${NC}"
    exit 1
fi

echo -e "${GREEN}openai-whisper installed.${NC}"
echo

# 6. 安装其余依赖
echo -e "[6/7] ${YELLOW}Installing remaining dependencies...${NC}"
venv/bin/python -m pip install fastapi==0.104.1 uvicorn==0.24.0.post1 python-multipart==0.0.6 pydantic==2.5.2 --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] Some package may have failed, but continuing...${NC}"
fi
echo

# 7. 安装 pydub 和 ffprobe
echo -e "[7/7] ${YELLOW}Installing pydub and checking ffmpeg...${NC}"
venv/bin/python -m pip install pydub --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST

# 检查系统 ffmpeg
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${YELLOW}[WARNING] ffmpeg not found in system.${NC}"
    echo "Please install ffmpeg:"
    echo "  Ubuntu/Debian: sudo apt update && sudo apt install ffmpeg"
    echo "  CentOS/RHEL: sudo yum install ffmpeg"
    echo "  macOS (Homebrew): brew install ffmpeg"
fi
echo

# 8. 验证安装
echo -e "[8/8] ${YELLOW}Verifying installation...${NC}"
venv/bin/python -c "import torch, whisper; print('torch version:', torch.__version__); print('whisper module loaded')"
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] Verification failed. Some modules may not work correctly.${NC}"
else
    echo -e "${GREEN}All modules verified.${NC}"
fi
echo

echo "========================================"
echo -e "${GREEN}Installation completed successfully!${NC}"
echo "========================================"
echo
echo "To start the lyrics service:"
echo "  1. Activate virtual environment: source venv/bin/activate"
echo "  2. Run: python main.py"
echo "  Or simply run: ./start.sh"
echo