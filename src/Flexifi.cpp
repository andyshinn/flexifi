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

Flexifi::Flexifi(AsyncWebServer* server, bool generatePassword) :
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
    _generatedPassword(""),
    _useGeneratedPassword(generatePassword),
    _portalTimeout(FLEXIFI_PORTAL_TIMEOUT),
    _connectTimeout(FLEXIFI_CONNECT_TIMEOUT),
    _portalStartTime(0),
    _connectStartTime(0),
    _lastScanTime(0),
    _lastStorageRetry(0),
    _networkCount(0),
    _networksJSON("[]"),
    _minSignalQuality(-70),
    _mdnsHostname("flexifi"),
    _mdnsStarted(false),
    _scanInProgress(false),
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
    
    // Generate password if requested
    if (_useGeneratedPassword) {
        _generatedPassword = _generatePassword();
        FLEXIFI_LOGI("Generated portal password: %s", _generatedPassword.c_str());
    }
    
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
}

bool Flexifi::init() {
    FLEXIFI_LOGD("Initializing Flexifi");
    
    if (!_storage) {
        FLEXIFI_LOGE("Storage manager not initialized");
        return false;
    }
    
    bool storageInit = _storage->init();
    if (!storageInit) {
        FLEXIFI_LOGW("Storage initialization failed, continuing without persistent storage");
        FLEXIFI_LOGI("Storage status: %s", _storage->getStorageInfo().c_str());
        // Don't fail initialization if storage fails - we can still function without it
    } else {
        FLEXIFI_LOGI("Storage initialized successfully");
        
        // Load configuration only if storage is available
        if (!loadConfig()) {
            FLEXIFI_LOGW("No previous configuration found");
        }
    }
    
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
    FLEXIFI_LOGD("Credentials set for SSID: %s", ssid.c_str());
}

void Flexifi::setPortalTimeout(unsigned long timeout) {
    _portalTimeout = timeout;
    FLEXIFI_LOGD("Portal timeout set to: %lu ms", timeout);
}

void Flexifi::setConnectTimeout(unsigned long timeout) {
    _connectTimeout = timeout;
    FLEXIFI_LOGD("Connect timeout set to: %lu ms", timeout);
}

// mDNS Configuration
void Flexifi::setMDNSHostname(const String& hostname) {
    _mdnsHostname = hostname;
    FLEXIFI_LOGI("mDNS hostname set to: %s", hostname.c_str());
    
    // If mDNS is already started, restart it with new hostname
    if (_mdnsStarted && WiFi.isConnected()) {
        _stopMDNS();
        _startMDNS();
    }
}

String Flexifi::getMDNSHostname() const {
    return _mdnsHostname;
}

String Flexifi::getGeneratedPassword() const {
    return _generatedPassword;
}

bool Flexifi::isMDNSEnabled() const {
    return _mdnsStarted;
}

bool Flexifi::_startMDNS() {
#ifdef FLEXIFI_MDNS
    if (_mdnsStarted) {
        FLEXIFI_LOGW("mDNS already started");
        return true;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        FLEXIFI_LOGW("Cannot start mDNS - WiFi not connected");
        return false;
    }
    
    if (MDNS.begin(_mdnsHostname.c_str())) {
        _mdnsStarted = true;
        FLEXIFI_LOGI("🌐 mDNS started: http://%s.local", _mdnsHostname.c_str());
        
        // Add HTTP service
        MDNS.addService("http", "tcp", 80);
        
        // Add device information
        MDNS.addServiceTxt("http", "tcp", "device", "flexifi");
        MDNS.addServiceTxt("http", "tcp", "version", "1.0");
        
        return true;
    } else {
        FLEXIFI_LOGE("Failed to start mDNS");
        return false;
    }
#else
    FLEXIFI_LOGD("mDNS not available - FLEXIFI_MDNS not defined");
    return false;
#endif
}

void Flexifi::_stopMDNS() {
#ifdef FLEXIFI_MDNS
    if (_mdnsStarted) {
        MDNS.end();
        _mdnsStarted = false;
        FLEXIFI_LOGI("mDNS stopped");
    }
#endif
}

