@echo off
REM Bolt Download Manager Browser Extension Installer
REM Copyright (c) 2026 changcheng967. All rights reserved.

echo Installing Bolt Download Manager Browser Integration...
echo.

REM Set installation paths
set "INSTALL_DIR=%LOCALAPPDATA%\BoltDM"
set "HOST_EXE=%~dp0..\build\bin\boltdm_host.exe"
set "HOST_JSON=%~dp0com.boltdm.host.json"

REM Create install directory
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo Created: %INSTALL_DIR%
)

REM Copy native host executable
if exist "%HOST_EXE%" (
    copy /Y "%HOST_EXE%" "%INSTALL_DIR%\boltdm_host.exe"
    echo Copied: boltdm_host.exe
) else (
    echo ERROR: boltdm_host.exe not found. Build it first!
    pause
    exit /b 1
)

REM Update manifest with correct path
echo Creating native messaging host manifest...
(
echo {
echo   "name": "com.boltdm.host",
echo   "description": "Bolt Download Manager Native Host",
echo   "path": "%INSTALL_DIR:\=\\%\\boltdm_host.exe",
echo   "type": "stdio",
echo   "allowed_origins": [
echo     "chrome-extension://EXTENSION_ID_PLACEHOLDER/"
echo   ]
echo }
) > "%INSTALL_DIR%\com.boltdm.host.json"
echo Created: %INSTALL_DIR%\com.boltdm.host.json

REM Register for Chrome
echo.
echo Registering for Chrome...
reg add "HKCU\SOFTWARE\Google\Chrome\NativeMessagingHosts\com.boltdm.host" /ve /t REG_SZ /d "%INSTALL_DIR%\com.boltdm.host.json" /f >nul 2>&1
if %errorlevel% equ 0 (
    echo Chrome: OK
) else (
    echo Chrome: Failed (try running as administrator)
)

REM Register for Firefox
echo Registering for Firefox...
reg add "HKCU\SOFTWARE\Mozilla\NativeMessagingHosts\com.boltdm.host" /ve /t REG_SZ /d "%INSTALL_DIR%\com.boltdm.host.json" /f >nul 2>&1
if %errorlevel% equ 0 (
    echo Firefox: OK
) else (
    echo Firefox: Failed (try running as administrator)
)

REM Register for Edge
echo Registering for Edge...
reg add "HKCU\SOFTWARE\Microsoft\Edge\NativeMessagingHosts\com.boltdm.host" /ve /t REG_SZ /d "%INSTALL_DIR%\com.boltdm.host.json" /f >nul 2>&1
if %errorlevel% equ 0 (
    echo Edge: OK
) else (
    echo Edge: Failed (try running as administrator)
)

echo.
echo ============================================
echo Installation complete!
echo.
echo NEXT STEPS:
echo 1. Load the extension in Chrome:
echo    - Open chrome://extensions/
echo    - Enable "Developer mode"
echo    - Click "Load unpacked"
echo    - Select: %~dp0
echo.
echo 2. Get the extension ID from chrome://extensions/
echo.
echo 3. Edit %INSTALL_DIR%\com.boltdm.host.json
echo    Replace EXTENSION_ID_PLACEHOLDER with your extension ID
echo.
echo 4. Restart Chrome
echo ============================================
echo.
pause
