#include "Flexifi.h"
#include "PortalWebServer.h"
#include "StorageManager.h"
#include "TemplateManager.h"
#include "FlexifiParameter.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <DNSServer.h>

// Static instance for WiFi event callbacks
Flexifi* Flexifi::_instance = nullptr;

Flexifi::Flexifi(AsyncWebServer* server) :
    _server(server),
    _portalServer(nullptr),
    _storage(nullptr),
    _templateManager(nullptr),
    _dnsServer(nullptr),
    _portalState(PortalState::STOPPED),
    _wifiState(WiFiState::DISCONNECTED),
    _currentSSID(""),
    _currentPassword(""),
    _apName(""),
    _apPassword(""),
    _portalTimeout(FLEXIFI_PORTAL_TIMEOUT),
    _connectTimeout(FLEXIFI_CONNECT_TIMEOUT),
    _portalStartTime(0),
    _connectStartTime(0),
    _lastScanTime(0),
    _networkCount(0),
    _networksJSON("[]"),
    _minSignalQuality(-80),
    _parameters(nullptr),
    _parameterCount(0),
    _maxParameters(10),
    _autoConnectEnabled(true),
    _lastAutoConnectAttempt(0),
    _autoConnectRetryCount(0),
    _autoConnectLimitReachedLogged(false),
    _onPortalStart(nullptr),
    _onPortalStop(nullptr),
    _onWiFiConnect(nullptr),
    _onWiFiDisconnect(nullptr),
    _onConfigSave(nullptr),
    _onScanComplete(nullptr),
    _onConnectStart(nullptr),
    _onConnectFailed(nullptr),
    _onInternalScanComplete(nullptr) {
    
    if (!_server) {
        FLEXIFI_LOGE("AsyncWebServer pointer is null");
        return;
    }
    
    // Set static instance for WiFi event callbacks
    _instance = this;
    
    // Initialize components
    _storage = new StorageManager();
    _templateManager = new TemplateManager();
    _portalServer = new PortalWebServer(_server, this);
    
    // Initialize parameters
    _initParameters();
    
    // Setup WiFi event handlers
    _setupWiFiEvents();
    
    FLEXIFI_LOGI("Flexifi initialized");
}

