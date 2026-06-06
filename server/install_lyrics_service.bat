@echo off
title Lyrics Service Installer
echo ========================================
echo   Lyrics Service Dependency Installer
echo ========================================
echo.

REM 1. 检查 Python 3.10
echo [1/7] Checking Python 3.10...
py -3.10 --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python 3.10 not found.
    echo Please install Python 3.10 from https://www.python.org/downloads/release/python-31011/
    pause
    exit /b 1
)
echo Python 3.10 found.
echo.

REM 2. 创建虚拟环境
echo [2/7] Creating virtual environment...
if exist venv (
    echo Removing existing venv...
    rmdir /s /q venv
)
py -3.10 -m venv venv
if %errorlevel% neq 0 (
    echo [ERROR] Failed to create virtual environment.
    pause
    exit /b 1
)
echo Virtual environment created.
echo.

REM 设置镜像源（使用清华源，SSL 正常）
set PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple
set PIP_TRUSTED_HOST=pypi.tuna.tsinghua.edu.cn

REM 3. 升级 pip, setuptools, wheel
echo [3/7] Upgrading pip, setuptools, wheel...
venv\Scripts\python.exe -m pip install --upgrade pip setuptools wheel --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [WARNING] Upgrade failed, continuing anyway...
)
echo.

REM 4. 安装 PyTorch (CPU 版) - 使用清华源
echo [4/7] Installing PyTorch 2.1.2 (CPU)...
venv\Scripts\python.exe -m pip install torch==2.1.2 --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [ERROR] PyTorch installation failed.
    pause
    exit /b 1
)
echo PyTorch installed.
echo.

REM 4.5 安装 NumPy（必须 <2，兼容 PyTorch）
echo [4.5/7] Installing numpy (compatible with PyTorch)...
venv\Scripts\python.exe -m pip install "numpy<2" --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [WARNING] numpy installation failed, but continuing...
)
echo.

REM 5. 安装 openai-whisper (使用 --no-build-isolation)
echo [5/7] Installing openai-whisper...
venv\Scripts\python.exe -m pip install openai-whisper --no-build-isolation --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [ERROR] openai-whisper installation failed.
    pause
    exit /b 1
)
echo openai-whisper installed.
echo.

REM 6. 安装其余依赖
echo [6/7] Installing remaining dependencies...
venv\Scripts\python.exe -m pip install fastapi==0.104.1 uvicorn==0.24.0.post1 python-multipart==0.0.6 pydantic==2.5.2 --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [WARNING] Some package may have failed, but continuing...
)
echo.

REM 7. 验证安装
echo [7/7] Verifying installation...
venv\Scripts\python.exe -c "import torch, whisper; print('torch version:', torch.__version__); print('whisper module loaded')"
if %errorlevel% neq 0 (
    echo [WARNING] Verification failed. Some modules may not work correctly.
) else (
    echo All modules verified.
)
echo.

echo ========================================
echo   Installation completed successfully!
echo ========================================
echo.
echo To start the lyrics service:
echo   1. Activate virtual environment: venv\Scripts\activate
echo   2. Run: python main.py
echo.
pause