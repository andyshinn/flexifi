#include "StorageManager.h"
#include "Flexifi.h"
#include <ArduinoJson.h>
#include <algorithm>

// Static constants
const char* StorageManager::CREDENTIALS_FILE = "/wifi_credentials.json";
const char* StorageManager::CONFIG_NAMESPACE = "flexifi";
const char* StorageManager::SSID_KEY = "ssid";
const char* StorageManager::PASSWORD_KEY = "password";
const char* StorageManager::PROFILES_FILE = "/wifi_profiles.json";
const char* StorageManager::PROFILES_KEY = "profiles";

StorageManager::StorageManager() :
    _littleFSAvailable(false),
    _nvsAvailable(false),
    _preferLittleFS(true),
    _cacheTime(0) {
}

StorageManager::~StorageManager() {
    deinit();
}

bool StorageManager::init() {
    FLEXIFI_LOGI("Initializing storage manager");
    
    _determineStoragePreference();
    
    bool littleFSInit = _initLittleFS();
    bool nvsInit = _initNVS();
    
    if (!littleFSInit && !nvsInit) {
        FLEXIFI_LOGE("Failed to initialize any storage system");
        return false;
    }
    
    if (littleFSInit) {
        FLEXIFI_LOGI("LittleFS storage initialized successfully");
    }
    
    if (nvsInit) {
        FLEXIFI_LOGI("NVS storage initialized successfully");
    }
    
    return true;
}

void StorageManager::deinit() {
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        _preferences.end();
        _nvsAvailable = false;
    }
#endif
    
    _littleFSAvailable = false;
    FLEXIFI_LOGD("Storage manager deinitialized");
}

bool StorageManager::saveCredentials(const String& ssid, const String& password) {
    FLEXIFI_LOGD("Saving credentials for SSID: %s", ssid.c_str());
    
    if (ssid.isEmpty()) {
        FLEXIFI_LOGW("Cannot save empty SSID");
        return false;
    }
    
    String encodedData = _encodeCredentials(ssid, password);
    
    // Try preferred storage first
    if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        if (_saveLittleFS(CREDENTIALS_FILE, encodedData)) {
            FLEXIFI_LOGD("Credentials saved to LittleFS");
            return true;
        }
#endif
    }
    
    // Fallback to NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        if (_saveNVS(SSID_KEY, ssid) && _saveNVS(PASSWORD_KEY, password)) {
            FLEXIFI_LOGD("Credentials saved to NVS");
            return true;
        }
    }
#endif
    
    // If LittleFS is not preferred, try it as fallback
    if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        if (_saveLittleFS(CREDENTIALS_FILE, encodedData)) {
            FLEXIFI_LOGD("Credentials saved to LittleFS (fallback)");
            return true;
        }
#endif
    }
    
    FLEXIFI_LOGE("Failed to save credentials to any storage");
    return false;
}

bool StorageManager::loadCredentials(String& ssid, String& password) {
    FLEXIFI_LOGD("Loading credentials from storage");
    
    ssid = "";
    password = "";
    
    // Try preferred storage first
    if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String encodedData = _loadLittleFS(CREDENTIALS_FILE);
        if (!encodedData.isEmpty() && _decodeCredentials(encodedData, ssid, password)) {
            FLEXIFI_LOGD("Credentials loaded from LittleFS");
            return true;
        }
#endif
    }
    
    // Fallback to NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        String nvsSSID = _loadNVS(SSID_KEY);
        String nvsPassword = _loadNVS(PASSWORD_KEY);
        if (!nvsSSID.isEmpty()) {
            ssid = nvsSSID;
            password = nvsPassword;
            FLEXIFI_LOGD("Credentials loaded from NVS");
            return true;
        }
    }
#endif
    
    // If LittleFS is not preferred, try it as fallback
    if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String encodedData = _loadLittleFS(CREDENTIALS_FILE);
        if (!encodedData.isEmpty() && _decodeCredentials(encodedData, ssid, password)) {
            FLEXIFI_LOGD("Credentials loaded from LittleFS (fallback)");
            return true;
        }
#endif
    }
    
    FLEXIFI_LOGD("No credentials found in storage");
    return false;
}

