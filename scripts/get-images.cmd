@echo off
rem Runs Get-Images.ps1 from CMD. Example: get-images.cmd -Name success,failure
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Get-Images.ps1" %*
exit /b %ERRORLEVEL%
