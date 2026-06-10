@echo off
title Music Player Server Startup
echo ========================================
echo   Music Player Server Startup
echo ========================================
echo.

REM Set working directory to script location
cd /d "%~dp0"

REM 1. Check Python 3.10+
echo [1/4] Checking Python 3.10+...
py -3.10 --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python 3.10 not found!
    echo.
    echo Please install Python 3.10 or higher from:
    echo https://www.python.org/downloads/release/python-31011/
    echo.
    echo Or run the installation script:
    echo install_lyrics_service.bat
    pause
    exit /b 1
)

for /f "tokens=2" %%i in ('py -3.10 --version') do set PYTHON_VERSION=%%i
echo Python %PYTHON_VERSION% found.
echo.

REM 2. Check virtual environment
echo [2/4] Checking virtual environment...
if not exist "venv" (
    echo [ERROR] Virtual environment 'venv' not found!
    echo.
    echo Please run the installation script first:
    echo install_lyrics_service.bat
    pause
    exit /b 1
)

echo Virtual environment found.
echo.

REM 3. Check and install dependencies
echo [3/4] Checking dependencies...
call venv\Scripts\activate

venv\Scripts\python.exe -c "import fastapi, uvicorn, multipart, pydantic, pydub, torch, whisper" >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARNING] Some dependencies are missing. Installing...
    set PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple
    set PIP_TRUSTED_HOST=pypi.tuna.tsinghua.edu.cn
    venv\Scripts\python.exe -m pip install fastapi uvicorn python-multipart pydantic pydub torch==2.1.2 "numpy<2" openai-whisper --index-url %PIP_INDEX_URL% --trusted-host %PIP_TRUSTED_HOST%
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to install dependencies.
        pause
        exit /b 1
    )
    echo Dependencies installed.
) else (
    echo All dependencies found.
)
echo.

REM 4. Start server
echo [4/4] Starting server...
echo [%date% %time%] Starting Music Player Server...
venv\Scripts\python.exe main.py
pause
