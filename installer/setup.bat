@echo off
title Встановлення U++ (Ukrainian++)
chcp 65001 > NUL
echo =========================================================
echo          Запуск інсталятора U++ (Ukrainian++)
echo =========================================================
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-UPlusPlus.ps1"
echo.
echo Інсталяцію завершено. Натисніть будь-яку клавішу для виходу...
pause > NUL
