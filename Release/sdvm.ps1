#requires -version 5
<#
.SYNOPSIS
    Star Dance 编译器+虚拟机
.DESCRIPTION
    sdvm.ps1 自动识别文件后缀：
      .star  -> 编译为 .dance 后执行
      .dance -> 直接执行
.PARAMETER File
    .star 源码文件 或 .dance 字节码文件
.PARAMETER v
    启用指令追踪调试
.EXAMPLE
    sdvm test.star
    sdvm test.star -v
    sdvm test.dance
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$File,
    [Parameter(Position = 1)]
    [switch]$v
)

$SDVM_HOME = if ($env:SDVM_HOME) { $env:SDVM_HOME } else { "D:\stay\SDVM" }

if (-not (Test-Path $File)) {
    Write-Host "[错误] 文件 '$File' 不存在" -ForegroundColor Red
    exit 1
}

$resolved = Resolve-Path $File
$ext = [System.IO.Path]::GetExtension($resolved).ToLower()
$verboseArg = if ($v) { "-v" } else { "" }

if ($ext -eq '.star') {
    $danceFile = [System.IO.Path]::ChangeExtension($resolved, '.dance')
    Write-Host "[编译] $resolved -> $danceFile" -ForegroundColor Cyan
    python "$SDVM_HOME\python\compiler.py" $resolved -o $danceFile
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[错误] 编译失败!" -ForegroundColor Red
        exit 1
    }
    Write-Host "[运行] $danceFile" -ForegroundColor Green
    & "$SDVM_HOME\release\SDVM.exe" $danceFile $verboseArg
}
elseif ($ext -eq '.dance') {
    & "$SDVM_HOME\Release\SDVM.exe" $resolved $verboseArg
}
else {
    Write-Host "用法: sdvm {文件.star|文件.dance} [-v]" -ForegroundColor Yellow
    Write-Host "  .star  自动编译为 .dance 后执行"
    Write-Host "  .dance 直接执行"
    Write-Host "  -v     启用指令追踪调试"
    exit 1
}