Flexifi::~Flexifi() {
    stopPortal();
    
    // Clear static instance
    if (_instance == this) {
        _instance = nullptr;
    }
    
    // Clean up parameters
    _clearParameters();
    
    if (_dnsServer) {
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    
    if (_portalServer) {
        delete _portalServer;
        _portalServer = nullptr;
    }
    
    if (_storage) {
        delete _storage;
        _storage = nullptr;
    }
    
    if (_templateManager) {
        delete _templateManager;
        _templateManager = nullptr;
    }
    
    FLEXIFI_LOGD("Flexifi destroyed");
}

bool Flexifi::init() {
    if (!_storage || !_templateManager || !_portalServer) {
        FLEXIFI_LOGE("Components not properly initialized");
        return false;
    }
    
    // Initialize storage
    if (!_storage->init()) {
        FLEXIFI_LOGE("Failed to initialize storage manager");
        return false;
    }
    
    // Load configuration
    loadConfig();
    
    FLEXIFI_LOGI("Flexifi initialization completed");
    return true;
}

void Flexifi::setTemplate(const String& templateName) {
    if (_templateManager) {
        _templateManager->setTemplate(templateName);
        FLEXIFI_LOGI("Template set to: %s", templateName.c_str());
    }
}

void Flexifi::setCustomTemplate(const String& htmlTemplate) {
    if (_templateManager) {
        _templateManager->setCustomTemplate(htmlTemplate);
        FLEXIFI_LOGI("Custom template set");
    }
}

void Flexifi::setCredentials(const String& ssid, const String& password) {
    _currentSSID = ssid;
    _currentPassword = password;
    FLEXIFI_LOGI("Credentials set for SSID: %s", ssid.c_str());
}

void Flexifi::setPortalTimeout(unsigned long timeout) {
    _portalTimeout = timeout;
    FLEXIFI_LOGD("Portal timeout set to: %lu ms", timeout);
}

void Flexifi::setConnectTimeout(unsigned long timeout) {
    _connectTimeout = timeout;
    FLEXIFI_LOGD("Connect timeout set to: %lu ms", timeout);
}

bool Flexifi::startPortal(const String& apName, const String& apPassword) {
    if (_portalState != PortalState::STOPPED) {
        FLEXIFI_LOGW("Portal already running");
        return false;
    }
    
    FLEXIFI_LOGI("Starting portal with AP: %s", apName.c_str());
    
    _apName = apName;
    _apPassword = apPassword;
    
    // Initialize storage
    if (!_storage->init()) {
        FLEXIFI_LOGE("Failed to initialize storage");
        return false;
    }
    
    // Initialize portal web server
    if (!_portalServer->init()) {
        FLEXIFI_LOGE("Failed to initialize portal web server");
        return false;
    }
    
    // Set up access point
    if (!_setupAP()) {
        FLEXIFI_LOGE("Failed to set up access point");
        return false;
    }
    
    _portalStartTime = millis();
    _onPortalStateChange(PortalState::ACTIVE);
    
    // Trigger callback
    if (_onPortalStart) {
        _onPortalStart();
    }
    
    FLEXIFI_LOGI("Portal started successfully");
    return true;
}

void Flexifi::stopPortal() {
    if (_portalState == PortalState::STOPPED) {
        return;
    }
    
    FLEXIFI_LOGI("Stopping portal");
    
    _onPortalStateChange(PortalState::STOPPING);
    
    // Stop access point
    _stopAP();
    
    // Clean up web server
    if (_portalServer) {
        _portalServer->cleanup();
    }
    
    // Clean up storage
    if (_storage) {
        _storage->deinit();
    }
    
    _onPortalStateChange(PortalState::STOPPED);
    
    // Trigger callback
    if (_onPortalStop) {
        _onPortalStop();
    }
    
    FLEXIFI_LOGI("Portal stopped");
}

bool Flexifi::isPortalActive() const {
    return _portalState == PortalState::ACTIVE;
}

PortalState Flexifi::getPortalState() const {
    return _portalState;
}

bool Flexifi::saveConfig() {
    if (!_storage) {
        FLEXIFI_LOGE("Storage not initialized");
        return false;
    }
    
    if (_currentSSID.isEmpty()) {
        FLEXIFI_LOGW("No credentials to save");
        return false;
    }
    
    // Create WiFi profile with auto-connect enabled and high priority
    WiFiProfile profile(_currentSSID, _currentPassword, 100);
    profile.autoConnect = true;
    profile.lastUsed = millis();
    
    bool success = _storage->saveWiFiProfile(profile);
    
    if (success) {
        FLEXIFI_LOGI("Configuration saved successfully");
        
        // Trigger callback
        if (_onConfigSave) {
            _onConfigSave(_currentSSID, _currentPassword);
        }
    } else {
        FLEXIFI_LOGE("Failed to save configuration");
    }
    
    return success;
}

bool Flexifi::loadConfig() {
    if (!_storage) {
        FLEXIFI_LOGE("Storage not initialized");
        return false;
    }
    
    if (!_storage->init()) {
        FLEXIFI_LOGE("Failed to initialize storage");
        return false;
    }
    
    String ssid, password;
    bool success = _storage->loadCredentials(ssid, password);
    
    if (success) {
        _currentSSID = ssid;
        _currentPassword = password;
        FLEXIFI_LOGI("Configuration loaded: %s", ssid.c_str());
    } else {
        FLEXIFI_LOGD("No saved configuration found");
    }
    
    return success;
}

void Flexifi::clearConfig() {
    if (_storage) {
        _storage->clearCredentials();
        FLEXIFI_LOGI("Configuration cleared");
    }
    
    _currentSSID = "";
    _currentPassword = "";
}

void Flexifi::addParameter(FlexifiParameter* parameter) {
    if (!parameter) {
        FLEXIFI_LOGW("Cannot add null parameter");
        return;
    }
    
    if (_addParameterToArray(parameter)) {
        FLEXIFI_LOGI("Added parameter: %s", parameter->getID().c_str());
    } else {
        FLEXIFI_LOGE("Failed to add parameter: %s", parameter->getID().c_str());
    }
}

void Flexifi::addParameter(const String& id, const String& label, const String& defaultValue, int maxLength) {
    FlexifiParameter* param = new FlexifiParameter(id, label, defaultValue, maxLength);
    addParameter(param);
}

FlexifiParameter* Flexifi::getParameter(const String& id) {
    int index = _findParameterIndex(id);
    if (index >= 0) {
        return _parameters[index];
    }
    return nullptr;
}

String Flexifi::getParameterValue(const String& id) const {
    int index = _findParameterIndex(id);
    if (index >= 0) {
        return _parameters[index]->getValue();
    }
    return "";
}

void Flexifi::setParameterValue(const String& id, const String& value) {
    int index = _findParameterIndex(id);
    if (index >= 0) {
        _parameters[index]->setValue(value);
        FLEXIFI_LOGD("Set parameter %s = %s", id.c_str(), value.c_str());
    } else {
        FLEXIFI_LOGW("Parameter not found: %s", id.c_str());
    }
}

int Flexifi::getParameterCount() const {
    return _parameterCount;
}

String Flexifi::getParametersHTML() const {
    String html = "";
    
    for (int i = 0; i < _parameterCount; i++) {
        html += _parameters[i]->generateHTML();
    }
    
    return html;
}

void Flexifi::setMinSignalQuality(int quality) {
    _minSignalQuality = quality;
    FLEXIFI_LOGD("Minimum signal quality set to: %d dBm", quality);
}

int Flexifi::getMinSignalQuality() const {
    return _minSignalQuality;
}

bool Flexifi::scanNetworks() {
    if (_wifiState == WiFiState::CONNECTING) {
        FLEXIFI_LOGW("Cannot scan while connecting");
        return false;
    }
    
    unsigned long now = millis();
    if (now - _lastScanTime < 30000) { // Throttle to 30 seconds between scans
        FLEXIFI_LOGD("Scan throttled - too soon since last scan (%lu ms ago)", now - _lastScanTime);
        return false;
    }
    
    FLEXIFI_LOGI("Starting WiFi scan");
    _lastScanTime = now;
    
    // Check current scan status first
    int currentScanStatus = WiFi.scanComplete();
    FLEXIFI_LOGD("Current scan status before new scan: %d", currentScanStatus);
    
    // If a scan is already running, don't start another
    if (currentScanStatus == WIFI_SCAN_RUNNING) {
        FLEXIFI_LOGD("Scan already in progress, waiting...");
        return true; // Scan is running, so consider this successful
    }
    
    // Ensure WiFi is in the right mode for scanning
    wifi_mode_t currentMode = WiFi.getMode();
    FLEXIFI_LOGD("Current WiFi mode: %d", currentMode);
    
    // Force WiFi to AP+STA mode if needed
    if (currentMode != WIFI_MODE_APSTA) {
        FLEXIFI_LOGW("WiFi mode is %d, forcing to AP+STA (%d) for scanning", currentMode, WIFI_MODE_APSTA);
        WiFi.mode(WIFI_MODE_APSTA);
        delay(200); // Give more time for mode change
    }
    
    // Clear any previous scan results
    if (currentScanStatus >= 0 || currentScanStatus == WIFI_SCAN_FAILED) {
        FLEXIFI_LOGD("Clearing previous scan results");
        WiFi.scanDelete();
        delay(50); // Small delay after clearing
    }
    
    // Try to start async scan with retries
    int scanResult = WIFI_SCAN_FAILED;
    for (int retry = 0; retry < 3; retry++) {
        scanResult = WiFi.scanNetworks(true);
        FLEXIFI_LOGD("Scan initiation attempt %d result: %d", retry + 1, scanResult);
        
        if (scanResult == WIFI_SCAN_RUNNING) {
            FLEXIFI_LOGI("WiFi scan started successfully");
            break;
        } else {
            FLEXIFI_LOGW("Scan start failed with result: %d, retry in 100ms", scanResult);
            delay(100);
        }
    }
    
    if (scanResult != WIFI_SCAN_RUNNING) {
        FLEXIFI_LOGE("Failed to start WiFi scan after 3 attempts, result: %d", scanResult);
        return false;
    }
    
    // Notify via WebSocket
    if (_portalServer) {
        _portalServer->broadcastMessage("scan_start", "Scanning for networks...");
    }
    
    return true;
}

String Flexifi::getNetworksJSON() const {
    return _networksJSON;
}

unsigned long Flexifi::getScanTimeRemaining() const {
    unsigned long now = millis();
    unsigned long timeSinceLastScan = now - _lastScanTime;
    
    if (timeSinceLastScan >= 30000) {
        return 0; // Can scan now
    }
    
    return 30000 - timeSinceLastScan; // Time remaining in ms
}

bool Flexifi::connectToWiFi(const String& ssid, const String& password) {
    if (ssid.isEmpty()) {
        FLEXIFI_LOGW("Cannot connect to empty SSID");
        return false;
    }
    
    if (_wifiState == WiFiState::CONNECTING) {
        FLEXIFI_LOGW("Already connecting to network");
        return false;
    }
    
    FLEXIFI_LOGI("Attempting to connect to: %s", ssid.c_str());
    
    // Save credentials
    _currentSSID = ssid;
    _currentPassword = password;
    
    // Disconnect from current network
    WiFi.disconnect();
    
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    
    // Start connection
    WiFi.begin(ssid.c_str(), password.c_str());
    
    _connectStartTime = millis();
    _onWiFiStateChange(WiFiState::CONNECTING);
    
    // Trigger callback
    if (_onConnectStart) {
        _onConnectStart(ssid);
    }
    
    // Notify via WebSocket
    if (_portalServer) {
        _portalServer->broadcastMessage("connect_start", "Connecting to " + ssid);
    }
    
    return true;
}

WiFiState Flexifi::getWiFiState() const {
    return _wifiState;
}

String Flexifi::getConnectedSSID() const {
    if (_wifiState == WiFiState::CONNECTED) {
        return WiFi.SSID();
    }
    return "";
}

void Flexifi::onPortalStart(std::function<void()> callback) {
    _onPortalStart = callback;
}

void Flexifi::onPortalStop(std::function<void()> callback) {
    _onPortalStop = callback;
}

void Flexifi::onWiFiConnect(std::function<void(const String&)> callback) {
    _onWiFiConnect = callback;
}

void Flexifi::onWiFiDisconnect(std::function<void()> callback) {
    _onWiFiDisconnect = callback;
}

void Flexifi::onConfigSave(std::function<void(const String&, const String&)> callback) {
    _onConfigSave = callback;
}

void Flexifi::onScanComplete(std::function<void(int)> callback) {
    _onScanComplete = callback;
}

void Flexifi::onConnectStart(std::function<void(const String&)> callback) {
    _onConnectStart = callback;
}

void Flexifi::onConnectFailed(std::function<void(const String&)> callback) {
    _onConnectFailed = callback;
}

void Flexifi::loop() {
    // Process DNS requests for captive portal
    if (_dnsServer && _portalState == PortalState::ACTIVE) {
        _dnsServer->processNextRequest();
    }
    
    // Handle WiFi events
    _handleWiFiEvents();
    
    // Check timeouts
    _checkTimeouts();
    
    // Update network scan results only if we have an active or completed scan
    int scanStatus = WiFi.scanComplete();
    if (scanStatus != WIFI_SCAN_FAILED) {
        _updateNetworksJSON();
    }
    
    // Handle auto-connect if enabled and not connected
    if (_autoConnectEnabled && 
        _wifiState == WiFiState::DISCONNECTED && 
        _portalState == PortalState::STOPPED) {
        
        static unsigned long lastAutoConnectLog = 0;
        unsigned long now = millis();
        if (now - lastAutoConnectLog > 5000) { // Log every 5 seconds to avoid spam
            FLEXIFI_LOGD("Auto-connect conditions met, calling autoConnect()");
            lastAutoConnectLog = now;
        }
        
        autoConnect();
    }
}

void Flexifi::reset() {
    FLEXIFI_LOGI("Resetting Flexifi configuration");
    
    // Clear stored credentials
    clearConfig();
    
    // Disconnect from WiFi
    WiFi.disconnect();
    
    // Reset state
    _onWiFiStateChange(WiFiState::DISCONNECTED);
    
    // Clear network list
    _networksJSON = "[]";
    _networkCount = 0;
}

String Flexifi::getStatusJSON() const {
    DynamicJsonDocument doc(512);
    
    doc["portal_state"] = static_cast<int>(_portalState);
    doc["wifi_state"] = static_cast<int>(_wifiState);
    doc["connected_ssid"] = getConnectedSSID();
    doc["network_count"] = _networkCount;
    doc["uptime"] = millis();
    
    if (_portalState == PortalState::ACTIVE) {
        doc["portal_uptime"] = millis() - _portalStartTime;
    }
    
    String json;
    serializeJson(doc, json);
    return json;
}

String Flexifi::getPortalHTML() const {
    if (!_templateManager) {
        return "<html><body><h1>Flexifi Portal</h1><p>Template manager not initialized</p></body></html>";
    }
    
    // Generate custom parameters HTML
    String customParametersHTML = getParametersHTML();
    
    return _templateManager->getPortalHTML(customParametersHTML);
}

// Private methods

bool Flexifi::_setupAP() {
    FLEXIFI_LOGD("Setting up access point");
    
    // Set WiFi mode to AP+STA to allow both AP and scanning
    WiFi.mode(WIFI_MODE_APSTA);
    
    // Configure AP
    bool success;
    if (_apPassword.isEmpty()) {
        success = WiFi.softAP(_apName.c_str());
    } else {
        success = WiFi.softAP(_apName.c_str(), _apPassword.c_str());
    }
    
    if (!success) {
        FLEXIFI_LOGE("Failed to start access point");
        return false;
    }
    
    // Wait for AP to be ready
    delay(100);
    
    IPAddress ip = WiFi.softAPIP();
    FLEXIFI_LOGI("Access point started - IP: %s", ip.toString().c_str());
    
    // Set up DNS server for captive portal
    if (!_dnsServer) {
        _dnsServer = new DNSServer();
    }
    
    // Start DNS server - redirect all DNS queries to our AP IP
    if (_dnsServer->start(53, "*", ip)) {
        FLEXIFI_LOGI("DNS server started for captive portal");
    } else {
        FLEXIFI_LOGW("Failed to start DNS server");
    }
    
    // Wait a moment for AP to stabilize before starting scan
    delay(500);
    
    // Start an initial network scan to populate the list
    _lastScanTime = 0; // Reset scan throttle
    FLEXIFI_LOGI("Initiating first network scan after AP setup");
    
    // Force start the initial scan by temporarily bypassing throttle
    unsigned long savedScanTime = _lastScanTime;
    _lastScanTime = millis() - 31000; // Set to over 30 seconds ago
    scanNetworks();
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
        _lastScanTime = savedScanTime; // Restore if scan didn't start
    }
    
    return true;
}

