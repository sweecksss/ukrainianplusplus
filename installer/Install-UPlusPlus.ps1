# Windows Installer script for U++ (Ukrainian++)
$ErrorActionPreference = "Stop"

$installDir = "$env:LOCALAPPDATA\Programs\UPlusPlus"
Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "        Встановлення U++ (Ukrainian++) на ПК             " -ForegroundColor Yellow
Write-Host "=========================================================" -ForegroundColor Cyan

# 1. Create installation directory
if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

# 2. Copy binary
$exeSource = Join-Path $PSScriptRoot "upp.exe"
if (-not (Test-Path $exeSource)) {
    $exeSource = Join-Path (Get-Item $PSScriptRoot).Parent.FullName "upp.exe"
}

if (Test-Path $exeSource) {
    Copy-Item -Path $exeSource -Destination "$installDir\upp.exe" -Force
    Write-Host "[✓] upp.exe скопійовано в $installDir" -ForegroundColor Green
} else {
    Write-Host "[X] Помилка: upp.exe не знайдено в папці інсталятора." -ForegroundColor Red
    exit 1
}

# 3. Add to User PATH
$userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$installDir*") {
    $newPath = "$userPath;$installDir"
    [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
    Write-Host "[✓] U++ додано у системну змінну оточення PATH!" -ForegroundColor Green
} else {
    Write-Host "[i] U++ вже є у змінній PATH." -ForegroundColor Yellow
}

# 4. Associate .upp file extension with upp.exe
try {
    cmd /c "assoc .upp=UPlusPlusScript" | Out-Null
    cmd /c "ftype UPlusPlusScript=""$installDir\upp.exe"" ""%1""" | Out-Null
    Write-Host "[✓] Асоціацію файлів .upp успішно створено!" -ForegroundColor Green
} catch {
    # Ignore if non-admin
}

Write-Host ""
Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " 🎉 УСПІХ! U++ (Ukrainian++) успішно встановлено!" -ForegroundColor Green
Write-Host " Перезапустіть термінал і введіть: upp <файл.upp>" -ForegroundColor White
Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "Натисніть кновку Enter для виходу..." -ForegroundColor Gray
Read-Host