bool Flexifi::startPortal(const String& apName, const String& apPassword) {
    if (_portalState != PortalState::STOPPED) {
        FLEXIFI_LOGW("Portal already running");
        return false;
    }
    
    FLEXIFI_LOGI("Starting portal with AP: %s", apName.c_str());
    
    _apName = apName;
    
    // Use generated password if enabled and no password provided
    if (_useGeneratedPassword && apPassword.isEmpty()) {
        _apPassword = _generatedPassword;
        FLEXIFI_LOGI("Using generated password for portal: %s", _apPassword.c_str());
    } else {
        _apPassword = apPassword;
    }
    
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
    
    // Trigger portal start callback
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
    
    // Clear cached network data to free memory
    _networksJSON = "[]";
    _networkCount = 0;
    _scanInProgress = false;
    
    _onPortalStateChange(PortalState::STOPPED);
    
    // Trigger portal stop callback
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
        return false;
    }
    
    bool success = _storage->saveCredentials(_currentSSID, _currentPassword);
    if (success) {
        FLEXIFI_LOGI("Configuration saved: %s", _currentSSID.c_str());
        
        // Save all custom parameter values
        _saveParameterValues();
        
        // Trigger config save callback
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
        return false;
    }
    
    String ssid, password;
    bool success = _storage->loadCredentials(ssid, password);
    if (success) {
        _currentSSID = ssid;
        _currentPassword = password;
        FLEXIFI_LOGI("Configuration loaded: %s", ssid.c_str());
    }
    
    // Note: Parameter values are loaded when parameters are added (see addParameter method)
    
    return success;
}

void Flexifi::clearConfig() {
    if (_storage) {
        _storage->clearCredentials();
        _currentSSID = "";
        _currentPassword = "";
        FLEXIFI_LOGI("Configuration cleared");
    }
}

bool Flexifi::retryStorageInit() {
    if (!_storage) {
        return false;
    }
    
    bool success = _storage->retryInitialization();
    if (success) {
        // If storage became available, try to load config and parameters
        loadConfig();
        _loadParameterValues();
    }
    
    return success;
}

// WiFi Profile Management
bool Flexifi::addWiFiProfile(const String& ssid, const String& password, int priority) {
    if (!_storage) {
        return false;
    }
    
    FLEXIFI_LOGI("Adding WiFi profile: %s (priority: %d)", ssid.c_str(), priority);
    WiFiProfile profile(ssid, password, priority);
    return _storage->saveWiFiProfile(profile);
}

bool Flexifi::updateWiFiProfile(const String& ssid, const String& password, int priority) {
    return addWiFiProfile(ssid, password, priority); // saveWiFiProfile handles updates
}

bool Flexifi::deleteWiFiProfile(const String& ssid) {
    if (!_storage) {
        return false;
    }
    
    FLEXIFI_LOGI("Deleting WiFi profile: %s", ssid.c_str());
    return _storage->deleteWiFiProfile(ssid);
}

bool Flexifi::hasWiFiProfile(const String& ssid) {
    if (!_storage) {
        return false;
    }
    
    std::vector<WiFiProfile> profiles = _storage->loadWiFiProfiles();
    for (const auto& profile : profiles) {
        if (profile.ssid == ssid) {
            return true;
        }
    }
    return false;
}

void Flexifi::clearAllWiFiProfiles() {
    if (_storage) {
        _storage->clearAllWiFiProfiles();
        FLEXIFI_LOGI("All WiFi profiles cleared");
    }
}

int Flexifi::getWiFiProfileCount() const {
    if (!_storage) {
        return 0;
    }
    
    std::vector<WiFiProfile> profiles = _storage->loadWiFiProfiles();
    return profiles.size();
}

String Flexifi::getWiFiProfilesJSON() const {
    if (!_storage) {
        return "[]";
    }
    
    std::vector<WiFiProfile> profiles = _storage->loadWiFiProfiles();
    return _formatProfilesJSON(profiles);
}