void Flexifi::_stopAP() {
    FLEXIFI_LOGD("Stopping access point");
    
    // Stop DNS server
    if (_dnsServer) {
        _dnsServer->stop();
        FLEXIFI_LOGD("DNS server stopped");
    }
    
    // Invalidate network cache since WiFi mode is changing
    _networksJSON = "[]";
    _networkCount = 0;
    _lastScanTime = 0; // Force fresh scan for auto-connect
    
    WiFi.softAPdisconnect(true);
    
    // Switch to STA mode to allow auto-connect to work properly
    WiFi.mode(WIFI_STA);
}

void Flexifi::_handleWiFiEvents() {
    // Handle WiFi connection state changes
    if (_wifiState == WiFiState::CONNECTING) {
        wl_status_t status = WiFi.status();
        
        if (status == WL_CONNECTED) {
            FLEXIFI_LOGI("WiFi connected successfully");
            _onWiFiStateChange(WiFiState::CONNECTED);
            
            // Save configuration
            saveConfig();
            
            // Trigger callback
            if (_onWiFiConnect) {
                _onWiFiConnect(_currentSSID);
            }
            
            // Notify via WebSocket
            if (_portalServer) {
                _portalServer->broadcastMessage("connect_success", "Connected to " + _currentSSID);
            }
            
        } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
            FLEXIFI_LOGW("WiFi connection failed");
            _onWiFiStateChange(WiFiState::FAILED);
            
            // Trigger callback
            if (_onConnectFailed) {
                _onConnectFailed(_currentSSID);
            }
            
            // Notify via WebSocket
            if (_portalServer) {
                _portalServer->broadcastMessage("connect_failed", "Failed to connect to " + _currentSSID);
            }
        }
    }
    
    // Handle disconnection
    if (_wifiState == WiFiState::CONNECTED && WiFi.status() != WL_CONNECTED) {
        FLEXIFI_LOGW("WiFi disconnected");
        _onWiFiStateChange(WiFiState::DISCONNECTED);
        
        // Trigger callback
        if (_onWiFiDisconnect) {
            _onWiFiDisconnect();
        }
    }
}

