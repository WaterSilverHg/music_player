@echo off
title Music Player Server Startup
echo ========================================
echo   Music Player Server Startup
echo ========================================
echo.

REM 设置工作目录为脚本所在目录
cd /d "%~dp0"

REM 1. 检查 Python 3.10+
echo [1/3] Checking Python 3.10+...
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

REM 2. 检查虚拟环境是否存在
echo [2/3] Checking virtual environment...
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

REM 3. 激活虚拟环境并启动服务器
echo [3/3] Starting server...
call venv\Scripts\activate

echo [%date% %time%] Starting Music Player Server...
py -3.10 main.py
pause