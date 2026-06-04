@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
REM sdvm -- Star Dance 编译器+虚拟机 一键运行
REM 用法: sdvm <文件.star|文件.dance> [-v]
REM   .star  自动编译为 .dance 后执行
REM   .dance 直接执行
REM   -v     启用指令追踪调试

if "%1"=="" goto usage

set "FILE=%~f1"
set VERBOSE=
if /I "%2"=="-v" set VERBOSE=-v
if /I "%2"=="--verbose" set VERBOSE=-v

if not exist "%FILE%" (
    echo 错误: 文件 '%FILE%' 不存在
    exit /b 1
)

set "EXT=%~x1"
if /I "%EXT%"==".star" goto compile
if /I "%EXT%"==".dance" goto run

:usage
echo 用法: sdvm ^<文件.star^|文件.dance^> [-v]
echo   .star  自动编译为 .dance 后执行
echo   .dance 直接执行
echo   -v     启用指令追踪调试
exit /b 1

:compile
echo [编译] %FILE% ...
python "%~dp0..\python\compiler.py" "%FILE%" -o "%~dpn1.dance"
if %ERRORLEVEL% neq 0 (
    echo 编译失败!
    exit /b %ERRORLEVEL%
)
set "FILE=%~dpn1.dance"

:run
"%~dp0SDVM.exe" "%FILE%" %VERBOSE%
exit /b %ERRORLEVEL%
