# Windows Installer script for U++ (Ukrainian++)
$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$installDir = "$env:LOCALAPPDATA\Programs\UPlusPlus"

Write-Host "[1/3] Створення папки програми: $installDir" -ForegroundColor Cyan

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

$exeSource = Join-Path $PSScriptRoot "upp.exe"
if (-not (Test-Path $exeSource)) {
    $exeSource = Join-Path (Get-Item $PSScriptRoot).Parent.FullName "upp.exe"
}

if (Test-Path $exeSource) {
    Copy-Item -Path $exeSource -Destination "$installDir\upp.exe" -Force
    Write-Host "[✓] upp.exe скопійовано успішно!" -ForegroundColor Green
} else {
    Write-Host "[X] УВАГА: Файл upp.exe не знайдено в папці інсталятора!" -ForegroundColor Red
    Write-Host "    Переконайтеся, що ви повністю РОЗАРХІВУВАЛИ zip-архів перед запуском setup.bat!" -ForegroundColor Yellow
    exit 1
}

Write-Host "[2/3] Додавання U++ у системні змінні оточення (PATH)..." -ForegroundColor Cyan
try {
    $userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    if ($userPath -notlike "*$installDir*") {
        $newPath = "$userPath;$installDir"
        [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
        Write-Host "[✓] Шлях $installDir успішно додано в PATH!" -ForegroundColor Green
    } else {
        Write-Host "[i] U++ вже присутній у змінній PATH." -ForegroundColor Yellow
    }
} catch {
    Write-Host "[!] Не вдалося оновити PATH: $_" -ForegroundColor Red
}

Write-Host "[3/3] Налаштування асоціацій файлів .upp..." -ForegroundColor Cyan
try {
    cmd /c "assoc .upp=UPlusPlusScript" | Out-Null
    cmd /c "ftype UPlusPlusScript=""$installDir\upp.exe"" ""%1""" | Out-Null
    Write-Host "[✓] Асоціацію файлів розширення .upp налаштовано!" -ForegroundColor Green
} catch {}

Write-Host ""
Write-Host "=========================================================" -ForegroundColor Green
Write-Host " 🎉 U++ (Ukrainian++) успішно встановлено на ваш ПК!" -ForegroundColor Green
Write-Host " Перезапустіть консоль і введіть: upp <файл.upp>" -ForegroundColor White
Write-Host "=========================================================" -ForegroundColor Green
