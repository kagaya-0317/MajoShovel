@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\dev_auto_reload.ps1" %*

endlocal
