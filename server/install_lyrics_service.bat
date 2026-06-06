@echo off
title Lyrics Service Installer
echo ========================================
echo   Lyrics Service Dependency Installer
echo ========================================
echo.

REM 设置工作目录为脚本所在目录
cd /d "%~dp0"

REM 1. 检查并安装 Python 3.10+
echo [1/9] Checking Python 3.10+...
py -3.10 --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Python 3.10 not found. Attempting to install...
    echo.
    
    REM 尝试使用 winget 安装（Windows 10/11 自带）
    winget --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Detected: winget available
        echo Installing Python 3.10 via winget...
        winget install --id Python.Python.3.10 --accept-package-agreements --accept-source-agreements
        echo.
        echo Python installed. Please restart your terminal and run this script again.
        pause
        exit /b 0
    )
    
    REM 尝试使用 Chocolatey 安装
    choco --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Detected: Chocolatey available
        echo Installing Python 3.10 via Chocolatey...
        choco install python310 -y
        echo.
        echo Python installed. Please restart your terminal and run this script again.
        pause
        exit /b 0
    )
    
    REM 无法自动安装，提示用户手动安装
    echo [ERROR] Cannot install Python automatically.
    echo.
    echo Please install Python 3.10 or higher using one of these methods:
    echo.
    echo Method 1: Use winget (recommended for Windows 10/11)
    echo   winget install Python.Python.3.10
    echo.
    echo Method 2: Use Chocolatey
    echo   choco install python310
    echo.
    echo Method 3: Download manually
    echo   Visit: https://www.python.org/downloads/release/python-31011/
    echo   Make sure to check "Add Python to PATH" during installation.
    echo.
    echo After installation, run this script again.
    pause
    exit /b 1
)

for /f "tokens=2" %%i in ('py -3.10 --version') do set PYTHON_VERSION=%%i
echo Python %PYTHON_VERSION% found.
echo.

REM 2. 检查并安装 ffmpeg
echo [2/9] Checking ffmpeg...
ffmpeg -version >nul 2>&1
if %errorlevel% neq 0 (
    echo ffmpeg not found. Attempting to install...
    echo.
    
    REM 尝试使用 winget 安装（Windows 10/11 自带）
    winget --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Detected: winget available
        echo Installing ffmpeg via winget...
        winget install --id Gyan.FFmpeg --accept-package-agreements --accept-source-agreements
        goto :check_ffmpeg_again
    )
    
    REM 尝试使用 Chocolatey 安装
    choco --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Detected: Chocolatey available
        echo Installing ffmpeg via Chocolatey...
        choco install ffmpeg -y
        goto :check_ffmpeg_again
    )
    
    REM 无法自动安装，提示用户手动安装
    echo [ERROR] Cannot install ffmpeg automatically.
    echo.
    echo Please install ffmpeg manually using one of these methods:
    echo.
    echo Method 1: Use winget (recommended for Windows 10/11)
    echo   winget install Gyan.FFmpeg
    echo.
    echo Method 2: Use Chocolatey
    echo   choco install ffmpeg
    echo.
    echo Method 3: Download manually
    echo   1. Visit: https://www.gyan.dev/ffmpeg/builds/
    echo   2. Download "ffmpeg-release-essentials.zip"
    echo   3. Extract to a folder (e.g., C:\ffmpeg)
    echo   4. Add C:\ffmpeg\bin to your PATH environment variable
    echo.
    echo After installation, run this script again.
    pause
    exit /b 1
    
    :check_ffmpeg_again
    REM 刷新环境变量并再次检查
    ffmpeg -version >nul 2>&1
    if %errorlevel% neq 0 (
        echo [WARNING] ffmpeg installed but not in PATH.
        echo Please restart your terminal or add ffmpeg to PATH manually.
        echo.
        echo Continuing installation anyway...
    ) else (
        echo ffmpeg installed successfully.
    )
) else (
    echo ffmpeg found.
)
echo.

REM 3. 创建虚拟环境
echo [3/9] Creating virtual environment...
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

REM 4. 升级 pip, setuptools, wheel
echo [4/9] Upgrading pip, setuptools, wheel...
venv\Scripts\python.exe -m pip install --upgrade pip setuptools wheel --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [WARNING] Upgrade failed, continuing anyway...
)
echo.

REM 5. 安装 PyTorch (CPU 版) - 使用清华源
echo [5/9] Installing PyTorch 2.1.2 (CPU)...
venv\Scripts\python.exe -m pip install torch==2.1.2 --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [ERROR] PyTorch installation failed.
    pause
    exit /b 1
)
echo PyTorch installed.
echo.

REM 6. 安装 NumPy（必须 <2，兼容 PyTorch）
echo [6/9] Installing numpy (compatible with PyTorch)...
venv\Scripts\python.exe -m pip install "numpy<2" --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [WARNING] numpy installation failed, but continuing...
)
echo.

REM 7. 安装 openai-whisper (使用 --no-build-isolation)
echo [7/9] Installing openai-whisper...
venv\Scripts\python.exe -m pip install openai-whisper --no-build-isolation --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [ERROR] openai-whisper installation failed.
    pause
    exit /b 1
)
echo openai-whisper installed.
echo.

REM 8. 安装其余依赖
echo [8/9] Installing remaining dependencies...
venv\Scripts\python.exe -m pip install fastapi==0.104.1 uvicorn==0.24.0.post1 python-multipart==0.0.6 pydantic==2.5.2 pydub --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo [WARNING] Some package may have failed, but continuing...
)
echo.

REM 9. 验证安装
echo [9/9] Verifying installation...
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
echo   Or simply run: start.bat
echo.
pause