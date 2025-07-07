let ws = null;
let scanInProgress = false;

function initWebSocket() {
    if ('WebSocket' in window) {
        console.log('Initializing WebSocket connection...');
        ws = new WebSocket('ws://' + window.location.host + '/ws');
        ws.onopen = function() { 
            console.log('✅ WebSocket connected successfully'); 
        };
        ws.onmessage = function(event) { 
            console.log('📨 WebSocket message received:', event.data);
            handleWebSocketMessage(event.data); 
        };
        ws.onclose = function() { 
            console.log('❌ WebSocket disconnected, reconnecting...'); 
            setTimeout(initWebSocket, 5000); 
        };
        ws.onerror = function(error) { 
            console.log('⚠️ WebSocket error:', error); 
        };
    } else {
        console.log('⚠️ WebSocket not supported by browser');
    }
}

function handleWebSocketMessage(data) {
    try {
        const msg = JSON.parse(data);
        console.log('📥 WebSocket message received:', msg);
        
        if (msg.type === 'scan_complete') {
            updateNetworks(msg.data.networks);
            scanInProgress = false;
            updateScanButton(false); // Re-enable scan button
        } else if (msg.type === 'status_update') {
            updateStatus(msg.data.status, msg.data.message);
        } else if (msg.hasOwnProperty('success')) {
            // Handle scan response (success/failure/throttled)
            console.log('📋 WebSocket scan response:', msg);
            
            if (msg.success) {
                console.log('✅ Scan initiated via WebSocket');
                // Keep scanning state - wait for scan_complete
            } else {
                // Scan failed or throttled
                console.log('⚠️ Scan failed via WebSocket:', msg.message);
                scanInProgress = false;
                updateScanButton(false); // Re-enable button
                
                if (msg.message && msg.message.includes('throttle')) {
                    updateStatus('throttled', msg.message);
                } else {
                    updateStatus('error', msg.message || 'Scan failed');
                }
                updateNetworks([]); // Clear networks
            }
        }
    } catch (e) {
        console.error('Error parsing WebSocket message:', e);
    }
}

function scanNetworks() {
    if (scanInProgress) {
        console.log('⚠️ Scan already in progress, ignoring request');
        return;
    }
    
    console.log('🔍 Starting network scan...');
    scanInProgress = true;
    updateScanButton(true); // Disable and show spinner
    updateStatus('scanning', 'Scanning for networks...');
    
    // Clear network list immediately when scan starts and ensure it's visible
    const networksList = document.getElementById('networks');
    const manualForm = document.getElementById('manualConnectForm');
    const toggleBtn = document.getElementById('manualToggleBtn');
    
    networksList.style.display = 'block';
    manualForm.style.display = 'none';
    toggleBtn.textContent = 'Enter Manually';
    updateNetworks([], true); // Pass isScanning=true to show scanning message
    
    if (ws && ws.readyState === WebSocket.OPEN) {
        console.log('📤 Sending scan request via WebSocket');
        ws.send(JSON.stringify({action: 'scan'}));
    } else {
        console.log('📤 WebSocket not available, using HTTP fetch');
        fetch('/scan')
            .then(response => {
                console.log('📡 Scan response status:', response.status);
                return response.json();
            })
            .then(data => {
                console.log('📋 Scan data received:', data);
                scanInProgress = false;
                updateScanButton(false); // Re-enable button
                
                if (data.success && data.data) {
                    console.log('✅ Scan successful, updating networks with:', data.data);
                    updateNetworks(data.data);
                } else if (data.message && data.message.includes('throttle')) {
                    // Handle throttled response
                    console.log('⏳ Scan throttled:', data.message);
                    updateStatus('throttled', data.message);
                    updateNetworks([]); // Show empty networks but not scanning
                } else {
                    console.log('⚠️ Scan response missing data or failed');
                    updateStatus('error', data.message || 'Scan failed');
                    updateNetworks([]);
                }
            })
            .catch(error => {
                console.error('❌ Scan error:', error);
                scanInProgress = false;
                updateScanButton(false); // Re-enable button
                updateStatus('error', 'Scan failed. Please try again.');
                updateNetworks([]); // Clear networks
            });
    }
}

function updateScanButton(scanning) {
    const scanBtn = document.getElementById('scanBtn');
    const scanBtnText = document.getElementById('scanBtnText');
    const scanSpinner = document.getElementById('scanSpinner');
    
    if (scanning) {
        scanBtn.disabled = true;
        scanBtnText.style.display = 'none';
        scanSpinner.style.display = 'inline-block';
    } else {
        scanBtn.disabled = false;
        scanBtnText.style.display = 'inline-block';
        scanSpinner.style.display = 'none';
    }
}

function selectNetwork(ssid) {
    document.getElementById('ssid').value = ssid;
    showManualForm(); // Automatically show the form when a network is selected
}

function showManualForm() {
    const manualForm = document.getElementById('manualConnectForm');
    const toggleBtn = document.getElementById('manualToggleBtn');
    const networksList = document.getElementById('networks');
    
    if (manualForm.style.display === 'none') {
        manualForm.style.display = 'block';
        toggleBtn.textContent = 'Hide Manual Entry';
        // Hide network list when manual entry is shown
        networksList.style.display = 'none';
    } else {
        manualForm.style.display = 'none';
        toggleBtn.textContent = 'Enter Manually';
        // Show network list when manual entry is hidden
        networksList.style.display = 'block';
    }
}

