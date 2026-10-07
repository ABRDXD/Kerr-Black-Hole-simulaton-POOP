@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-black-hole.ps1" %*
if errorlevel 1 pause