bool StorageManager::clearCredentials() {
    FLEXIFI_LOGD("Clearing stored credentials");
    
    bool cleared = false;
    
    // Clear from LittleFS
#ifndef FLEXIFI_DISABLE_LITTLEFS
    if (_littleFSAvailable) {
        if (_deleteLittleFS(CREDENTIALS_FILE)) {
            FLEXIFI_LOGD("Credentials cleared from LittleFS");
            cleared = true;
        }
    }
#endif
    
    // Clear from NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        bool nvsCleared = _deleteNVS(SSID_KEY) && _deleteNVS(PASSWORD_KEY);
        if (nvsCleared) {
            FLEXIFI_LOGD("Credentials cleared from NVS");
            cleared = true;
        }
    }
#endif
    
    return cleared;
}

bool StorageManager::saveConfig(const String& key, const String& value) {
    if (key.isEmpty()) {
        FLEXIFI_LOGW("Cannot save config with empty key");
        return false;
    }
    
    String sanitizedKey = _sanitizeKey(key);
    FLEXIFI_LOGD("Saving config: %s", sanitizedKey.c_str());
    
    // Try preferred storage first
    if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String filename = "/" + sanitizedKey + ".txt";
        if (_saveLittleFS(filename, value)) {
            return true;
        }
#endif
    }
    
    // Fallback to NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        if (_saveNVS(sanitizedKey, value)) {
            return true;
        }
    }
#endif
    
    // If LittleFS is not preferred, try it as fallback
    if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String filename = "/" + sanitizedKey + ".txt";
        if (_saveLittleFS(filename, value)) {
            return true;
        }
#endif
    }
    
    FLEXIFI_LOGE("Failed to save config key: %s", sanitizedKey.c_str());
    return false;
}

String StorageManager::loadConfig(const String& key, const String& defaultValue) {
    if (key.isEmpty()) {
        return defaultValue;
    }
    
    String sanitizedKey = _sanitizeKey(key);
    FLEXIFI_LOGD("Loading config: %s", sanitizedKey.c_str());
    
    // Try preferred storage first
    if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String filename = "/" + sanitizedKey + ".txt";
        String value = _loadLittleFS(filename);
        if (!value.isEmpty()) {
            return value;
        }
#endif
    }
    
    // Fallback to NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        String value = _loadNVS(sanitizedKey, defaultValue);
        if (value != defaultValue) {
            return value;
        }
    }
#endif
    
    // If LittleFS is not preferred, try it as fallback
    if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String filename = "/" + sanitizedKey + ".txt";
        String value = _loadLittleFS(filename);
        if (!value.isEmpty()) {
            return value;
        }
#endif
    }
    
    return defaultValue;
}

bool StorageManager::clearConfig(const String& key) {
    if (key.isEmpty()) {
        return false;
    }
    
    String sanitizedKey = _sanitizeKey(key);
    FLEXIFI_LOGD("Clearing config: %s", sanitizedKey.c_str());
    
    bool cleared = false;
    
    // Clear from LittleFS
#ifndef FLEXIFI_DISABLE_LITTLEFS
    if (_littleFSAvailable) {
        String filename = "/" + sanitizedKey + ".txt";
        if (_deleteLittleFS(filename)) {
            cleared = true;
        }
    }
#endif
    
    // Clear from NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        if (_deleteNVS(sanitizedKey)) {
            cleared = true;
        }
    }
#endif
    
    return cleared;
}

bool StorageManager::clearAllConfig() {
    FLEXIFI_LOGI("Clearing all stored configuration");
    
    bool cleared = false;
    
    // Clear LittleFS
#ifndef FLEXIFI_DISABLE_LITTLEFS
    if (_littleFSAvailable) {
        LittleFS.format();
        cleared = true;
        FLEXIFI_LOGD("LittleFS formatted");
    }
#endif
    
    // Clear NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        _preferences.clear();
        cleared = true;
        FLEXIFI_LOGD("NVS namespace cleared");
    }
#endif
    
    return cleared;
}

// WiFi Profile Management

