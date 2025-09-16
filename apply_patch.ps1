# 一键应用补丁：apply_patch.ps1
# 用法：在仓库任意子目录打开 PowerShell，执行：powershell -ExecutionPolicy Bypass -File .\apply_patch.ps1

$ErrorActionPreference = 'Stop'

# 1) 找到 Git 仓库根目录
$repo = (& git rev-parse --show-toplevel) 2>$null
if (-not $repo) {
    throw "当前目录不是 Git 仓库（未找到 .git）。请先 cd 到你的项目目录。"
}
Set-Location $repo

# 2) 补丁文件路径
$patchPath = Join-Path $repo 'patch.diff'
if (-not (Test-Path $patchPath)) {
    throw "未找到补丁文件：$patchPath。请把 patch.diff 放到仓库根目录。"
}

# 3) 规范化编码（避免 BOM/换行问题）
$raw = Get-Content -Raw -Path $patchPath
if ($PSVersionTable.PSVersion.Major -ge 7) {
    # PS7 的 utf8 默认无 BOM
    $raw | Out-File -FilePath $patchPath -Encoding utf8
} else {
    # PS5.1：用 .NET 写 UTF-8（无 BOM）
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($patchPath, $raw, $enc)
}


# 4) 先干跑检查
Write-Host "检查补丁可应用性..." -ForegroundColor Cyan
& git apply --check $patchPath

# 5) 尝试直接应用；如果失败，再尝试三方合并
try {
    Write-Host "正在应用补丁（普通模式）..." -ForegroundColor Cyan
    & git apply $patchPath
    Write-Host "补丁应用成功（普通模式）。" -ForegroundColor Green
} catch {
    Write-Host "普通模式失败，尝试三方合并（--3way）..." -ForegroundColor Yellow
    & git apply --3way $patchPath
    Write-Host "补丁应用成功（--3way）。" -ForegroundColor Green
}

# 6) 显示状态，提示你提交
Write-Host "`n当前变更：" -ForegroundColor Cyan
& git status --porcelain=v1
Write-Host "`n如需提交：" -ForegroundColor Cyan
Write-Host "  git add -A"
Write-Host "  git commit -m `"Apply patch`""
