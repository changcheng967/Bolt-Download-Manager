// Bolt Download Manager Browser Extension - IDM-style download interception
// Copyright (c) 2026 changcheng967. All rights reserved.

const NATIVE_HOST_NAME = 'com.boltdm.host';

// Native messaging port
let nativePort = null;

// Track if we're intercepting
let interceptEnabled = true;

// Pending downloads waiting for user response
const pendingDownloads = new Map();

// Connect to native host
function connectNative() {
    try {
        nativePort = chrome.runtime.connectNative(NATIVE_HOST_NAME);

        nativePort.onMessage.addListener((response) => {
            console.log('BoltDM response:', response);
        });

        nativePort.onDisconnect.addListener(() => {
            console.log('Disconnected from native host, will reconnect on next download');
            nativePort = null;
        });

        return true;
    } catch (e) {
        console.error('Failed to connect to native host:', e);
        return false;
    }
}

// Send download to BoltDM
function sendToBoltDM(url, filename, savePath) {
    if (!nativePort) {
        if (!connectNative()) {
            showNotification('BoltDM Error', 'Could not connect to Bolt Download Manager. Is it installed?');
            return false;
        }
    }

    nativePort.postMessage({
        url: url,
        filename: filename,
        savePath: savePath
    });

    return true;
}

// Show notification
function showNotification(title, message) {
    chrome.notifications.create({
        type: 'basic',
        iconUrl: 'icons/icon48.png',
        title: title,
        message: message
    });
}

// Intercept downloads - IDM style
chrome.downloads.onCreated.addListener((downloadItem) => {
    if (!interceptEnabled) return;

    const url = downloadItem.url;
    const filename = downloadItem.filename || '';

    // Skip blob URLs and data URLs
    if (url.startsWith('blob:') || url.startsWith('data:')) return;

    // Cancel the browser's download immediately
    chrome.downloads.cancel(downloadItem.id, () => {
        if (chrome.runtime.lastError) {
            // Download might have already completed
            return;
        }

        // Store pending download info
        const downloadId = Date.now();
        pendingDownloads.set(downloadId, {
            url: url,
            filename: filename,
            fileSize: downloadItem.fileSize || 0,
            mime: downloadItem.mime || ''
        });

        // Open the download dialog popup
        chrome.windows.create({
            url: `dialog.html?id=${downloadId}&url=${encodeURIComponent(url)}&filename=${encodeURIComponent(filename)}&size=${downloadItem.fileSize || 0}`,
            type: 'popup',
            width: 500,
            height: 400,
            focused: true
        });
    });
});

// Context menu for manual download
chrome.contextMenus.create({
    id: 'download-with-boltdm',
    title: 'Download with BoltDM',
    contexts: ['link']
});

chrome.contextMenus.onClicked.addListener((info, tab) => {
    if (info.menuItemId === 'download-with-boltdm' && info.linkUrl) {
        // Create a fake download item for the dialog
        const downloadId = Date.now();
        pendingDownloads.set(downloadId, {
            url: info.linkUrl,
            filename: '',
            fileSize: 0,
            mime: ''
        });

        chrome.windows.create({
            url: `dialog.html?id=${downloadId}&url=${encodeURIComponent(info.linkUrl)}&filename=&size=0`,
            type: 'popup',
            width: 500,
            height: 400,
            focused: true
        });
    }
});

// Listen for messages from dialog
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === 'startDownload') {
        const success = sendToBoltDM(request.url, request.filename, request.savePath);
        sendResponse({ success: success });

        if (success) {
            showNotification('BoltDM', `Downloading: ${request.filename}`);
        }
    } else if (request.action === 'cancelDownload') {
        sendResponse({ success: true });
    } else if (request.action === 'toggleIntercept') {
        interceptEnabled = request.enabled;
        sendResponse({ success: true, interceptEnabled });
    } else if (request.action === 'getInterceptState') {
        sendResponse({ interceptEnabled: interceptEnabled });
    }
    return true;
});

// Initialize
connectNative();
