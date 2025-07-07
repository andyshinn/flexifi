#ifndef FLEXIFI_H
#define FLEXIFI_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <functional>
#include "StorageManager.h"

// Forward declarations
class PortalWebServer;
class StorageManager;
class TemplateManager;
class FlexifiParameter;
class DNSServer;
struct WiFiProfile;

// Configuration macros
#ifndef FLEXIFI_DEBUG_LEVEL
#define FLEXIFI_DEBUG_LEVEL 2
#endif

#ifndef FLEXIFI_LOG_TAG
#define FLEXIFI_LOG_TAG "Flexifi"
#endif

#ifndef FLEXIFI_SCAN_TIMEOUT
#define FLEXIFI_SCAN_TIMEOUT 10000
#endif

#ifndef FLEXIFI_CONNECT_TIMEOUT
#define FLEXIFI_CONNECT_TIMEOUT 15000
#endif

#ifndef FLEXIFI_PORTAL_TIMEOUT
#define FLEXIFI_PORTAL_TIMEOUT 300000
#endif

// Logging macros
#if FLEXIFI_DEBUG_LEVEL >= 1
#include <esp_log.h>
#define FLEXIFI_LOGE(format, ...) ESP_LOGE(FLEXIFI_LOG_TAG, format, ##__VA_ARGS__)
#else
#define FLEXIFI_LOGE(format, ...)
#endif

#if FLEXIFI_DEBUG_LEVEL >= 2
#define FLEXIFI_LOGW(format, ...) ESP_LOGW(FLEXIFI_LOG_TAG, format, ##__VA_ARGS__)
#else
#define FLEXIFI_LOGW(format, ...)
#endif

#if FLEXIFI_DEBUG_LEVEL >= 3
#define FLEXIFI_LOGI(format, ...) ESP_LOGI(FLEXIFI_LOG_TAG, format, ##__VA_ARGS__)
#else
#define FLEXIFI_LOGI(format, ...)
#endif

#if FLEXIFI_DEBUG_LEVEL >= 4
#define FLEXIFI_LOGD(format, ...) ESP_LOGD(FLEXIFI_LOG_TAG, format, ##__VA_ARGS__)
#else
#define FLEXIFI_LOGD(format, ...)
#endif

enum class PortalState {
    STOPPED,
    STARTING,
    ACTIVE,
    STOPPING
};

enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

class Flexifi {
public:
    Flexifi(AsyncWebServer* server);
    ~Flexifi();
    
    // Initialization
    bool init();

    // Configuration methods
    void setTemplate(const String& templateName);
    void setCustomTemplate(const String& htmlTemplate);
    void setCredentials(const String& ssid, const String& password);
    void setPortalTimeout(unsigned long timeout);
    void setConnectTimeout(unsigned long timeout);

    // Portal management
    bool startPortal(const String& apName, const String& apPassword = "");
    void stopPortal();
    bool isPortalActive() const;
    PortalState getPortalState() const;

    // Storage management
    bool saveConfig();
    bool loadConfig();
    void clearConfig();
    
    // WiFi profile management
    bool addWiFiProfile(const String& ssid, const String& password, int priority = 50);
    bool updateWiFiProfile(const String& ssid, const String& password, int priority = 50);
    bool deleteWiFiProfile(const String& ssid);
    bool hasWiFiProfile(const String& ssid);
    void clearAllWiFiProfiles();
    int getWiFiProfileCount();
    String getWiFiProfilesJSON() const;
    
    // Auto-connect functionality
    bool autoConnect();
    void setAutoConnectEnabled(bool enabled);
    bool isAutoConnectEnabled() const;
    String getHighestPrioritySSID() const;
    bool updateProfileLastUsed(const String& ssid);

    // Custom parameters
    void addParameter(FlexifiParameter* parameter);
    void addParameter(const String& id, const String& label, const String& defaultValue = "", 
                     int maxLength = 40);
    FlexifiParameter* getParameter(const String& id);
    String getParameterValue(const String& id) const;
    void setParameterValue(const String& id, const String& value);
    int getParameterCount() const;
    String getParametersHTML() const;