void Flexifi::_checkTimeouts() {
    unsigned long now = millis();
    
    // Check connection timeout
    if (_wifiState == WiFiState::CONNECTING && 
        now - _connectStartTime > _connectTimeout) {
        FLEXIFI_LOGW("Connection timeout");
        _onWiFiStateChange(WiFiState::FAILED);
        
        // Trigger callback
        if (_onConnectFailed) {
            _onConnectFailed(_currentSSID);
        }
        
        // Notify via WebSocket
        if (_portalServer) {
            _portalServer->broadcastMessage("connect_failed", "Connection timeout");
        }
    }
    
    // Check portal timeout
    if (_portalState == PortalState::ACTIVE && 
        _portalTimeout > 0 && 
        now - _portalStartTime > _portalTimeout) {
        FLEXIFI_LOGI("Portal timeout reached");
        stopPortal();
    }
}

void Flexifi::_updateNetworksJSON() {
    int scanResult = WiFi.scanComplete();
    
    if (scanResult == WIFI_SCAN_RUNNING) {
        // Scan still in progress
        return;
    }
    
    if (scanResult >= 0) {
        // Scan completed
        FLEXIFI_LOGD("WiFi scan completed, found %d networks", scanResult);
        
        // Build JSON array with quality filtering
        DynamicJsonDocument doc(2048);
        JsonArray networks = doc.to<JsonArray>();
        int filteredCount = 0;
        
        for (int i = 0; i < scanResult; i++) {
            int rssi = WiFi.RSSI(i);
            String ssid = WiFi.SSID(i);
            
            // Skip networks that don't meet quality threshold
            if (!_networkMeetsQuality(rssi)) {
                FLEXIFI_LOGD("Filtering out weak network: %s (%d dBm)", ssid.c_str(), rssi);
                continue;
            }
            
            // Skip networks with empty SSID
            if (ssid.isEmpty()) {
                continue;
            }
            
            JsonObject network = networks.createNestedObject();
            network["ssid"] = ssid;
            network["rssi"] = rssi;
            network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            network["channel"] = WiFi.channel(i);
            network["signal_strength"] = _getSignalStrengthIcon(rssi);
            filteredCount++;
        }
        
        _networkCount = filteredCount;
        
        // Clear and rebuild the JSON string
        _networksJSON = "";
        serializeJson(doc, _networksJSON);
        
        // Clear scan results
        WiFi.scanDelete();
        
        // Trigger callback with filtered count
        if (_onScanComplete) {
            _onScanComplete(filteredCount);
        }
        
        FLEXIFI_LOGI("Network scan completed: %d total, %d after filtering", scanResult, filteredCount);
        
        // Reset scan time to prevent immediate re-scanning
        _lastScanTime = millis();
        
        // Notify via WebSocket
        if (_portalServer) {
            _portalServer->broadcastNetworks(_networksJSON);
        }
    } else if (scanResult == WIFI_SCAN_FAILED) {
        // Only log failure once every 30 seconds to avoid spam
        static unsigned long lastFailureLog = 0;
        unsigned long now = millis();
        
        if (now - lastFailureLog > 30000) {
            FLEXIFI_LOGW("WiFi scan failed (scanResult: %d, WiFi mode: %d)", scanResult, WiFi.getMode());
            lastFailureLog = now;
            
            // Try to restart scanning after a delay
            if (now - _lastScanTime > 15000) { // Only retry every 15 seconds
                FLEXIFI_LOGI("Attempting to restart WiFi scanning due to persistent failures");
                _lastScanTime = 0; // Reset throttle
                
                // Force a new scan attempt
                scanNetworks();
            }
        }
        
        _networksJSON = "[]";
        _networkCount = 0;
    }
}

