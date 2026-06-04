@echo off
chcp 65001 >nul
REM SDVM 一键编译 + 运行
REM 用法: run_sdvm <文件.star> [-v]

if "%1"=="" (
    echo 用法: run_sdvm ^<文件.star^> [-v]
    echo   -v  启用指令追踪调试
    exit /b 1
)

set STAR_FILE=%1
set VERBOSE=
if "%2"=="-v" set VERBOSE=-v
if "%2"=="--verbose" set VERBOSE=-v

if not exist "%STAR_FILE%" (
    echo 错误: 文件 '%STAR_FILE%' 不存在
    exit /b 1
)

echo [编译] %STAR_FILE% ...
python compiler.py "%STAR_FILE%" -o "%~n1.dance"
if %ERRORLEVEL% neq 0 (
    echo 编译失败!
    exit /b %ERRORLEVEL%
)

echo.
echo [运行] %~n1.dance ...
.\Release\SDVM.exe "%~n1.dance" %VERBOSE%
