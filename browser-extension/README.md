# Bolt Download Manager Browser Extension

Browser extension for Chrome and Firefox to integrate with Bolt Download Manager.

## Installation

### 1. Install the Native Host

1. Build `boltdm_host.exe` and copy it to `C:\Program Files\BoltDM\`
2. Edit `com.boltdm.host.json` and update the extension IDs
3. Copy `com.boltdm.host.json` to:
   - Chrome: `HKEY_CURRENT_USER\SOFTWARE\Google\Chrome\NativeMessagingHosts\com.boltdm.host`
   - Firefox: `HKEY_CURRENT_USER\SOFTWARE\Mozilla\NativeMessagingHosts\com.boltdm.host`

Or run the install script (as Administrator):

```powershell
# Install native host registry entry
reg add "HKCU\SOFTWARE\Google\Chrome\NativeMessagingHosts\com.boltdm.host" /ve /t REG_SZ /d "C:\Program Files\BoltDM\com.boltdm.host.json" /f
reg add "HKCU\SOFTWARE\Mozilla\NativeMessagingHosts\com.boltdm.host" /ve /t REG_SZ /d "C:\Program Files\BoltDM\com.boltdm.host.json" /f
```

### 2. Install the Extension

**Chrome:**
1. Open `chrome://extensions/`
2. Enable "Developer mode"
3. Click "Load unpacked"
4. Select the `browser-extension` folder

**Firefox:**
1. Open `about:debugging#/runtime/this-firefox`
2. Click "Load Temporary Add-on"
3. Select `manifest.json` from the `browser-extension` folder

## Usage

1. Right-click any link and select "Download with BoltDM"
2. Or click the extension icon and paste a URL
3. Toggle "Intercept all downloads" to automatically send all downloads to BoltDM

## Features

- Right-click context menu integration
- Paste URL in popup
- Optional download interception
- Filename auto-detection from URL
- Referrer support

## Copyright

Copyright (c) 2026 changcheng967. All rights reserved.