bool Flexifi::_validateCredentials(const String& ssid, const String& password) {
    if (ssid.isEmpty()) {
        return false;
    }
    
    if (ssid.length() > 32) {
        return false;
    }
    
    if (password.length() > 64) {
        return false;
    }
    
    return true;
}

void Flexifi::_onPortalStateChange(PortalState newState) {
    PortalState oldState = _portalState;
    _portalState = newState;
    
    FLEXIFI_LOGD("Portal state changed: %d -> %d", static_cast<int>(oldState), static_cast<int>(newState));
}

void Flexifi::_onWiFiStateChange(WiFiState newState) {
    WiFiState oldState = _wifiState;
    _wifiState = newState;
    
    FLEXIFI_LOGD("WiFi state changed: %d -> %d", static_cast<int>(oldState), static_cast<int>(newState));
}

// Parameter management methods

void Flexifi::_initParameters() {
    _parameters = new FlexifiParameter*[_maxParameters];
    for (int i = 0; i < _maxParameters; i++) {
        _parameters[i] = nullptr;
    }
    _parameterCount = 0;
    FLEXIFI_LOGD("Parameter system initialized (max: %d)", _maxParameters);
}

void Flexifi::_clearParameters() {
    if (_parameters) {
        for (int i = 0; i < _parameterCount; i++) {
            if (_parameters[i]) {
                delete _parameters[i];
                _parameters[i] = nullptr;
            }
        }
        delete[] _parameters;
        _parameters = nullptr;
    }
    _parameterCount = 0;
    FLEXIFI_LOGD("Parameters cleared");
}

