// Bolt Download Manager Popup
// Copyright (c) 2026 changcheng967. All rights reserved.

document.addEventListener('DOMContentLoaded', () => {
    const urlInput = document.getElementById('url');
    const filenameInput = document.getElementById('filename');
    const downloadBtn = document.getElementById('downloadBtn');
    const interceptToggle = document.getElementById('interceptToggle');
    const statusDiv = document.getElementById('status');

    // Check clipboard for URL on focus
    urlInput.addEventListener('focus', async () => {
        try {
            const clipText = await navigator.clipboard.readText();
            if (clipText && (clipText.startsWith('http://') || clipText.startsWith('https://'))) {
                urlInput.value = clipText;
                // Extract filename from URL
                const url = new URL(clipText);
                const pathParts = url.pathname.split('/');
                const possibleFilename = pathParts[pathParts.length - 1];
                if (possibleFilename && possibleFilename.includes('.')) {
                    filenameInput.value = possibleFilename;
                }
            }
        } catch (e) {
            // Clipboard access denied - ignore
        }
    });

    // Add download
    downloadBtn.addEventListener('click', () => {
        const url = urlInput.value.trim();
        if (!url) {
            statusDiv.textContent = 'Please enter a URL';
            statusDiv.className = 'status disconnected';
            return;
        }

        chrome.runtime.sendMessage({
            action: 'download',
            url: url,
            filename: filenameInput.value.trim()
        }, (response) => {
            if (response && response.success) {
                statusDiv.textContent = 'Download added!';
                statusDiv.className = 'status connected';
                urlInput.value = '';
                filenameInput.value = '';
            } else {
                statusDiv.textContent = 'Failed to add download';
                statusDiv.className = 'status disconnected';
            }
        });
    });

    // Toggle intercept downloads
    interceptToggle.addEventListener('change', () => {
        chrome.runtime.sendMessage({
            action: 'toggleIntercept',
            enabled: interceptToggle.checked
        });
    });
});