// Auto-connect functionality
bool Flexifi::autoConnect() {
    if (!_autoConnectEnabled) {
        FLEXIFI_LOGD("🚫 autoConnect() called but auto-connect is disabled");
        return false;
    }
    
    if (!_storage) {
        FLEXIFI_LOGD("🚫 autoConnect() called but storage is not available");
        return false;
    }
    
    FLEXIFI_LOGI("🔍 autoConnect() called - enabled: YES, storage: YES");
    
    // Check retry limits
    unsigned long now = millis();
    if (_autoConnectRetryCount >= MAX_AUTO_CONNECT_RETRIES) {
        if (!_autoConnectLimitReachedLogged) {
            FLEXIFI_LOGW("🚫 Auto-connect retry limit reached (%d/%d)", _autoConnectRetryCount, MAX_AUTO_CONNECT_RETRIES);
            _autoConnectLimitReachedLogged = true;
        }
        return false;
    }
    
    // Check retry delay (but allow immediate first attempt)
    if (_lastAutoConnectAttempt > 0 && now - _lastAutoConnectAttempt < AUTO_CONNECT_RETRY_DELAY) {
        unsigned long remainingDelay = AUTO_CONNECT_RETRY_DELAY - (now - _lastAutoConnectAttempt);
        FLEXIFI_LOGD("🕐 Auto-connect retry delay: %lu ms remaining", remainingDelay);
        return false;
    }
    
    _lastAutoConnectAttempt = now;
    _autoConnectRetryCount++;
    
    FLEXIFI_LOGI("🔄 Starting auto-connect attempt %d/%d", _autoConnectRetryCount, MAX_AUTO_CONNECT_RETRIES);
    
    // Try to connect to saved profiles
    return _tryConnectToProfiles();
}

void Flexifi::setAutoConnectEnabled(bool enabled) {
    _autoConnectEnabled = enabled;
    FLEXIFI_LOGI("Auto-connect %s", enabled ? "enabled" : "disabled");
}

bool Flexifi::isAutoConnectEnabled() const {
    return _autoConnectEnabled;
}

String Flexifi::getHighestPrioritySSID() const {
    if (!_storage) {
        return "";
    }
    
    std::vector<WiFiProfile> profiles = _storage->loadWiFiProfiles();
    if (profiles.empty()) {
        return "";
    }
    
    // Find highest priority profile with autoConnect enabled
    WiFiProfile* highest = nullptr;
    for (auto& profile : profiles) {
        if (profile.autoConnect && (!highest || profile.priority > highest->priority)) {
            highest = &profile;
        }
    }
    
    return highest ? highest->ssid : "";
}

bool Flexifi::updateProfileLastUsed(const String& ssid) {
    if (!_storage) {
        return false;
    }
    
    return _storage->updateProfileLastUsed(ssid);
}

// Custom parameters
void Flexifi::addParameter(FlexifiParameter* parameter) {
    if (_addParameterToArray(parameter)) {
        FLEXIFI_LOGD("Parameter added: %s", parameter->getID().c_str());
        
        // Load saved value for this parameter (if any)
        _loadParameterValue(parameter);
    }
}

void Flexifi::addParameter(const String& id, const String& label, const String& defaultValue, int maxLength) {
    FlexifiParameter* param = new FlexifiParameter(id, label, defaultValue, maxLength);
    addParameter(param);
}

FlexifiParameter* Flexifi::getParameter(const String& id) {
    int index = _findParameterIndex(id);
    return (index >= 0) ? _parameters[index] : nullptr;
}

String Flexifi::getParameterValue(const String& id) const {
    int index = _findParameterIndex(id);
    return (index >= 0) ? _parameters[index]->getValue() : "";
}

void Flexifi::setParameterValue(const String& id, const String& value) {
    int index = _findParameterIndex(id);
    if (index >= 0) {
        _parameters[index]->setValue(value);
        FLEXIFI_LOGD("Parameter value set: %s = %s", id.c_str(), value.c_str());
    }
}

int Flexifi::getParameterCount() const {
    return _parameterCount;
}

String Flexifi::getParametersHTML() const {
    String html = "";
    for (int i = 0; i < _parameterCount; i++) {
        if (_parameters[i]) {
            html += _parameters[i]->generateHTML();
        }
    }
    return html;
}