int Flexifi::_findParameterIndex(const String& id) const {
    for (int i = 0; i < _parameterCount; i++) {
        if (_parameters[i] && _parameters[i]->getID() == id) {
            return i;
        }
    }
    return -1;
}

bool Flexifi::_addParameterToArray(FlexifiParameter* parameter) {
    if (!parameter) {
        return false;
    }
    
    // Check if parameter already exists
    if (_findParameterIndex(parameter->getID()) >= 0) {
        FLEXIFI_LOGW("Parameter already exists: %s", parameter->getID().c_str());
        return false;
    }
    
    // Check if we have space
    if (_parameterCount >= _maxParameters) {
        FLEXIFI_LOGE("Maximum parameters reached (%d)", _maxParameters);
        return false;
    }
    
    // Add parameter
    _parameters[_parameterCount] = parameter;
    _parameterCount++;
    
    return true;
}

// Network filtering methods

bool Flexifi::_networkMeetsQuality(int rssi) const {
    return rssi >= _minSignalQuality;
}

String Flexifi::_getSignalStrengthIcon(int rssi) const {
    if (rssi > -50) {
        return "📶📶📶📶"; // Excellent
    } else if (rssi > -60) {
        return "📶📶📶"; // Good
    } else if (rssi > -70) {
        return "📶📶"; // Fair
    } else if (rssi > -80) {
        return "📶"; // Poor
    } else {
        return "📵"; // Very poor
    }
}

// WiFi Profile Management