bool StorageManager::saveWiFiProfile(const WiFiProfile& profile) {
    if (!profile.isValid()) {
        FLEXIFI_LOGW("Cannot save invalid WiFi profile");
        return false;
    }
    
    FLEXIFI_LOGD("Saving WiFi profile: %s (priority: %d)", profile.ssid.c_str(), profile.priority);
    
    // Load existing profiles
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    
    // Find existing profile or add new one
    int existingIndex = _findProfileIndex(profiles, profile.ssid);
    if (existingIndex >= 0) {
        // Update existing profile
        profiles[existingIndex] = profile;
        FLEXIFI_LOGD("Updated existing profile: %s", profile.ssid.c_str());
    } else {
        // Add new profile
        if (profiles.size() >= MAX_PROFILES) {
            FLEXIFI_LOGW("Maximum profiles reached (%d), removing oldest", MAX_PROFILES);
            // Remove profile with lowest priority and oldest last used time
            auto minIt = std::min_element(profiles.begin(), profiles.end(), 
                [](const WiFiProfile& a, const WiFiProfile& b) {
                    if (a.priority != b.priority) return a.priority < b.priority;
                    return a.lastUsed < b.lastUsed;
                });
            profiles.erase(minIt);
        }
        profiles.push_back(profile);
        FLEXIFI_LOGD("Added new profile: %s", profile.ssid.c_str());
    }
    
    // Save updated profiles
    String encodedProfiles = _encodeProfiles(profiles);
    
    // Try preferred storage first
    if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
            FLEXIFI_LOGD("WiFi profiles saved to LittleFS");
            _cachedProfiles.clear(); // Invalidate cache
            return true;
        }
#endif
    }
    
    // Fallback to NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        if (_saveNVS(PROFILES_KEY, encodedProfiles)) {
            FLEXIFI_LOGD("WiFi profiles saved to NVS");
            _cachedProfiles.clear(); // Invalidate cache
            return true;
        }
    }
#endif
    
    // If LittleFS is not preferred, try it as fallback
    if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
            FLEXIFI_LOGD("WiFi profiles saved to LittleFS (fallback)");
            _cachedProfiles.clear(); // Invalidate cache
            return true;
        }
#endif
    }
    
    FLEXIFI_LOGE("Failed to save WiFi profile: %s", profile.ssid.c_str());
    return false;
}

bool StorageManager::updateWiFiProfile(const String& ssid, const WiFiProfile& profile) {
    if (ssid.isEmpty() || !profile.isValid()) {
        return false;
    }
    
    // Load existing profiles
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    
    // Find and update profile
    int index = _findProfileIndex(profiles, ssid);
    if (index >= 0) {
        profiles[index] = profile;
        String encodedProfiles = _encodeProfiles(profiles);
        
        // Save using same logic as saveWiFiProfile
        if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
            if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
                return true;
            }
#endif
        }
        
#ifndef FLEXIFI_DISABLE_NVS
        if (_nvsAvailable) {
            if (_saveNVS(PROFILES_KEY, encodedProfiles)) {
                return true;
            }
        }
#endif
        
        if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
            if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
                return true;
            }
#endif
        }
    }
    
    return false;
}

bool StorageManager::deleteWiFiProfile(const String& ssid) {
    if (ssid.isEmpty()) {
        return false;
    }
    
    FLEXIFI_LOGD("Deleting WiFi profile: %s", ssid.c_str());
    
    // Load existing profiles
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    
    // Find and remove profile
    int index = _findProfileIndex(profiles, ssid);
    if (index >= 0) {
        profiles.erase(profiles.begin() + index);
        String encodedProfiles = _encodeProfiles(profiles);
        
        // Save updated profiles
        if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
            if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
                FLEXIFI_LOGD("WiFi profile deleted from LittleFS");
                _cachedProfiles.clear(); // Invalidate cache
                return true;
            }
#endif
        }
        
#ifndef FLEXIFI_DISABLE_NVS
        if (_nvsAvailable) {
            if (_saveNVS(PROFILES_KEY, encodedProfiles)) {
                FLEXIFI_LOGD("WiFi profile deleted from NVS");
                _cachedProfiles.clear(); // Invalidate cache
                return true;
            }
        }
#endif
        
        if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
            if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
                FLEXIFI_LOGD("WiFi profile deleted from LittleFS (fallback)");
                _cachedProfiles.clear(); // Invalidate cache
                return true;
            }
#endif
        }
    }
    
    return false;
}

