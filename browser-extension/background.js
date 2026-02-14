// Bolt Download Manager Browser Extension
// Copyright (c) 2026 changcheng967. All rights reserved.

const NATIVE_HOST_NAME = 'com.boltdm.host';

// Native messaging port
let nativePort = null;

// Connect to native host
function connectNative() {
    try {
        nativePort = chrome.runtime.connectNative(NATIVE_HOST_NAME);

        nativePort.onMessage.addListener((response) => {
            console.log('BoltDM response:', response);
            if (response.success) {
                showNotification('Download Added', response.message);
            } else {
                showNotification('Error', response.message);
            }
        });

        nativePort.onDisconnect.addListener(() => {
            console.log('Disconnected from native host');
            nativePort = null;
        });
    } catch (e) {
        console.error('Failed to connect to native host:', e);
    }
}

// Send download to BoltDM
function sendToBoltDM(url, filename = '', referrer = '') {
    if (!nativePort) {
        connectNative();
    }

    if (nativePort) {
        nativePort.postMessage({
            url: url,
            filename: filename,
            referrer: referrer
        });
    } else {
        showNotification('Error', 'Could not connect to Bolt Download Manager');
    }
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

// Context menu: Download with BoltDM
chrome.contextMenus.create({
    id: 'download-with-boltdm',
    title: 'Download with BoltDM',
    contexts: ['link']
});

// Handle context menu click
chrome.contextMenus.onClicked.addListener((info, tab) => {
    if (info.menuItemId === 'download-with-boltdm') {
        sendToBoltDM(info.linkUrl, '', info.pageUrl);
    }
});

// Intercept downloads (optional - user can toggle)
let interceptDownloads = false;

chrome.downloads.onCreated.addListener((downloadItem) => {
    if (interceptDownloads && downloadItem.url) {
        // Cancel browser download and send to BoltDM
        chrome.downloads.cancel(downloadItem.id);
        sendToBoltDM(downloadItem.url, downloadItem.filename);
    }
});

// Listen for messages from popup
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === 'download') {
        sendToBoltDM(request.url, request.filename, request.referrer);
        sendResponse({ success: true });
    } else if (request.action === 'toggleIntercept') {
        interceptDownloads = request.enabled;
        sendResponse({ success: true, interceptDownloads });
    }
    return true;
});

// Initialize
connectNative();
