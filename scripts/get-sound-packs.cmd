@echo off
rem Runs Get-SoundPacks.ps1 from CMD. Example: get-sound-packs.cmd -Name peon,glados
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Get-SoundPacks.ps1" %*
exit /b %ERRORLEVEL%