std::vector<WiFiProfile> StorageManager::loadWiFiProfiles() {
    // Check if we can use cached profiles
    unsigned long now = millis();
    if (!_cachedProfiles.empty() && (now - _cacheTime) < CACHE_DURATION) {
        return _cachedProfiles;
    }
    
    FLEXIFI_LOGD("Loading WiFi profiles from storage");
    
    std::vector<WiFiProfile> profiles;
    
    // Try preferred storage first
    if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String encodedProfiles = _loadLittleFS(PROFILES_FILE);
        if (!encodedProfiles.isEmpty()) {
            profiles = _decodeProfiles(encodedProfiles);
            if (!profiles.empty()) {
                FLEXIFI_LOGD("Loaded %d WiFi profiles from LittleFS", profiles.size());
                _sortProfilesByPriority(profiles);
                _cachedProfiles = profiles;
                _cacheTime = millis();
                return profiles;
            }
        }
#endif
    }
    
    // Fallback to NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        String encodedProfiles = _loadNVS(PROFILES_KEY);
        if (!encodedProfiles.isEmpty()) {
            profiles = _decodeProfiles(encodedProfiles);
            if (!profiles.empty()) {
                FLEXIFI_LOGD("Loaded %d WiFi profiles from NVS", profiles.size());
                _sortProfilesByPriority(profiles);
                _cachedProfiles = profiles;
                _cacheTime = millis();
                return profiles;
            }
        }
    }
#endif
    
    // If LittleFS is not preferred, try it as fallback
    if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
        String encodedProfiles = _loadLittleFS(PROFILES_FILE);
        if (!encodedProfiles.isEmpty()) {
            profiles = _decodeProfiles(encodedProfiles);
            if (!profiles.empty()) {
                FLEXIFI_LOGD("Loaded %d WiFi profiles from LittleFS (fallback)", profiles.size());
                _sortProfilesByPriority(profiles);
                _cachedProfiles = profiles;
                _cacheTime = millis();
                return profiles;
            }
        }
#endif
    }
    
    // Check for legacy single credential and migrate
    String legacySSID, legacyPassword;
    if (loadCredentials(legacySSID, legacyPassword)) {
        FLEXIFI_LOGI("Migrating legacy credentials to profile system");
        WiFiProfile legacyProfile(legacySSID, legacyPassword, 100); // High priority
        legacyProfile.lastUsed = millis();
        profiles.push_back(legacyProfile);
        
        // Save migrated profile
        if (saveWiFiProfile(legacyProfile)) {
            FLEXIFI_LOGD("Legacy credentials migrated successfully");
            clearCredentials(); // Remove legacy credentials
        }
    }
    
    FLEXIFI_LOGD("Loaded %d WiFi profiles total", profiles.size());
    _cachedProfiles = profiles;
    _cacheTime = millis();
    return profiles;
}

WiFiProfile StorageManager::getWiFiProfile(const String& ssid) {
    if (ssid.isEmpty()) {
        return WiFiProfile();
    }
    
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    int index = _findProfileIndex(profiles, ssid);
    
    if (index >= 0) {
        return profiles[index];
    }
    
    return WiFiProfile();
}

bool StorageManager::hasWiFiProfile(const String& ssid) {
    if (ssid.isEmpty()) {
        return false;
    }
    
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    return _findProfileIndex(profiles, ssid) >= 0;
}

void StorageManager::clearAllWiFiProfiles() {
    FLEXIFI_LOGI("Clearing all WiFi profiles");
    
    // Clear from LittleFS
#ifndef FLEXIFI_DISABLE_LITTLEFS
    if (_littleFSAvailable) {
        _deleteLittleFS(PROFILES_FILE);
        FLEXIFI_LOGD("WiFi profiles cleared from LittleFS");
    }
#endif
    
    // Clear from NVS
#ifndef FLEXIFI_DISABLE_NVS
    if (_nvsAvailable) {
        _deleteNVS(PROFILES_KEY);
        FLEXIFI_LOGD("WiFi profiles cleared from NVS");
    }
#endif
}

std::vector<WiFiProfile> StorageManager::getProfilesByPriority() {
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    _sortProfilesByPriority(profiles);
    return profiles;
}

WiFiProfile StorageManager::getHighestPriorityProfile() {
    std::vector<WiFiProfile> profiles = getProfilesByPriority();
    if (!profiles.empty()) {
        return profiles[0]; // First profile has highest priority
    }
    return WiFiProfile();
}

