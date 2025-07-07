#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <Arduino.h>
#include <vector>

// Conditional includes based on compile flags
#ifndef FLEXIFI_DISABLE_LITTLEFS
#include <LittleFS.h>
#endif

#ifndef FLEXIFI_DISABLE_NVS
#include <Preferences.h>
#endif

// WiFi profile structure
struct WiFiProfile {
    String ssid;
    String password;
    int priority;           // Higher number = higher priority
    unsigned long lastUsed; // Timestamp of last successful connection
    bool autoConnect;       // Whether to auto-connect to this profile
    
    WiFiProfile(const String& s = "", const String& p = "", int pr = 0) : 
        ssid(s), password(p), priority(pr), lastUsed(0), autoConnect(true) {}
        
    bool isValid() const { return !ssid.isEmpty(); }
};

class StorageManager {
public:
    StorageManager();
    ~StorageManager();

    // Initialization
    bool init();
    void deinit();

    // Credential management (legacy single profile)
    bool saveCredentials(const String& ssid, const String& password);
    bool loadCredentials(String& ssid, String& password);
    bool clearCredentials();
    
    // WiFi profile management
    bool saveWiFiProfile(const WiFiProfile& profile);
    bool updateWiFiProfile(const String& ssid, const WiFiProfile& profile);
    bool deleteWiFiProfile(const String& ssid);
    std::vector<WiFiProfile> loadWiFiProfiles();
    WiFiProfile getWiFiProfile(const String& ssid);
    bool hasWiFiProfile(const String& ssid);
    void clearAllWiFiProfiles();
    
    // Profile management utilities
    std::vector<WiFiProfile> getProfilesByPriority();
    WiFiProfile getHighestPriorityProfile();
    bool updateProfileLastUsed(const String& ssid);
    int getProfileCount();

    // Configuration management
    bool saveConfig(const String& key, const String& value);
    String loadConfig(const String& key, const String& defaultValue = "");
    bool clearConfig(const String& key);
    bool clearAllConfig();

    // Storage status
    bool isLittleFSAvailable() const;
    bool isNVSAvailable() const;
    String getStorageInfo() const;

private:
    bool _littleFSAvailable;
    bool _nvsAvailable;
    bool _preferLittleFS;

#ifndef FLEXIFI_DISABLE_NVS
    Preferences _preferences;
#endif

    // Profile cache to prevent repeated loading during status checks
    mutable std::vector<WiFiProfile> _cachedProfiles;
    mutable unsigned long _cacheTime;
    static const unsigned long CACHE_DURATION = 5000; // 5 seconds

    // Configuration constants
    static const char* CREDENTIALS_FILE;
    static const char* PROFILES_FILE;
    static const char* CONFIG_NAMESPACE;
    static const char* SSID_KEY;
    static const char* PASSWORD_KEY;
    static const char* PROFILES_KEY;
    static const int MAX_PROFILES = 10;

    // Initialization methods
    bool _initLittleFS();
    bool _initNVS();
    void _determineStoragePreference();

    // LittleFS methods
#ifndef FLEXIFI_DISABLE_LITTLEFS
    bool _saveLittleFS(const String& filename, const String& data);
    String _loadLittleFS(const String& filename);
    bool _deleteLittleFS(const String& filename);
    bool _existsLittleFS(const String& filename);
#endif

    // NVS methods
#ifndef FLEXIFI_DISABLE_NVS
    bool _saveNVS(const String& key, const String& value);
    String _loadNVS(const String& key, const String& defaultValue = "");
    bool _deleteNVS(const String& key);
    bool _existsNVS(const String& key);
#endif

    // Utility methods
    String _encodeCredentials(const String& ssid, const String& password) const;
    bool _decodeCredentials(const String& encoded, String& ssid, String& password) const;
    String _sanitizeKey(const String& key) const;
    
    // Profile management utilities
    String _encodeProfiles(const std::vector<WiFiProfile>& profiles) const;
    std::vector<WiFiProfile> _decodeProfiles(const String& encoded) const;
    int _findProfileIndex(const std::vector<WiFiProfile>& profiles, const String& ssid) const;
    void _sortProfilesByPriority(std::vector<WiFiProfile>& profiles) const;
};

#endif // STORAGEMANAGER_H