function connectToWiFi() {
    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    
    if (!ssid) {
        alert('Please enter a network name');
        return;
    }
    
    updateStatus('connecting', 'Connecting to ' + ssid + '...');
    
    const data = new FormData();
    data.append('ssid', ssid);
    data.append('password', password);
    
    // Add custom parameters to form data
    const form = document.getElementById('connectForm');
    if (form) {
        const formData = new FormData(form);
        for (let [key, value] of formData.entries()) {
            if (key !== 'ssid' && key !== 'password') {
                data.append(key, value);
            }
        }
    }
    
    fetch('/connect', {
        method: 'POST',
        body: data
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            updateStatus('connected', 'Connected successfully!');
            setTimeout(() => {
                window.location.href = '/';
            }, 3000);
        } else {
            updateStatus('failed', 'Connection failed: ' + data.message);
        }
    })
    .catch(error => {
        console.error('Connection error:', error);
        updateStatus('failed', 'Connection failed');
    });
}

function resetConfig() {
    if (confirm('Are you sure you want to reset the configuration?')) {
        fetch('/reset', {method: 'POST'})
            .then(() => {
                updateStatus('ready', 'Configuration reset');
                document.getElementById('ssid').value = '';
                document.getElementById('password').value = '';
                
                // Reset custom parameters
                const form = document.getElementById('connectForm');
                if (form) {
                    form.reset();
                }
            })
            .catch(error => console.error('Reset error:', error));
    }
}

function updateStatus(status, message) {
    const statusEl = document.getElementById('status');
    statusEl.className = 'status ' + status;
    statusEl.textContent = message || getStatusMessage(status);
}

function getStatusMessage(status) {
    switch(status) {
        case 'scanning': return '🔄 Scanning for networks...';
        case 'connecting': return '⏳ Connecting to network...';
        case 'connected': return '✅ Connected successfully!';
        case 'failed': return '❌ Connection failed';
        case 'error': return '❌ Error occurred';
        case 'throttled': return '⏳ Scan throttled - please wait';
        default: return '🔧 Ready to configure';
    }
}

function updateNetworks(networks, isScanning = false) {
    console.log('🔄 Updating networks UI with data:', networks, 'isScanning:', isScanning);
    const networksEl = document.getElementById('networks');
    
    if (isScanning) {
        // While scanning, show a scanning message instead of "No networks found"
        networksEl.innerHTML = '<p style="color: #92400e;">🔄 Scanning...</p>';
        return;
    }
    
    if (!networks || networks.length === 0) {
        console.log('⚠️ No networks to display');
        networksEl.innerHTML = '<p>No networks found</p>';
        // Update status when no networks found (only if not currently scanning and no important status is showing)
        if (!scanInProgress) {
            const statusEl = document.getElementById('status');
            const currentStatus = statusEl.className;
            // Don't override error, throttled, or connecting statuses
            if (!currentStatus.includes('error') && !currentStatus.includes('throttled') && !currentStatus.includes('connecting')) {
                updateStatus('ready', 'No networks found. Try scanning again.');
            }
        }
        return;
    }
    
    // Update status when networks are found (always override any previous status)
    updateStatus('ready', 'Select a network or enter manually');
    
    console.log('✅ Building HTML for', networks.length, 'networks');
    let html = '<div class="network-list">';
    networks.forEach(network => {
        const securityIcon = network.secure ? '🔒' : '🔓';
        let signalStrength = network.signal_strength || '📶';
        
        html += '<div class="network-item" onclick="selectNetwork(\'' + 
                network.ssid.replace(/'/g, "\\'") + '\')">';
        html += '<span class="network-name">' + network.ssid + '</span>';
        html += '<span class="network-info">' + securityIcon + ' ' + signalStrength + '</span>';
        html += '</div>';
    });
    html += '</div>';
    networksEl.innerHTML = html;
    console.log('✅ Networks UI updated successfully');
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    initWebSocket();
    
    // Set up form submission
    const form = document.getElementById('connectForm');
    if (form) {
        form.addEventListener('submit', function(e) {
            e.preventDefault();
            connectToWiFi();
        });
    }
    
    // Set up scan button
    const scanBtn = document.getElementById('scanBtn');
    if (scanBtn) {
        scanBtn.addEventListener('click', scanNetworks);
    }
    
    // Set up manual toggle button
    const manualToggleBtn = document.getElementById('manualToggleBtn');
    if (manualToggleBtn) {
        manualToggleBtn.addEventListener('click', showManualForm);
    }
    
    // Set up reset button
    const resetBtn = document.getElementById('resetBtn');
    if (resetBtn) {
        resetBtn.addEventListener('click', resetConfig);
    }
    
    // Load initial networks from server-side scan (if available)
    loadInitialNetworks();
    
    // If no networks are shown, do a scan
    setTimeout(function() {
        const networksEl = document.getElementById('networks');
        if (!networksEl.innerHTML || networksEl.innerHTML.trim() === '' || 
            networksEl.innerHTML.includes('Scanning for networks')) {
            scanNetworks();
        }
    }, 500);
});

function loadInitialNetworks() {
    console.log('🔄 Loading initial networks from /networks.json...');
    // Fetch networks.json to get the initial list from server-side scanning
    fetch('/networks.json')
        .then(response => {
            console.log('📡 networks.json response status:', response.status);
            return response.json();
        })
        .then(data => {
            console.log('📋 networks.json data received:', data);
            if (data.networks && data.networks.length > 0) {
                console.log('✅ Found', data.networks.length, 'networks, updating UI...');
                updateNetworks(data.networks);
                updateStatus('ready', 'Select a network or enter manually');
            } else {
                console.log('ℹ️ No networks found in initial data');
                updateStatus('ready', 'Click "Scan Networks" to find WiFi networks');
            }
        })
        .catch(error => {
            console.log('❌ Error loading initial networks:', error);
            updateStatus('ready', 'Click "Scan Networks" to find WiFi networks');
        });
}