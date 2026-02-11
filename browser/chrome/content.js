// Copyright (c) 2026 changcheng967. All rights reserved.

// Content script for detecting media URLs
(function() {
  'use strict';

  // Detect video elements
  const observer = new MutationObserver(() => {
    document.querySelectorAll('video, audio').forEach(media => {
      if (!media.dataset.boltdm) {
        media.dataset.boltdm = '1';
        console.log('[BoltDM] Media detected:', media.src);
      }
    });
  });

  observer.observe(document.body, { childList: true, subtree: true });

  // Listen for messages from popup
  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === 'getMediaUrls') {
      const urls = [];
      document.querySelectorAll('video, audio, source').forEach(el => {
        if (el.src) {
          urls.push(el.src);
        }
      });
      sendResponse({ urls });
    }
    return true;
  });
})();
