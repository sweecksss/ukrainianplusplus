@echo off
title Встановлення U++ (Ukrainian++)
chcp 65001 > NUL
echo Запуск інсталятора U++...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-UPlusPlus.ps1"
