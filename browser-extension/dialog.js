// Bolt Download Manager Download Dialog
// Copyright (c) 2026 changcheng967. All rights reserved.

document.addEventListener('DOMContentLoaded', async () => {
    const params = new URLSearchParams(window.location.search);
    const url = decodeURIComponent(params.get('url') || '');
    const filename = decodeURIComponent(params.get('filename') || '');
    const size = parseInt(params.get('size') || '0');

    // Display info
    document.getElementById('urlDisplay').textContent = url;
    document.getElementById('filenameDisplay').textContent = filename || extractFilename(url);
    document.getElementById('filenameInput').value = filename || extractFilename(url);
    document.getElementById('fileSize').textContent = size > 0 ? formatSize(size) : 'Unknown (will detect)';

    // Default save path
    const defaultPath = 'C:\\Users\\chang\\Downloads';
    document.getElementById('savePath').value = defaultPath;

    // Browse button - open folder picker (works in Chrome with downloads API)
    document.getElementById('browseBtn').addEventListener('click', async () => {
        // We can't directly open a folder picker, but we can show a message
        // In a real implementation, this would open a native file dialog
        alert('In the full version, this opens a folder browser. For now, edit the path manually.\n\nThe native host can show a real dialog.');
    });

    // Cancel button
    document.getElementById('cancelBtn').addEventListener('click', () => {
        chrome.runtime.sendMessage({ action: 'cancelDownload' });
        window.close();
    });

    // Download button
    document.getElementById('downloadBtn').addEventListener('click', () => {
        const filename = document.getElementById('filenameInput').value.trim();
        const savePath = document.getElementById('savePath').value.trim();

        if (!filename) {
            alert('Please enter a filename');
            return;
        }

        // Combine path and filename
        const fullPath = savePath.endsWith('\\')
            ? savePath + filename
            : savePath + '\\' + filename;

        chrome.runtime.sendMessage({
            action: 'startDownload',
            url: url,
            filename: filename,
            savePath: fullPath
        }, (response) => {
            if (response && response.success) {
                window.close();
            } else {
                alert('Failed to start download. Is BoltDM running?');
            }
        });
    });

    // Focus download button
    document.getElementById('downloadBtn').focus();
});

// Extract filename from URL
function extractFilename(url) {
    try {
        const urlObj = new URL(url);
        const pathParts = urlObj.pathname.split('/');
        let filename = pathParts[pathParts.length - 1];

        // Decode URL encoding
        filename = decodeURIComponent(filename);

        // If no extension, add a generic one
        if (filename && !filename.includes('.')) {
            filename += '.download';
        }

        return filename || 'download';
    } catch (e) {
        return 'download';
    }
}

// Format file size
function formatSize(bytes) {
    if (bytes >= 1024 * 1024 * 1024) {
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
    } else if (bytes >= 1024 * 1024) {
        return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    } else if (bytes >= 1024) {
        return (bytes / 1024).toFixed(0) + ' KB';
    }
    return bytes + ' bytes';
}