bool Flexifi::addWiFiProfile(const String& ssid, const String& password, int priority) {
    if (!_storage) {
        FLEXIFI_LOGE("Storage manager not initialized");
        return false;
    }
    
    if (ssid.isEmpty()) {
        FLEXIFI_LOGW("Cannot add WiFi profile with empty SSID");
        return false;
    }
    
    WiFiProfile profile(ssid, password, priority);
    profile.lastUsed = millis();
    
    FLEXIFI_LOGI("Adding WiFi profile: %s (priority: %d)", ssid.c_str(), priority);
    
    return _storage->saveWiFiProfile(profile);
}

bool Flexifi::updateWiFiProfile(const String& ssid, const String& password, int priority) {
    if (!_storage) {
        FLEXIFI_LOGE("Storage manager not initialized");
        return false;
    }
    
    if (ssid.isEmpty()) {
        FLEXIFI_LOGW("Cannot update WiFi profile with empty SSID");
        return false;
    }
    
    WiFiProfile profile(ssid, password, priority);
    profile.lastUsed = millis();
    
    FLEXIFI_LOGI("Updating WiFi profile: %s (priority: %d)", ssid.c_str(), priority);
    
    return _storage->updateWiFiProfile(ssid, profile);
}

bool Flexifi::deleteWiFiProfile(const String& ssid) {
    if (!_storage) {
        FLEXIFI_LOGE("Storage manager not initialized");
        return false;
    }
    
    if (ssid.isEmpty()) {
        FLEXIFI_LOGW("Cannot delete WiFi profile with empty SSID");
        return false;
    }
    
    FLEXIFI_LOGI("Deleting WiFi profile: %s", ssid.c_str());
    
    return _storage->deleteWiFiProfile(ssid);
}

bool Flexifi::hasWiFiProfile(const String& ssid) {
    if (!_storage) {
        return false;
    }
    
    return _storage->hasWiFiProfile(ssid);
}

void Flexifi::clearAllWiFiProfiles() {
    if (!_storage) {
        FLEXIFI_LOGE("Storage manager not initialized");
        return;
    }
    
    FLEXIFI_LOGI("Clearing all WiFi profiles");
    _storage->clearAllWiFiProfiles();
}

int Flexifi::getWiFiProfileCount() {
    if (!_storage) {
        return 0;
    }
    
    return _storage->getProfileCount();
}

String Flexifi::getWiFiProfilesJSON() const {
    if (!_storage) {
        return "[]";
    }
    
    std::vector<WiFiProfile> profiles = _storage->getProfilesByPriority();
    return _formatProfilesJSON(profiles);
}

// Auto-connect functionality

bool Flexifi::autoConnect() {
    if (!_autoConnectEnabled || !_storage) {
        return false;
    }
    
    unsigned long currentTime = millis();
    
    // Check if we should attempt auto-connect
    if (currentTime - _lastAutoConnectAttempt < AUTO_CONNECT_RETRY_DELAY) {
        return false;
    }
    
    // Check if we've exceeded retry count
    if (_autoConnectRetryCount >= MAX_AUTO_CONNECT_RETRIES) {
        // Only log the warning once to avoid spam
        if (!_autoConnectLimitReachedLogged) {
            FLEXIFI_LOGW("Auto-connect retry limit reached");
            _autoConnectLimitReachedLogged = true;
        }
        return false;
    }
    
    FLEXIFI_LOGI("Attempting auto-connect (attempt %d/%d)", _autoConnectRetryCount + 1, MAX_AUTO_CONNECT_RETRIES);
    
    _lastAutoConnectAttempt = currentTime;
    _autoConnectRetryCount++;
    
    return _tryConnectToProfiles();
}

void Flexifi::setAutoConnectEnabled(bool enabled) {
    _autoConnectEnabled = enabled;
    if (enabled) {
        _autoConnectRetryCount = 0;
        _lastAutoConnectAttempt = 0;
        _autoConnectLimitReachedLogged = false;
        FLEXIFI_LOGI("Auto-connect enabled");
    } else {
        FLEXIFI_LOGI("Auto-connect disabled");
    }
}

bool Flexifi::isAutoConnectEnabled() const {
    return _autoConnectEnabled;
}

String Flexifi::getHighestPrioritySSID() const {
    if (!_storage) {
        return "";
    }
    
    WiFiProfile highestPriorityProfile = _storage->getHighestPriorityProfile();
    return highestPriorityProfile.ssid;
}

bool Flexifi::updateProfileLastUsed(const String& ssid) {
    if (!_storage) {
        return false;
    }
    
    return _storage->updateProfileLastUsed(ssid);
}

// Profile management helpers

