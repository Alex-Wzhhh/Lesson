# D:\University\Qt\Qt-Project\Lesson\backend\run_backend.ps1
[CmdletBinding()]
param(
  [string]$EnvPath   = "D:\University\Qt\Qt-Project\Lesson\backend\env",  # conda/venv 根目录
  [string]$AppModule = "app:app",                                        # FastAPI 模块:对象
  [string]$Bind      = "127.0.0.1",                                       # 绑定地址（避免与 $Host 冲突）
  [ValidateRange(1,65535)][int]$Port = 5001,
  [switch]$Reload
)

$ErrorActionPreference = "Stop"

# 切到脚本所在目录（backend）
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# 可靠选择 python.exe（先 env\python.exe，再 env\Scripts\python.exe）
$python = Join-Path $EnvPath "python.exe"
if (-not (Test-Path $python)) {
  $python = Join-Path $EnvPath "Scripts\python.exe"
}
if (-not (Test-Path $python)) {
  throw "未找到 python.exe。请确认 EnvPath 是否为 conda/venv 的根目录：$EnvPath"
}

# 端口占用检查（避免 10048）
$portFree = $true
try {
  $tcp = New-Object System.Net.Sockets.TcpClient
  $tcp.Connect($Bind, $Port)
  $tcp.Close()
  $portFree = $false
} catch { $portFree = $true }

if (-not $portFree) {
  Write-Error "端口 $Bind`:$Port 已被占用。请改端口或关闭占用该端口的进程。"
  exit 1
}

# 组装 uvicorn 启动参数
$uvicornArgs = @("-m","uvicorn", $AppModule, "--host", $Bind, "--port", $Port)
if ($Reload) { $uvicornArgs += "--reload" }

Write-Host "使用环境 $EnvPath 启动后端：`"$python`" $($uvicornArgs -join ' ')"

# 如需自动安装依赖，取消下一行注释
# & $python -m pip install -r (Join-Path $ScriptDir "requirements.txt")

& $python @uvicornArgs