bool StorageManager::updateProfileLastUsed(const String& ssid) {
    if (ssid.isEmpty()) {
        return false;
    }
    
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    int index = _findProfileIndex(profiles, ssid);
    
    if (index >= 0) {
        profiles[index].lastUsed = millis();
        String encodedProfiles = _encodeProfiles(profiles);
        
        // Save using same logic as saveWiFiProfile
        if (_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
            if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
                return true;
            }
#endif
        }
        
#ifndef FLEXIFI_DISABLE_NVS
        if (_nvsAvailable) {
            if (_saveNVS(PROFILES_KEY, encodedProfiles)) {
                return true;
            }
        }
#endif
        
        if (!_preferLittleFS && _littleFSAvailable) {
#ifndef FLEXIFI_DISABLE_LITTLEFS
            if (_saveLittleFS(PROFILES_FILE, encodedProfiles)) {
                return true;
            }
#endif
        }
    }
    
    return false;
}

int StorageManager::getProfileCount() {
    std::vector<WiFiProfile> profiles = loadWiFiProfiles();
    return profiles.size();
}

bool StorageManager::isLittleFSAvailable() const {
    return _littleFSAvailable;
}

bool StorageManager::isNVSAvailable() const {
    return _nvsAvailable;
}

String StorageManager::getStorageInfo() const {
    String info = "Storage: ";
    
    if (_littleFSAvailable) {
        info += "LittleFS ";
#ifndef FLEXIFI_DISABLE_LITTLEFS
        info += "(" + String(LittleFS.totalBytes()) + " bytes total, " + 
                String(LittleFS.usedBytes()) + " used)";
#endif
    }
    
    if (_nvsAvailable) {
        if (_littleFSAvailable) info += ", ";
        info += "NVS";
    }
    
    if (!_littleFSAvailable && !_nvsAvailable) {
        info += "None available";
    }
    
    return info;
}

// Private methods

void StorageManager::_determineStoragePreference() {
#ifdef FLEXIFI_FORCE_LITTLEFS
    _preferLittleFS = true;
#elif defined(FLEXIFI_FORCE_NVS)
    _preferLittleFS = false;
#else
    // Default preference: LittleFS
    _preferLittleFS = true;
#endif
}

bool StorageManager::_initLittleFS() {
#ifdef FLEXIFI_DISABLE_LITTLEFS
    return false;
#else
    if (!LittleFS.begin()) {
        FLEXIFI_LOGD("LittleFS mount failed, attempting format");
        if (!LittleFS.format()) {
            FLEXIFI_LOGE("LittleFS format failed");
            return false;
        }
        if (!LittleFS.begin()) {
            FLEXIFI_LOGE("LittleFS mount failed after format");
            return false;
        }
    }
    
    _littleFSAvailable = true;
    return true;
#endif
}

bool StorageManager::_initNVS() {
#ifdef FLEXIFI_DISABLE_NVS
    return false;
#else
    if (!_preferences.begin(CONFIG_NAMESPACE, false)) {
        FLEXIFI_LOGE("Failed to initialize NVS preferences");
        return false;
    }
    
    _nvsAvailable = true;
    return true;
#endif
}

#ifndef FLEXIFI_DISABLE_LITTLEFS
bool StorageManager::_saveLittleFS(const String& filename, const String& data) {
    File file = LittleFS.open(filename, FILE_WRITE);
    if (!file) {
        FLEXIFI_LOGE("Failed to open file for writing: %s", filename.c_str());
        return false;
    }
    
    size_t bytesWritten = file.print(data);
    file.close();
    
    return bytesWritten == data.length();
}

String StorageManager::_loadLittleFS(const String& filename) {
    if (!LittleFS.exists(filename)) {
        return "";
    }
    
    File file = LittleFS.open(filename, FILE_READ);
    if (!file) {
        FLEXIFI_LOGE("Failed to open file for reading: %s", filename.c_str());
        return "";
    }
    
    String data = file.readString();
    file.close();
    
    return data;
}

bool StorageManager::_deleteLittleFS(const String& filename) {
    if (!LittleFS.exists(filename)) {
        return true; // Already deleted
    }
    
    return LittleFS.remove(filename);
}

bool StorageManager::_existsLittleFS(const String& filename) {
    return LittleFS.exists(filename);
}
#endif

#ifndef FLEXIFI_DISABLE_NVS
bool StorageManager::_saveNVS(const String& key, const String& value) {
    return _preferences.putString(key.c_str(), value) == value.length();
}