bool Flexifi::_tryConnectToProfiles() {
    if (!_storage) {
        return false;
    }
    
    std::vector<WiFiProfile> profiles = _storage->getProfilesByPriority();
    
    if (profiles.empty()) {
        FLEXIFI_LOGD("No WiFi profiles available for auto-connect");
        return false;
    }
    
    // Scan for available networks only if we don't have recent results
    unsigned long now = millis();
    if (_networksJSON == "[]" || _networkCount == 0 || (now - _lastScanTime) > 60000) {
        FLEXIFI_LOGD("Scanning for networks before auto-connect");
        
        // Set up callback for when scan completes
        _onInternalScanComplete = [this, profiles](int networkCount) {
            FLEXIFI_LOGD("Scan completed for auto-connect, proceeding with connection attempts");
            _tryConnectWithProfiles(profiles);
            _onInternalScanComplete = nullptr; // Clear callback
        };
        
        scanNetworks();
        
        // If scan just started, defer the connection attempt
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
            FLEXIFI_LOGD("Scan in progress, deferring auto-connect");
            return true; // Return true because scan was started
        }
    } else {
        FLEXIFI_LOGD("Using cached network list for auto-connect (%d networks)", _networkCount);
    }
    
    // Try to connect to the highest priority available network
    for (const WiFiProfile& profile : profiles) {
        if (!profile.autoConnect) {
            continue;
        }
        
        // Check if this network is available
        if (_networksJSON.indexOf(profile.ssid) >= 0) {
            FLEXIFI_LOGI("Attempting to connect to profile: %s (priority: %d)", 
                        profile.ssid.c_str(), profile.priority);
            
            if (connectToWiFi(profile.ssid, profile.password)) {
                // Reset retry count on successful connection
                _autoConnectRetryCount = 0;
                _autoConnectLimitReachedLogged = false;
                _storage->updateProfileLastUsed(profile.ssid);
                return true;
            }
        }
    }
    
    FLEXIFI_LOGD("No available WiFi profiles found for auto-connect");
    return false;
}

bool Flexifi::_connectToHighestPriorityNetwork() {
    if (!_storage) {
        return false;
    }
    
    WiFiProfile highestPriorityProfile = _storage->getHighestPriorityProfile();
    
    if (!highestPriorityProfile.isValid()) {
        FLEXIFI_LOGD("No valid WiFi profiles available");
        return false;
    }
    
    FLEXIFI_LOGI("Connecting to highest priority network: %s", highestPriorityProfile.ssid.c_str());
    
    return connectToWiFi(highestPriorityProfile.ssid, highestPriorityProfile.password);
}

void Flexifi::_updateProfilePriorities() {
    if (!_storage) {
        return;
    }
    
    // This method could be used to automatically adjust priorities based on usage patterns
    // For now, it's a placeholder for future enhancement
    FLEXIFI_LOGD("Profile priorities update (placeholder)");
}

String Flexifi::_formatProfilesJSON(const std::vector<WiFiProfile>& profiles) const {
    DynamicJsonDocument doc(1024);
    JsonArray profilesArray = doc.createNestedArray("profiles");
    
    for (const WiFiProfile& profile : profiles) {
        JsonObject profileObj = profilesArray.createNestedObject();
        profileObj["ssid"] = profile.ssid;
        profileObj["priority"] = profile.priority;
        profileObj["autoConnect"] = profile.autoConnect;
        profileObj["lastUsed"] = profile.lastUsed;
        // Note: password is not included in JSON for security
    }
    
    doc["count"] = profiles.size();
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    return json;
}

// WiFi Event Handling

void Flexifi::_setupWiFiEvents() {
    FLEXIFI_LOGD("Setting up WiFi event handlers");
    WiFi.onEvent(_onWiFiEvent);
}

void Flexifi::_onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!_instance) return;
    
    switch (event) {
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            FLEXIFI_LOGD("WiFi scan completed event received");
            _instance->_updateNetworksJSON();
            
            // Call internal scan completion callback if set
            if (_instance->_onInternalScanComplete) {
                _instance->_onInternalScanComplete(_instance->_networkCount);
            }
            break;
            
        default:
            // We only care about scan completion for now
            break;
    }
}

bool Flexifi::_tryConnectWithProfiles(const std::vector<WiFiProfile>& profiles) {
    // Try to connect to the highest priority available network
    for (const WiFiProfile& profile : profiles) {
        if (!profile.autoConnect) {
            continue;
        }
        
        // Check if this network is available
        if (_networksJSON.indexOf(profile.ssid) >= 0) {
            FLEXIFI_LOGI("Attempting to connect to profile: %s (priority: %d)", 
                        profile.ssid.c_str(), profile.priority);
            
            if (connectToWiFi(profile.ssid, profile.password)) {
                // Reset retry count on successful connection
                _autoConnectRetryCount = 0;
                _autoConnectLimitReachedLogged = false;
                _storage->updateProfileLastUsed(profile.ssid);
                return true;
            }
        }
    }
    
    FLEXIFI_LOGD("No available WiFi profiles found for auto-connect");
    return false;
}