// Network management
bool Flexifi::scanNetworks(bool bypassThrottle) {
    unsigned long now = millis();
    
    // Check throttle limit unless bypassed
    if (!bypassThrottle && now - _lastScanTime < FLEXIFI_SCAN_THROTTLE_TIME) {
        FLEXIFI_LOGW("🚫 Scan throttled - too soon since last scan (%lu ms ago)", now - _lastScanTime);
        return false;
    }
    
    if (bypassThrottle) {
        FLEXIFI_LOGI("⏭️ Bypassing scan throttle for initial scan");
    } else {
        FLEXIFI_LOGD("✅ Scan throttle check passed (%lu ms since last scan)", now - _lastScanTime);
    }
    
    FLEXIFI_LOGI("Starting WiFi scan");
    
    FLEXIFI_LOGD("Current scan status before new scan: %d", WiFi.scanComplete());
    
    // Ensure clean WiFi state before scanning (workaround for ESP32 scan issues)
    if (WiFi.status() == WL_CONNECTED || WiFi.status() == WL_CONNECT_FAILED) {
        FLEXIFI_LOGD("Disconnecting from WiFi before scan to ensure clean state");
        WiFi.disconnect();
        delay(100);
    }
    
    // Ensure WiFi is in the correct mode for scanning
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_AP_STA);
        delay(100); // Give time for mode to change
    }
    
    FLEXIFI_LOGD("Current WiFi mode: %d", WiFi.getMode());
    
    // Delete any previous scan results
    int scanResult = WiFi.scanComplete();
    if (scanResult >= 0) {
        FLEXIFI_LOGD("Clearing previous scan results");
        WiFi.scanDelete();
    }
    
    // Start new scan (async, don't show hidden networks)
    int result = WiFi.scanNetworks(true, false); // true = async, false = no hidden networks
    FLEXIFI_LOGD("Scan initiation attempt 1 result: %d", result);
    
    if (result == WIFI_SCAN_FAILED) {
        FLEXIFI_LOGW("WiFi scan failed to start");
        return false;
    }
    
    FLEXIFI_LOGI("WiFi scan started successfully");
    _lastScanTime = now;
    _scanInProgress = true;
    return true;
}

String Flexifi::getNetworksJSON() const {
    return _networksJSON;
}