String StorageManager::_loadNVS(const String& key, const String& defaultValue) {
    return _preferences.getString(key.c_str(), defaultValue);
}

bool StorageManager::_deleteNVS(const String& key) {
    return _preferences.remove(key.c_str());
}

bool StorageManager::_existsNVS(const String& key) {
    return _preferences.isKey(key.c_str());
}
#endif

String StorageManager::_encodeCredentials(const String& ssid, const String& password) const {
    DynamicJsonDocument doc(512);
    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["timestamp"] = millis();
    
    String encoded;
    serializeJson(doc, encoded);
    return encoded;
}

bool StorageManager::_decodeCredentials(const String& encoded, String& ssid, String& password) const {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, encoded);
    
    if (error) {
        FLEXIFI_LOGE("Failed to decode credentials: %s", error.c_str());
        return false;
    }
    
    if (!doc.containsKey("ssid")) {
        FLEXIFI_LOGE("Credentials missing SSID field");
        return false;
    }
    
    ssid = doc["ssid"].as<String>();
    password = doc["password"].as<String>();
    
    return true;
}

String StorageManager::_sanitizeKey(const String& key) const {
    String sanitized = key;
    
    // Replace invalid characters with underscores
    for (int i = 0; i < sanitized.length(); i++) {
        char c = sanitized[i];
        if (!isalnum(c) && c != '_' && c != '-') {
            sanitized[i] = '_';
        }
    }
    
    // Ensure key is not too long (NVS has 15 character limit)
    if (sanitized.length() > 15) {
        sanitized = sanitized.substring(0, 15);
    }
    
    return sanitized;
}

// Profile management utility methods

String StorageManager::_encodeProfiles(const std::vector<WiFiProfile>& profiles) const {
    DynamicJsonDocument doc(2048); // Increased size for multiple profiles
    JsonArray profilesArray = doc.createNestedArray("profiles");
    
    for (const auto& profile : profiles) {
        JsonObject profileObj = profilesArray.createNestedObject();
        profileObj["ssid"] = profile.ssid;
        profileObj["password"] = profile.password;
        profileObj["priority"] = profile.priority;
        profileObj["lastUsed"] = profile.lastUsed;
        profileObj["autoConnect"] = profile.autoConnect;
    }
    
    doc["timestamp"] = millis();
    doc["version"] = 1;
    
    String encoded;
    serializeJson(doc, encoded);
    return encoded;
}

std::vector<WiFiProfile> StorageManager::_decodeProfiles(const String& encoded) const {
    std::vector<WiFiProfile> profiles;
    
    if (encoded.isEmpty()) {
        return profiles;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, encoded);
    
    if (error) {
        FLEXIFI_LOGE("Failed to decode WiFi profiles: %s", error.c_str());
        return profiles;
    }
    
    if (!doc.containsKey("profiles")) {
        FLEXIFI_LOGE("WiFi profiles missing profiles array");
        return profiles;
    }
    
    JsonArray profilesArray = doc["profiles"];
    for (JsonObject profileObj : profilesArray) {
        if (!profileObj.containsKey("ssid")) {
            continue; // Skip invalid profiles
        }
        
        WiFiProfile profile;
        profile.ssid = profileObj["ssid"].as<String>();
        profile.password = profileObj["password"].as<String>();
        profile.priority = profileObj["priority"].as<int>();
        profile.lastUsed = profileObj["lastUsed"].as<unsigned long>();
        profile.autoConnect = profileObj["autoConnect"].as<bool>();
        
        if (profile.isValid()) {
            profiles.push_back(profile);
        }
    }
    
    return profiles;
}

int StorageManager::_findProfileIndex(const std::vector<WiFiProfile>& profiles, const String& ssid) const {
    for (int i = 0; i < profiles.size(); i++) {
        if (profiles[i].ssid.equals(ssid)) {
            return i;
        }
    }
    return -1;
}

void StorageManager::_sortProfilesByPriority(std::vector<WiFiProfile>& profiles) const {
    std::sort(profiles.begin(), profiles.end(), [](const WiFiProfile& a, const WiFiProfile& b) {
        // Sort by priority (descending), then by last used (descending)
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        return a.lastUsed > b.lastUsed;
    });
}