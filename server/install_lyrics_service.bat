@echo off
chcp 65001 >nul
title Lyrics Service Installer
echo ========================================
echo   Lyrics Service Dependency Installer
echo ========================================
echo.

cd /d "%~dp0"

echo [1/9] Checking Python 3.10...
py -3.10 --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Python 3.10 not found.
    winget --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Installing Python 3.10 via winget...
        winget install --id Python.Python.3.10 --accept-package-agreements --accept-source-agreements
        echo Python installed. Please restart your terminal and run this script again.
        pause
        exit /b 0
    )
    choco --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Installing Python 3.10 via Chocolatey...
        choco install python310 -y
        echo Python installed. Please restart your terminal and run this script again.
        pause
        exit /b 0
    )
    echo ERROR: Cannot install Python automatically.
    echo Please install Python 3.10 from:
    echo https://www.python.org/downloads/release/python-31011/
    pause
    exit /b 1
)

for /f "tokens=2" %%i in ('py -3.10 --version') do set PYTHON_VERSION=%%i
echo Python %PYTHON_VERSION% found.
echo.

echo [2/9] Checking ffmpeg...
ffmpeg -version >nul 2>&1
if %errorlevel% neq 0 (
    echo ffmpeg not found.
    winget --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Installing ffmpeg via winget...
        winget install --id Gyan.FFmpeg --accept-package-agreements --accept-source-agreements
        goto :ffmpeg_check
    )
    choco --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo Installing ffmpeg via Chocolatey...
        choco install ffmpeg -y
        goto :ffmpeg_check
    )
    echo ERROR: Cannot install ffmpeg automatically.
    echo Please install ffmpeg manually.
    pause
    exit /b 1
    :ffmpeg_check
    ffmpeg -version >nul 2>&1
    if %errorlevel% neq 0 (
        echo WARNING: ffmpeg installed but not in PATH.
    ) else (
        echo ffmpeg installed successfully.
    )
) else (
    echo ffmpeg found.
)
echo.

echo [3/9] Creating virtual environment...
if exist venv (
    echo Removing existing venv...
    rmdir /s /q venv
)
py -3.10 -m venv venv
if %errorlevel% neq 0 (
    echo ERROR: Failed to create virtual environment.
    pause
    exit /b 1
)
echo Virtual environment created.
echo.

set PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple
set PIP_TRUSTED_HOST=pypi.tuna.tsinghua.edu.cn

echo [4/9] Upgrading pip...
venv\Scripts\python.exe -m pip install --upgrade pip >nul 2>&1
echo Done.
echo.

echo [5/9] Installing PyTorch...
venv\Scripts\python.exe -m pip install torch==2.1.2 --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo ERROR: PyTorch installation failed.
    pause
    exit /b 1
)
echo Done.
echo.

echo [6/9] Installing numpy...
venv\Scripts\python.exe -m pip install "numpy<2" --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
echo Done.
echo.

echo [7/9] Installing openai-whisper...
venv\Scripts\python.exe -m pip install openai-whisper --no-build-isolation --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
if %errorlevel% neq 0 (
    echo ERROR: openai-whisper installation failed.
    pause
    exit /b 1
)
echo Done.
echo.

echo [8/9] Installing remaining dependencies...
venv\Scripts\python.exe -m pip install fastapi uvicorn python-multipart pydantic pydub --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
echo Done.
echo.

echo [9/9] Verifying installation...
venv\Scripts\python.exe -c "import torch, whisper; print('torch:', torch.__version__); print('whisper loaded')"
if %errorlevel% neq 0 (
    echo WARNING: Verification failed.
) else (
    echo All modules verified.
)
echo.

echo ========================================
echo Installation completed successfully!
echo ========================================
echo.
echo To start:
echo  1. venv\Scripts\activate
echo  2. python main.py
echo Or run: start.bat
echo.
pause