unsigned long Flexifi::getScanTimeRemaining() const {
    unsigned long now = millis();
    unsigned long elapsed = now - _lastScanTime;
    return (elapsed < FLEXIFI_SCAN_THROTTLE_TIME) ? (FLEXIFI_SCAN_THROTTLE_TIME - elapsed) : 0;
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

void Flexifi::setMinSignalQuality(int quality) {
    _minSignalQuality = quality;
    FLEXIFI_LOGD("Minimum signal quality set to: %d dBm", quality);
}

int Flexifi::getMinSignalQuality() const {
    return _minSignalQuality;
}

// Event callbacks
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

// Utility methods
void Flexifi::loop() {
    // Handle WiFi events
    _handleWiFiEvents();
    
    // Check timeouts
    _checkTimeouts();
    
    // Retry storage initialization if it failed and enough time has passed
    if (_storage && (!_storage->isLittleFSAvailable() || !_storage->isNVSAvailable())) {
        unsigned long now = millis();
        if (now - _lastStorageRetry > STORAGE_RETRY_DELAY) {
            _lastStorageRetry = now;
            retryStorageInit();
        }
    }
    
    // Only check scan status when we're actually scanning
    if (_scanInProgress) {
        _updateNetworksJSON();
    }
    
    // Periodically log generated password if portal is active and using generated password
    if (_useGeneratedPassword && _portalState == PortalState::ACTIVE && !_generatedPassword.isEmpty()) {
        static unsigned long lastPasswordLog = 0;
        unsigned long now = millis();
        if (now - lastPasswordLog > FLEXIFI_PASSWORD_LOG_INTERVAL) {
            lastPasswordLog = now;
            FLEXIFI_LOGI("📶 Portal active - Password: %s", _generatedPassword.c_str());
        }
    }
}

void Flexifi::reset() {
    FLEXIFI_LOGI("Resetting Flexifi");
    
    // Stop portal
    stopPortal();
    
    // Clear configuration
    clearConfig();
    clearAllWiFiProfiles();
    
    // Reset state
    _wifiState = WiFiState::DISCONNECTED;
    _currentSSID = "";
    _currentPassword = "";
    _autoConnectRetryCount = 0;
    _lastAutoConnectAttempt = 0;
    _autoConnectLimitReachedLogged = false;
    
    FLEXIFI_LOGI("Flexifi reset completed");
}

String Flexifi::getStatusJSON() const {
    DynamicJsonDocument doc(512);
    doc["portal_state"] = static_cast<int>(_portalState);
    doc["wifi_state"] = static_cast<int>(_wifiState);
    doc["connected_ssid"] = getConnectedSSID();
    doc["profile_count"] = getWiFiProfileCount();
    doc["auto_connect"] = _autoConnectEnabled;
    doc["scan_remaining"] = getScanTimeRemaining();
    
    // Check if scan is currently running
    int scanStatus = WiFi.scanComplete();
    doc["scan_in_progress"] = (scanStatus == WIFI_SCAN_RUNNING);
    doc["scan_status"] = scanStatus;
    doc["network_count"] = _networkCount;
    
    String json;
    serializeJson(doc, json);
    return json;
}

String Flexifi::getPortalHTML() const {
    if (!_templateManager) {
        return "<html><body><h1>Template Manager Not Available</h1></body></html>";
    }
    
    String customParams = getParametersHTML();
    return _templateManager->getPortalHTML(customParams);
}

// Private methods
bool Flexifi::_setupAP() {
    FLEXIFI_LOGD("Setting up access point");
    
    // Stop any existing WiFi connections
    WiFi.disconnect();
    
    // Set WiFi mode to Access Point + Station
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    
    // Configure and start access point
    bool result;
    if (_apPassword.isEmpty()) {
        result = WiFi.softAP(_apName.c_str());
    } else {
        result = WiFi.softAP(_apName.c_str(), _apPassword.c_str());
    }
    
    if (!result) {
        FLEXIFI_LOGE("Failed to start access point");
        return false;
    }
    
    FLEXIFI_LOGI("Access point started - IP: %s", WiFi.softAPIP().toString().c_str());
    
    // Start DNS server for captive portal
    if (!_dnsServer) {
        _dnsServer = new DNSServer();
    }
    _dnsServer->start(53, "*", WiFi.softAPIP());
    FLEXIFI_LOGI("DNS server started for captive portal");
    
    // Start initial network scan
    delay(500); // Give AP time to fully initialize
    FLEXIFI_LOGI("Initiating first network scan after AP setup");
    scanNetworks(); // This will start async scan
    
    return true;
}

void Flexifi::_stopAP() {
    FLEXIFI_LOGD("Stopping access point");
    
    if (_dnsServer) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    
    FLEXIFI_LOGI("Access point stopped");
}

void Flexifi::_handleWiFiEvents() {
    // Handle connection
    if (_wifiState == WiFiState::CONNECTING) {
        unsigned long now = millis();
        
        // Check for timeout
        if (now - _connectStartTime > _connectTimeout) {
            FLEXIFI_LOGW("WiFi connection timeout");
            _onWiFiStateChange(WiFiState::FAILED);
            
            // Trigger callback
            if (_onConnectFailed) {
                _onConnectFailed(_currentSSID);
            }
            
            // Notify via WebSocket
            if (_portalServer) {
                _portalServer->broadcastMessage("connect_failed", "Connection timeout");
            }
            return;
        }
        
        // Check connection status
        wl_status_t status = WiFi.status();
        
        if (status == WL_CONNECTED) {
            FLEXIFI_LOGI("WiFi connected successfully");
            _onWiFiStateChange(WiFiState::CONNECTED);
            
            // Save configuration
            saveConfig();
            
            // Start mDNS if enabled
            _startMDNS();
            
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
        
        // Stop mDNS on disconnection
        _stopMDNS();
        
        // Trigger callback
        if (_onWiFiDisconnect) {
            _onWiFiDisconnect();
        }
    }
}

void Flexifi::_checkTimeouts() {
    unsigned long now = millis();
    
    // Check portal timeout
    if (_portalState == PortalState::ACTIVE && _portalTimeout > 0 && 
        now - _portalStartTime > _portalTimeout) {
        FLEXIFI_LOGI("Portal timeout reached");
        stopPortal();
    }
}

void Flexifi::_updateNetworksJSON() {
    int scanResult = WiFi.scanComplete();
    
    // Debug: Log scan status changes
    static int lastScanResult = -99;
    static unsigned long lastStatusLog = 0;
    unsigned long now = millis();
    
    if (scanResult != lastScanResult || now - lastStatusLog > 10000) {
        FLEXIFI_LOGD("🔍 Scan status check: result=%d, WiFi_mode=%d, time=%lu", 
                    scanResult, WiFi.getMode(), now);
        lastScanResult = scanResult;
        lastStatusLog = now;
    }
    
    // Handle scan states as recommended in ESP32 GitHub issue
    if (scanResult == WIFI_SCAN_FAILED) {
        // -2: No scan data available, but don't restart here (handled elsewhere)
        FLEXIFI_LOGD("🔍 No scan data available (WIFI_SCAN_FAILED)");
        return;
    }
    
    if (scanResult == WIFI_SCAN_RUNNING) {
        // -1: Scan still in progress
        return;
    }
    
    if (scanResult >= 0) {
        // Scan completed
        FLEXIFI_LOGD("WiFi scan completed, found %d networks", scanResult);
        
        // Debug: Log ALL networks found before filtering
        FLEXIFI_LOGI("=== ALL NETWORKS FOUND (min signal quality: %d dBm) ===", _minSignalQuality);
        for (int i = 0; i < scanResult; i++) {
            int rssi = WiFi.RSSI(i);
            String ssid = WiFi.SSID(i);
            bool isEncrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            FLEXIFI_LOGI("Network %d: %s (%d dBm) %s", i, ssid.c_str(), rssi, isEncrypted ? "🔒" : "🔓");
        }
        FLEXIFI_LOGI("=== END ALL NETWORKS ===");
        
        // Build JSON array with quality filtering - increased size for large network lists
        DynamicJsonDocument doc(8192);
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
            
            // Debug: Log networks that pass the filter
            FLEXIFI_LOGI("✅ Keeping strong network: %s (%d dBm)", ssid.c_str(), rssi);
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
        _scanInProgress = false;
        
        // Notify via WebSocket
        if (_portalServer) {
            FLEXIFI_LOGI("📡 Broadcasting networks via WebSocket: %s", _networksJSON.substring(0, 100).c_str());
            _portalServer->broadcastNetworks(_networksJSON);
        } else {
            FLEXIFI_LOGW("⚠️ Portal server not available for WebSocket broadcast");
        }
    } else if (scanResult == WIFI_SCAN_FAILED) {
        // Only log failure once every 30 seconds to avoid spam
        static unsigned long lastFailureLog = 0;
        unsigned long now = millis();
        
        if (now - lastFailureLog > 30000) {
            FLEXIFI_LOGW("WiFi scan failed (scanResult: %d, WiFi mode: %d)", scanResult, WiFi.getMode());
            lastFailureLog = now;
            
            // Only auto-retry if we have no networks cached and portal is active
            if (_networkCount == 0 && _portalState == PortalState::ACTIVE && now - _lastScanTime > 60000) {
                FLEXIFI_LOGI("Attempting to restart WiFi scanning due to persistent failures (no networks cached)");
                _lastScanTime = 0; // Reset throttle
                
                // Force a new scan attempt
                scanNetworks();
            } else if (_networkCount > 0) {
                FLEXIFI_LOGD("Scan failed but we have %d cached networks, not retrying", _networkCount);
            }
        }
    }
}

bool Flexifi::_validateCredentials(const String& ssid, const String& password) {
    if (ssid.isEmpty()) {
        FLEXIFI_LOGW("SSID cannot be empty");
        return false;
    }
    
    if (ssid.length() > 32) {
        FLEXIFI_LOGW("SSID too long (max 32 characters)");
        return false;
    }
    
    if (password.length() > 64) {
        FLEXIFI_LOGW("Password too long (max 64 characters)");
        return false;
    }
    
    return true;
}

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

bool Flexifi::_tryConnectToProfiles() {
    FLEXIFI_LOGI("🔍 _tryConnectToProfiles() called");
    
    if (!_storage) {
        FLEXIFI_LOGE("🚫 Storage not available for profile loading");
        return false;
    }
    
    std::vector<WiFiProfile> profiles = _storage->loadWiFiProfiles();
    FLEXIFI_LOGI("📋 Found %d profiles to try", (int)profiles.size());
    
    // Log all profiles
    for (const WiFiProfile& profile : profiles) {
        FLEXIFI_LOGI("  - %s (priority: %d, autoConnect: %s)", 
                    profile.ssid.c_str(), profile.priority, 
                    profile.autoConnect ? "YES" : "NO");
    }
    
    // Log network cache status
    FLEXIFI_LOGI("📡 Network cache status: JSON='%s', count=%d, lastScan=%lu, now=%lu", 
                 _networksJSON.substring(0, 50).c_str(), _networkCount, _lastScanTime, millis());
    
    // Direct connection attempts without scanning
    for (const WiFiProfile& profile : profiles) {
        if (!profile.autoConnect) continue;
        
        FLEXIFI_LOGI("🔌 Trying direct connection to: %s (priority: %d)", 
                    profile.ssid.c_str(), profile.priority);
        
        if (connectToWiFi(profile.ssid, profile.password)) {
            FLEXIFI_LOGI("✅ Successfully connected to: %s", profile.ssid.c_str());
            return true;
        }
    }
    
    FLEXIFI_LOGD("No available WiFi profiles found for auto-connect");
    return false;
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
        FLEXIFI_LOGE("Maximum parameter count reached: %d", _maxParameters);
        return false;
    }
    
    // Add to array
    _parameters[_parameterCount] = parameter;
    _parameterCount++;
    
    return true;
}

// Network filtering
bool Flexifi::_networkMeetsQuality(int rssi) const {
    return rssi >= _minSignalQuality;
}

String Flexifi::_getSignalStrengthIcon(int rssi) const {
    // Convert RSSI to 0-5 scale for CSS signal bars
    int strength = 0;
    if (rssi >= -30) strength = 5;
    else if (rssi >= -50) strength = 4;
    else if (rssi >= -60) strength = 3;
    else if (rssi >= -70) strength = 2;
    else if (rssi >= -80) strength = 1;
    else strength = 0;
    
    return String(strength);
}

// State change handlers
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

// Parameter persistence methods
void Flexifi::_saveParameterValues() {
    if (!_storage || !_parameters) {
        return;
    }
    
    FLEXIFI_LOGD("Saving %d parameter values", _parameterCount);
    
    for (int i = 0; i < _parameterCount; i++) {
        if (_parameters[i]) {
            // Use shorter key format to avoid NVS 15-character limit
            String key = "p_" + _parameters[i]->getID();
            String value = _parameters[i]->getValue();
            
            if (_storage->saveConfig(key, value)) {
                FLEXIFI_LOGD("Saved parameter: %s = %s", _parameters[i]->getID().c_str(), value.c_str());
            } else {
                FLEXIFI_LOGW("Failed to save parameter: %s", _parameters[i]->getID().c_str());
            }
        }
    }
}

void Flexifi::_loadParameterValues() {
    if (!_storage || !_parameters) {
        return;
    }
    
    // Check if storage is actually available before trying to load
    if (!_storage->isLittleFSAvailable() && !_storage->isNVSAvailable()) {
        FLEXIFI_LOGD("Storage not available, skipping parameter load");
        return;
    }
    
    FLEXIFI_LOGD("Loading parameter values for %d parameters", _parameterCount);
    
    for (int i = 0; i < _parameterCount; i++) {
        if (_parameters[i]) {
            _loadParameterValue(_parameters[i]);
        }
    }
}

void Flexifi::_loadParameterValue(FlexifiParameter* parameter) {
    if (!_storage || !parameter) {
        return;
    }
    
    // Check if storage is actually available before trying to load
    if (!_storage->isLittleFSAvailable() && !_storage->isNVSAvailable()) {
        FLEXIFI_LOGD("Storage not available, skipping parameter load for: %s", parameter->getID().c_str());
        return;
    }
    
    // Use shorter key format to avoid NVS 15-character limit
    String key = "p_" + parameter->getID();
    String savedValue = _storage->loadConfig(key);
    
    if (!savedValue.isEmpty()) {
        parameter->setValue(savedValue);
        FLEXIFI_LOGD("Loaded parameter: %s = %s", parameter->getID().c_str(), savedValue.c_str());
        
        // Special handling for mDNS hostname parameter
        if (parameter->getID() == "mdns_hostname" && savedValue != _mdnsHostname) {
            FLEXIFI_LOGI("Restoring mDNS hostname from storage: %s", savedValue.c_str());
            setMDNSHostname(savedValue);
        }
    } else {
        FLEXIFI_LOGD("No saved value found for parameter: %s", parameter->getID().c_str());
    }
}

// Password generation implementation
String Flexifi::_generatePassword(int length) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    String password = "";
    
    // Seed random number generator with MAC address and current time
    randomSeed(micros() + esp_random());
    
    for (int i = 0; i < length; i++) {
        password += charset[random(0, sizeof(charset) - 1)];
    }
    
    return password;
}