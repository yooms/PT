@echo off
rem PT -> GitHub upload. The actual work is done by GitPush.ps1 (Korean messages).
rem This launcher is ASCII only on purpose: cmd.exe misreads UTF-8 Korean text in .bat files.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0GitPush.ps1" %*
if errorlevel 9009 pause
