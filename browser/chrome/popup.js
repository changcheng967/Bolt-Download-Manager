// Copyright (c) 2026 changcheng967. All rights reserved.

document.addEventListener('DOMContentLoaded', () => {
  const btnAdd = document.getElementById('btnAdd');
  const btnOpen = document.getElementById('btnOpen');
  const status = document.getElementById('status');

  function showStatus(msg, isError = false) {
    status.textContent = msg;
    status.className = 'status ' + (isError ? 'error' : 'success');
    status.style.display = 'block';
    setTimeout(() => {
      status.style.display = 'none';
    }, 3000);
  }

  btnAdd.addEventListener('click', () => {
    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
      const url = tabs[0].url;
      if (url && (url.startsWith('http://') || url.startsWith('https://'))) {
        chrome.runtime.sendNativeMessage('com.changcheng967.boltdm', {
          url: url
        }, (response) => {
          if (chrome.runtime.lastError) {
            showStatus('BoltDM not connected', true);
          } else if (response && response.success) {
            showStatus('Download added!');
          } else {
            showStatus('Failed to add download', true);
          }
        });
      } else {
        showStatus('Not a downloadable URL', true);
      }
    });
  });

  btnOpen.addEventListener('click', () => {
    // TODO: Launch BoltDM GUI
    showStatus('BoltDM should be running');
  });
});
