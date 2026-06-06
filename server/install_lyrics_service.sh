#!/bin/bash
# Lyrics Service Dependency Installer for Linux
# =============================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================"
echo "   Lyrics Service Dependency Installer"
echo "========================================"
echo

# 1. 检查并安装 Python 3.10+
echo -e "[1/9] ${YELLOW}Checking Python 3.10+...${NC}"
PYTHON_FOUND=false

if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
    PYTHON_MAJOR=$(echo $PYTHON_VERSION | cut -d'.' -f1)
    PYTHON_MINOR=$(echo $PYTHON_VERSION | cut -d'.' -f2)
    
    if [ $PYTHON_MAJOR -ge 3 ] && [ $PYTHON_MINOR -ge 10 ]; then
        PYTHON_FOUND=true
    fi
fi

if [ "$PYTHON_FOUND" = false ]; then
    echo -e "${YELLOW}Python 3.10+ not found. Attempting to install...${NC}"
    echo
    
    # 检测系统类型并安装 Python
    if command -v apt-get &> /dev/null; then
        echo "Detected: Ubuntu/Debian"
        echo "Installing Python 3.10 via apt..."
        sudo apt-get update && sudo apt-get install -y python3.10 python3.10-venv python3.10-dev
    elif command -v yum &> /dev/null; then
        echo "Detected: CentOS/RHEL"
        echo "Installing Python 3.10 via yum..."
        sudo yum install -y python3.10 python3.10-devel
    elif command -v dnf &> /dev/null; then
        echo "Detected: Fedora"
        echo "Installing Python 3.10 via dnf..."
        sudo dnf install -y python3.10 python3.10-devel
    elif command -v brew &> /dev/null; then
        echo "Detected: macOS (Homebrew)"
        echo "Installing Python 3.10 via brew..."
        brew install python@3.10
    else
        echo -e "${RED}[ERROR] Cannot install Python automatically.${NC}"
        echo
        echo "Please install Python 3.10+ manually:"
        echo "  Ubuntu/Debian: sudo apt update && sudo apt install python3.10 python3.10-venv"
        echo "  CentOS/RHEL: sudo yum install python3.10"
        echo "  macOS (Homebrew): brew install python@3.10"
        echo
        echo "After installation, run this script again."
        exit 1
    fi
    
    # 检查安装是否成功
    if ! command -v python3.10 &> /dev/null; then
        echo -e "${RED}[ERROR] Python 3.10 installation failed.${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Python 3.10 installed successfully.${NC}"
    echo "Please restart your terminal and run this script again."
    exit 0
fi

echo -e "${GREEN}Python $PYTHON_VERSION found.${NC}"
echo

# 2. 检查并安装 ffmpeg
echo -e "[2/9] ${YELLOW}Checking ffmpeg...${NC}"
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${YELLOW}ffmpeg not found. Attempting to install...${NC}"
    
    # 检测系统类型并安装 ffmpeg
    if command -v apt-get &> /dev/null; then
        echo "Detected: Ubuntu/Debian"
        sudo apt-get update && sudo apt-get install -y ffmpeg
    elif command -v yum &> /dev/null; then
        echo "Detected: CentOS/RHEL"
        sudo yum install -y ffmpeg
    elif command -v dnf &> /dev/null; then
        echo "Detected: Fedora"
        sudo dnf install -y ffmpeg
    elif command -v brew &> /dev/null; then
        echo "Detected: macOS (Homebrew)"
        brew install ffmpeg
    else
        echo -e "${RED}[ERROR] Cannot install ffmpeg automatically.${NC}"
        echo "Please install ffmpeg manually:"
        echo "  Ubuntu/Debian: sudo apt install ffmpeg"
        echo "  CentOS/RHEL: sudo yum install ffmpeg"
        echo "  macOS: brew install ffmpeg"
        echo
        echo "After installation, run this script again."
        exit 1
    fi
    
    # 再次检查 ffmpeg
    if ! command -v ffmpeg &> /dev/null; then
        echo -e "${RED}[ERROR] ffmpeg installation failed.${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}ffmpeg installed successfully.${NC}"
else
    echo -e "${GREEN}ffmpeg found.${NC}"
fi
echo

# 3. 创建虚拟环境
echo -e "[3/9] ${YELLOW}Creating virtual environment...${NC}"
if [ -d "venv" ]; then
    echo "Removing existing venv..."
    rm -rf venv
fi

python3 -m venv venv
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] Failed to create virtual environment.${NC}"
    echo
    echo "Make sure python3-venv is installed:"
    echo "  Ubuntu/Debian: sudo apt install python3.10-venv"
    exit 1
fi

echo -e "${GREEN}Virtual environment created.${NC}"
echo

# 设置镜像源（使用清华源）
export PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple
export PIP_TRUSTED_HOST=pypi.tuna.tsinghua.edu.cn

# 4. 升级 pip, setuptools, wheel
echo -e "[4/9] ${YELLOW}Upgrading pip, setuptools, wheel...${NC}"
venv/bin/python -m pip install --upgrade pip setuptools wheel
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] Upgrade failed, continuing anyway...${NC}"
fi
echo

# 5. 安装 PyTorch (CPU 版)
echo -e "[5/9] ${YELLOW}Installing PyTorch 2.1.2 (CPU)...${NC}"
venv/bin/python -m pip install torch==2.1.2 --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] PyTorch installation failed.${NC}"
    exit 1
fi

echo -e "${GREEN}PyTorch installed.${NC}"
echo

# 6. 安装 NumPy（必须 <2，兼容 PyTorch）
echo -e "[6/9] ${YELLOW}Installing numpy (compatible with PyTorch)...${NC}"
venv/bin/python -m pip install "numpy<2" --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] numpy installation failed, but continuing...${NC}"
fi
echo

# 7. 安装 openai-whisper
echo -e "[7/9] ${YELLOW}Installing openai-whisper...${NC}"
venv/bin/python -m pip install openai-whisper --no-build-isolation --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] openai-whisper installation failed.${NC}"
    exit 1
fi

echo -e "${GREEN}openai-whisper installed.${NC}"
echo

# 8. 安装其余依赖
echo -e "[8/9] ${YELLOW}Installing remaining dependencies...${NC}"
venv/bin/python -m pip install fastapi==0.104.1 uvicorn==0.24.0.post1 python-multipart==0.0.6 pydantic==2.5.2 pydub --index-url $PIP_INDEX_URL --trusted-host $PIP_TRUSTED_HOST
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}[WARNING] Some package may have failed, but continuing...${NC}"
fi
echo

# 9. 验证安装
echo -e "[9/9] ${BLUE}Verifying installation...${NC}"
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
echo -e "${BLUE}  1. Activate virtual environment:${NC} source venv/bin/activate"
echo -e "${BLUE}  2. Run:${NC} python main.py"
echo -e "${BLUE}  Or simply run:${NC} ./start.sh"
echo