    // Network management
    bool scanNetworks(); // Returns true if scan started, false if throttled
    String getNetworksJSON() const;
    unsigned long getScanTimeRemaining() const; // Returns ms until next scan allowed
    bool connectToWiFi(const String& ssid, const String& password);
    WiFiState getWiFiState() const;
    String getConnectedSSID() const;
    void setMinSignalQuality(int quality);
    int getMinSignalQuality() const;

    // Event callbacks
    void onPortalStart(std::function<void()> callback);
    void onPortalStop(std::function<void()> callback);
    void onWiFiConnect(std::function<void(const String&)> callback);
    void onWiFiDisconnect(std::function<void()> callback);
    void onConfigSave(std::function<void(const String&, const String&)> callback);
    void onScanComplete(std::function<void(int)> callback);
    void onConnectStart(std::function<void(const String&)> callback);
    void onConnectFailed(std::function<void(const String&)> callback);

    // Utility methods
    void loop();
    void reset();
    String getStatusJSON() const;
    String getPortalHTML() const;

private:
    AsyncWebServer* _server;
    PortalWebServer* _portalServer;
    StorageManager* _storage;
    TemplateManager* _templateManager;
    DNSServer* _dnsServer;

    // State management
    PortalState _portalState;
    WiFiState _wifiState;
    String _currentSSID;
    String _currentPassword;
    String _apName;
    String _apPassword;
    
    // Profile management
    bool _autoConnectEnabled;
    unsigned long _lastAutoConnectAttempt;
    int _autoConnectRetryCount;
    bool _autoConnectLimitReachedLogged;
    static const int MAX_AUTO_CONNECT_RETRIES = 3;
    static const unsigned long AUTO_CONNECT_RETRY_DELAY = 30000;

    // Timing
    unsigned long _portalTimeout;
    unsigned long _connectTimeout;
    unsigned long _portalStartTime;
    unsigned long _connectStartTime;
    unsigned long _lastScanTime;

    // Network data
    int _networkCount;
    String _networksJSON;
    int _minSignalQuality;

    // Custom parameters
    FlexifiParameter** _parameters;
    int _parameterCount;
    int _maxParameters;

    // Callback functions
    std::function<void()> _onPortalStart;
    std::function<void()> _onPortalStop;
    std::function<void(const String&)> _onWiFiConnect;
    std::function<void()> _onWiFiDisconnect;
    std::function<void(const String&, const String&)> _onConfigSave;
    std::function<void(int)> _onScanComplete;
    std::function<void(const String&)> _onConnectStart;
    std::function<void(const String&)> _onConnectFailed;
    
    // Internal scan completion callback
    std::function<void(int)> _onInternalScanComplete;
    
    // Static instance for WiFi event callbacks
    static Flexifi* _instance;

    // Private methods
    bool _setupAP();
    void _stopAP();
    void _handleWiFiEvents();
    void _checkTimeouts();
    void _updateNetworksJSON();
    bool _validateCredentials(const String& ssid, const String& password);
    void _setupWiFiEvents();
    static void _onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    
    // Parameter management
    void _initParameters();
    void _clearParameters();
    int _findParameterIndex(const String& id) const;
    bool _addParameterToArray(FlexifiParameter* parameter);
    
    // Network filtering
    bool _networkMeetsQuality(int rssi) const;
    String _getSignalStrengthIcon(int rssi) const;
    
    // State change handlers
    void _onPortalStateChange(PortalState newState);
    void _onWiFiStateChange(WiFiState newState);
    
    // Profile management helpers
    bool _tryConnectToProfiles();
    bool _tryConnectWithProfiles(const std::vector<WiFiProfile>& profiles);
    bool _connectToHighestPriorityNetwork();
    void _updateProfilePriorities();
    String _formatProfilesJSON(const std::vector<WiFiProfile>& profiles) const;
};

#endif // FLEXIFI_H