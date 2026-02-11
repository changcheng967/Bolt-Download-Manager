// Copyright (c) 2026 changcheng967. All rights reserved.

const BOLT_HOST = "com.changcheng967.boltdm";

// Send download to native host
function sendToBoltDM(url, filename) {
  const message = {
    url: url,
    filename: filename || null,
    referrer: window.location.href
  };

  chrome.runtime.sendNativeMessage(BOLT_HOST, message, (response) => {
    if (chrome.runtime.lastError) {
      console.error("BoltDM error:", chrome.runtime.lastError.message);
      showNotification("Error", "Could not connect to BoltDM. Is it running?");
      return;
    }

    if (response && response.success) {
      showNotification("BoltDM", "Download added!");
    } else {
      showNotification("BoltDM", response?.message || "Failed to add download");
    }
  });
}

// Intercept downloads
chrome.downloads.onCreated.addListener((downloadItem) => {
  if (downloadItem.finalUrl) {
    chrome.downloads.cancel(downloadItem.id);
    const filename = downloadItem.filename;
    sendToBoltDM(downloadItem.finalUrl, filename);
  }
});

// Context menu
chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: "downloadWithBolt",
    title: "Download with BoltDM",
    contexts: ["link", "video", "audio"]
  });
});

chrome.contextMenus.onClicked.addListener((info, tab) => {
  if (info.menuItemId === "downloadWithBolt") {
    sendToBoltDM(info.linkUrl || info.srcUrl);
  }
});

// Notification
function showNotification(title, message) {
  chrome.notifications.create({
    type: "basic",
    iconUrl: "icon128.png",
    title: title,
    message: message
  });
}
