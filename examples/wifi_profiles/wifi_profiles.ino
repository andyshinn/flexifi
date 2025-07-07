/*
 * Flexifi WiFi Profile Management Example
 * 
 * This example demonstrates the multiple WiFi profile storage and management
 * capabilities of the Flexifi library. It shows how to:
 * - Add multiple WiFi profiles with different priorities
 * - Auto-connect to the highest priority available network
 * - Manage profiles through the captive portal
 * - Handle profile-based connection logic
 * 
 * Features demonstrated:
 * - Multiple WiFi profile storage (up to 10 profiles)
 * - Priority-based auto-connection
 * - Profile management through web interface
 * - Automatic retry logic with backoff
 * - Profile migration from legacy credentials
 * 
 * Hardware required:
 * - ESP32 development board
 * - Optional: LED for status indication
 * 
 * Author: Andy Shinn
 * License: MIT
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Flexifi.h>

// Configuration
const char* AP_NAME = "Flexifi-Profiles";
const char* AP_PASSWORD = "flexifi123";
const int STATUS_LED = 2;

// Web server and Flexifi instances
AsyncWebServer server(80);
Flexifi flexifi(&server);

// State management
enum class AppState {
    INITIALIZING,
    CHECKING_PROFILES,
    AUTO_CONNECTING,
    WIFI_CONNECTED,
    PORTAL_STARTING,
    PORTAL_ACTIVE,
    ERROR_STATE
};

AppState currentState = AppState::INITIALIZING;
unsigned long stateStartTime = 0;
unsigned long lastStatusCheck = 0;
bool ledState = false;

// Profile management variables
int profileCount = 0;
String connectedSSID = "";
bool autoConnectCompleted = false;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("=== Flexifi WiFi Profile Management Example ===");
    
    // Initialize status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    
    // Initialize Flexifi
    if (!flexifi.init()) {
        Serial.println("❌ Failed to initialize Flexifi");
        currentState = AppState::ERROR_STATE;
        return;
    }
    
    // Set up some example WiFi profiles
    setupExampleProfiles();
    
    // Configure Flexifi settings
    flexifi.setTemplate("modern");
    flexifi.setPortalTimeout(5 * 60 * 1000); // 5 minutes
    flexifi.setConnectTimeout(15 * 1000);    // 15 seconds
    flexifi.setAutoConnectEnabled(true);
    flexifi.setMinSignalQuality(-75);        // Only connect to decent signals
    
    // Add custom parameters for profile management
    flexifi.addParameter("device_name", "Device Name", "ESP32-Device", 32);
    flexifi.addParameter("location", "Location", "Living Room", 32);
    
    // Set up event callbacks
    setupEventCallbacks();
    
    // Start with checking profiles
    changeState(AppState::CHECKING_PROFILES);
    
    Serial.println("✅ Setup completed");
}

void loop() {
    // Update Flexifi
    flexifi.loop();
    
    // Handle state machine
    handleStateMachine();
    
    // Update status LED
    updateStatusLED();
    
    // Check profiles periodically
    if (millis() - lastStatusCheck > 30000) { // Every 30 seconds
        checkProfileStatus();
        lastStatusCheck = millis();
    }
    
    delay(100);
}

void setupExampleProfiles() {
    Serial.println("🔧 Setting up example WiFi profiles...");
    
    // Check if we already have profiles
    profileCount = flexifi.getWiFiProfileCount();
    
    if (profileCount > 0) {
        Serial.printf("📋 Found %d existing WiFi profiles\n", profileCount);
        Serial.println("🔄 Loading existing profiles...");
        printProfileStatus();
        return;
    }
    
    // Add example profiles with different priorities
    Serial.println("➕ Adding example WiFi profiles...");
    
    // High priority networks (home/office)
    flexifi.addWiFiProfile("HomeNetwork", "homepassword123", 100);
    flexifi.addWiFiProfile("OfficeWiFi", "officepassword456", 90);
    
    // Medium priority networks (backup/mobile)
    flexifi.addWiFiProfile("GuestNetwork", "guestpass789", 50);
    flexifi.addWiFiProfile("MobileHotspot", "mobilepass321", 40);
    
    // Low priority networks (public/temporary)
    flexifi.addWiFiProfile("CafeWiFi", "cafepassword", 20);
    flexifi.addWiFiProfile("LibraryWiFi", "librarypass", 15);
    
    profileCount = flexifi.getWiFiProfileCount();
    Serial.printf("✅ Added %d WiFi profiles\n", profileCount);
    
    printProfileStatus();
}

void setupEventCallbacks() {
    Serial.println("🔗 Setting up event callbacks...");
    
    // Portal events
    flexifi.onPortalStart([]() {
        Serial.println("🚀 Portal started");
        changeState(AppState::PORTAL_ACTIVE);
    });
    
    flexifi.onPortalStop([]() {
        Serial.println("🛑 Portal stopped");
        changeState(AppState::CHECKING_PROFILES);
    });
    
    // WiFi events
    flexifi.onWiFiConnect([](const String& ssid) {
        Serial.printf("📶 Connected to WiFi: %s\n", ssid.c_str());
        connectedSSID = ssid;
        changeState(AppState::WIFI_CONNECTED);
        
        // Update profile last used time
        flexifi.updateProfileLastUsed(ssid);
    });
    
    flexifi.onWiFiDisconnect([]() {
        Serial.println("📵 WiFi disconnected");
        connectedSSID = "";
        changeState(AppState::CHECKING_PROFILES);
    });
    
    // Connection events
    flexifi.onConnectStart([](const String& ssid) {
        Serial.printf("🔄 Attempting to connect to: %s\n", ssid.c_str());
    });
    
    flexifi.onConnectFailed([](const String& ssid) {
        Serial.printf("❌ Failed to connect to: %s\n", ssid.c_str());
    });
    
    // Configuration events
    flexifi.onConfigSave([](const String& ssid, const String& password) {
        Serial.printf("💾 New WiFi profile saved: %s\n", ssid.c_str());
        
        // Add the new profile with medium priority
        flexifi.addWiFiProfile(ssid, password, 60);
        
        Serial.printf("📋 Total profiles: %d\n", flexifi.getWiFiProfileCount());
    });
    
    // Scan events
    flexifi.onScanComplete([](int networkCount) {
        Serial.printf("📡 Network scan completed: %d networks found\n", networkCount);
    });
}

void handleStateMachine() {
    unsigned long stateTime = millis() - stateStartTime;
    
    switch (currentState) {
        case AppState::INITIALIZING:
            // Already handled in setup
            break;
            
        case AppState::CHECKING_PROFILES:
            handleCheckingProfilesState(stateTime);
            break;
            
        case AppState::AUTO_CONNECTING:
            handleAutoConnectingState(stateTime);
            break;
            
        case AppState::WIFI_CONNECTED:
            handleWiFiConnectedState(stateTime);
            break;
            
        case AppState::PORTAL_STARTING:
            handlePortalStartingState(stateTime);
            break;
            
        case AppState::PORTAL_ACTIVE:
            handlePortalActiveState(stateTime);
            break;
            
        case AppState::ERROR_STATE:
            handleErrorState(stateTime);
            break;
    }
}

void handleCheckingProfilesState(unsigned long stateTime) {
    if (stateTime < 2000) {
        return; // Wait a bit before checking
    }
    
    // Check if we have WiFi profiles
    if (flexifi.getWiFiProfileCount() == 0) {
        Serial.println("⚠️  No WiFi profiles found, starting portal");
        changeState(AppState::PORTAL_STARTING);
        return;
    }
    
    // Check if we're already connected
    if (flexifi.getWiFiState() == WiFiState::CONNECTED) {
        Serial.println("✅ Already connected to WiFi");
        changeState(AppState::WIFI_CONNECTED);
        return;
    }
    
    // Try auto-connect
    if (!autoConnectCompleted) {
        Serial.println("🔄 Attempting auto-connect to saved profiles...");
        changeState(AppState::AUTO_CONNECTING);
    } else {
        Serial.println("⚠️  Auto-connect failed, starting portal");
        changeState(AppState::PORTAL_STARTING);
    }
}

void handleAutoConnectingState(unsigned long stateTime) {
    // Try auto-connect
    if (flexifi.autoConnect()) {
        Serial.println("🚀 Auto-connect initiated");
        return;
    }
    
    // Check if connection succeeded
    if (flexifi.getWiFiState() == WiFiState::CONNECTED) {
        Serial.println("✅ Auto-connect successful");
        changeState(AppState::WIFI_CONNECTED);
        return;
    }
    
    // Timeout after 30 seconds
    if (stateTime > 30000) {
        Serial.println("⏰ Auto-connect timeout");
        autoConnectCompleted = true;
        changeState(AppState::PORTAL_STARTING);
    }
}

void handleWiFiConnectedState(unsigned long stateTime) {
    // Check if we're still connected
    if (flexifi.getWiFiState() != WiFiState::CONNECTED) {
        Serial.println("❌ WiFi connection lost");
        changeState(AppState::CHECKING_PROFILES);
        return;
    }
    
    // Print status every 60 seconds
    if (stateTime > 0 && stateTime % 60000 == 0) {
        printConnectionStatus();
    }
}

void handlePortalStartingState(unsigned long stateTime) {
    if (stateTime < 1000) {
        return; // Wait a bit before starting portal
    }
    
    Serial.printf("🚀 Starting captive portal: %s\n", AP_NAME);
    
    if (flexifi.startPortal(AP_NAME, AP_PASSWORD)) {
        Serial.println("✅ Portal started successfully");
        changeState(AppState::PORTAL_ACTIVE);
    } else {
        Serial.println("❌ Failed to start portal");
        changeState(AppState::ERROR_STATE);
    }
}

void handlePortalActiveState(unsigned long stateTime) {
    // Check if we've connected to WiFi
    if (flexifi.getWiFiState() == WiFiState::CONNECTED) {
        Serial.println("✅ WiFi connected via portal");
        flexifi.stopPortal();
        changeState(AppState::WIFI_CONNECTED);
        return;
    }
    
    // Print status every 30 seconds
    if (stateTime > 0 && stateTime % 30000 == 0) {
        printPortalStatus();
    }
}

void handleErrorState(unsigned long stateTime) {
    // Flash LED rapidly in error state
    if (stateTime % 200 == 0) {
        ledState = !ledState;
        digitalWrite(STATUS_LED, ledState);
    }
    
    // Print error status every 10 seconds
    if (stateTime > 0 && stateTime % 10000 == 0) {
        Serial.println("💥 System in error state - check configuration");
    }
}

void updateStatusLED() {
    static unsigned long lastLEDUpdate = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastLEDUpdate < 500) {
        return;
    }
    
    lastLEDUpdate = currentTime;
    
    switch (currentState) {
        case AppState::INITIALIZING:
        case AppState::CHECKING_PROFILES:
            // Slow blink
            ledState = !ledState;
            digitalWrite(STATUS_LED, ledState);
            break;
            
        case AppState::AUTO_CONNECTING:
            // Fast blink
            ledState = !ledState;
            digitalWrite(STATUS_LED, ledState);
            delay(100);
            break;
            
        case AppState::WIFI_CONNECTED:
            // Solid on
            digitalWrite(STATUS_LED, HIGH);
            break;
            
        case AppState::PORTAL_STARTING:
        case AppState::PORTAL_ACTIVE:
            // Double blink
            digitalWrite(STATUS_LED, HIGH);
            delay(100);
            digitalWrite(STATUS_LED, LOW);
            delay(100);
            digitalWrite(STATUS_LED, HIGH);
            delay(100);
            digitalWrite(STATUS_LED, LOW);
            break;
            
        case AppState::ERROR_STATE:
            // Handled in handleErrorState
            break;
    }
}

void changeState(AppState newState) {
    Serial.printf("🔄 State change: %s -> %s\n", 
                  stateToString(currentState).c_str(), 
                  stateToString(newState).c_str());
    
    currentState = newState;
    stateStartTime = millis();
}

String stateToString(AppState state) {
    switch (state) {
        case AppState::INITIALIZING: return "INITIALIZING";
        case AppState::CHECKING_PROFILES: return "CHECKING_PROFILES";
        case AppState::AUTO_CONNECTING: return "AUTO_CONNECTING";
        case AppState::WIFI_CONNECTED: return "WIFI_CONNECTED";
        case AppState::PORTAL_STARTING: return "PORTAL_STARTING";
        case AppState::PORTAL_ACTIVE: return "PORTAL_ACTIVE";
        case AppState::ERROR_STATE: return "ERROR_STATE";
        default: return "UNKNOWN";
    }
}

void printProfileStatus() {
    Serial.println("\n📋 WiFi Profile Status:");
    Serial.println("========================");
    
    String profilesJSON = flexifi.getWiFiProfilesJSON();
    Serial.printf("Profiles JSON: %s\n", profilesJSON.c_str());
    
    Serial.printf("Profile count: %d\n", flexifi.getWiFiProfileCount());
    Serial.printf("Auto-connect enabled: %s\n", 
                  flexifi.isAutoConnectEnabled() ? "Yes" : "No");
    
    String highestPrioritySSID = flexifi.getHighestPrioritySSID();
    if (!highestPrioritySSID.isEmpty()) {
        Serial.printf("Highest priority SSID: %s\n", highestPrioritySSID.c_str());
    }
    
    Serial.println("========================\n");
}

void printConnectionStatus() {
    Serial.println("\n📶 Connection Status:");
    Serial.println("=====================");
    
    Serial.printf("WiFi State: %s\n", wifiStateToString(flexifi.getWiFiState()).c_str());
    Serial.printf("Connected SSID: %s\n", connectedSSID.c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    
    Serial.println("=====================\n");
}

void printPortalStatus() {
    Serial.println("\n🌐 Portal Status:");
    Serial.println("==================");
    
    Serial.printf("Portal State: %s\n", portalStateToString(flexifi.getPortalState()).c_str());
    Serial.printf("AP Name: %s\n", AP_NAME);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("Connected clients: %d\n", WiFi.softAPgetStationNum());
    
    Serial.println("==================\n");
}

void checkProfileStatus() {
    if (currentState == AppState::WIFI_CONNECTED) {
        // Check if we're still connected to the best available network
        String currentSSID = flexifi.getConnectedSSID();
        String highestPrioritySSID = flexifi.getHighestPrioritySSID();
        
        if (!currentSSID.equals(highestPrioritySSID)) {
            Serial.printf("🔄 Better network available: %s (current: %s)\n", 
                         highestPrioritySSID.c_str(), currentSSID.c_str());
            
            // Note: In a real implementation, you might want to add logic
            // to switch to higher priority networks when they become available
        }
    }
}

String wifiStateToString(WiFiState state) {
    switch (state) {
        case WiFiState::DISCONNECTED: return "DISCONNECTED";
        case WiFiState::CONNECTING: return "CONNECTING";
        case WiFiState::CONNECTED: return "CONNECTED";
        case WiFiState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

String portalStateToString(PortalState state) {
    switch (state) {
        case PortalState::STOPPED: return "STOPPED";
        case PortalState::STARTING: return "STARTING";
        case PortalState::ACTIVE: return "ACTIVE";
        case PortalState::STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}