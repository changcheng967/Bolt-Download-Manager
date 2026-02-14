@echo off
REM Update this file with your Chrome extension ID after loading the extension

set EXTENSION_ID=YOUR_EXTENSION_ID_HERE

echo Updating native messaging host manifest with extension ID: %EXTENSION_ID%

(
echo {
echo   "name": "com.boltdm.host",
echo   "description": "Bolt Download Manager Native Host",
echo   "path": "C:\\Users\\chang\\AppData\\Local\\BoltDM\\boltdm_host.exe",
echo   "type": "stdio",
echo   "allowed_origins": [
echo     "chrome-extension://%EXTENSION_ID%/"
echo   ]
echo }
) > "%LOCALAPPDATA%\BoltDM\com.boltdm.host.json"

echo Done! Restart Chrome to apply changes.
pause
