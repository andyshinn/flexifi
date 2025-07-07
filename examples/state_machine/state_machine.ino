/*
 * Flexifi State Machine Example
 * 
 * This example demonstrates how to use PortalState and WiFiState enums
 * in a state machine pattern for robust connection management, similar
 * to NetWizard's approach.
 * 
 * Features:
 * - State-driven application logic
 * - Automatic reconnection handling
 * - LED status indicators
 * - Serial state monitoring
 * - Timeout-based state transitions
 * 
 * Hardware Requirements:
 * - ESP32 development board
 * - LED on GPIO 2 (built-in LED on most boards)
 * - Optional: Button on GPIO 0 for manual portal trigger
 * 
 * Library Dependencies:
 * - ESPAsyncWebServer
 * - ArduinoJson
 * - LittleFS (included with ESP32 core)
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Flexifi.h>

// Pin definitions
#define LED_PIN 2
#define BUTTON_PIN 0

// Create AsyncWebServer instance
AsyncWebServer server(80);

// Create Flexifi portal instance
Flexifi portal(&server);

// Application states
enum class AppState {
    INITIALIZING,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    PORTAL_ACTIVE,
    PORTAL_TIMEOUT,
    ERROR_STATE,
    RUNNING
};

// Global state variables
AppState currentAppState = AppState::INITIALIZING;
unsigned long stateStartTime = 0;
unsigned long lastStatusPrint = 0;
unsigned long lastLEDToggle = 0;
bool ledState = false;

// Timing constants
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;  // 30 seconds
const unsigned long PORTAL_TIMEOUT = 300000;       // 5 minutes
const unsigned long STATUS_INTERVAL = 5000;        // 5 seconds
const unsigned long RECONNECT_DELAY = 10000;       // 10 seconds

// Connection retry management
int wifiRetryCount = 0;
const int MAX_WIFI_RETRIES = 3;
unsigned long lastRetryTime = 0;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("Flexifi State Machine Example");
    Serial.println("=============================");
    
    // Initialize hardware
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, LOW);
    
    // Configure portal
    portal.setTemplate("modern");
    portal.setMinSignalQuality(-70);
    
    // Add some custom parameters
    portal.addParameter("device_name", "Device Name", "FlexifiDevice", 32);
    portal.addParameter("update_interval", "Update Interval (sec)", "60", 5);
    
    // Set up event callbacks
    setupCallbacks();
    
    // Initialize state machine
    changeAppState(AppState::INITIALIZING);
    
    // Start web server
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    // Handle portal events
    portal.loop();
    
    // Run state machine
    handleStateMachine();
    
    // Handle LED status indication
    handleLEDStatus();
    
    // Handle button press (manual portal trigger)
    handleButton();
    
    // Print status periodically
    if (millis() - lastStatusPrint > STATUS_INTERVAL) {
        printCurrentStatus();
        lastStatusPrint = millis();
    }
    
    delay(10);
}

void handleStateMachine() {
    unsigned long now = millis();
    unsigned long stateUptime = now - stateStartTime;
    
    switch (currentAppState) {
        case AppState::INITIALIZING:
            handleInitializingState();
            break;
            
        case AppState::WIFI_CONNECTING:
            handleWiFiConnectingState(stateUptime);
            break;
            
        case AppState::WIFI_CONNECTED:
            handleWiFiConnectedState();
            break;
            
        case AppState::PORTAL_ACTIVE:
            handlePortalActiveState(stateUptime);
            break;
            
        case AppState::PORTAL_TIMEOUT:
            handlePortalTimeoutState();
            break;
            
        case AppState::ERROR_STATE:
            handleErrorState(stateUptime);
            break;
            
        case AppState::RUNNING:
            handleRunningState();
            break;
    }
}

void handleInitializingState() {
    Serial.println("🔄 Initializing system...");
    
    // Try to load saved configuration
    if (portal.loadConfig()) {
        Serial.println("📋 Found saved WiFi configuration");
        changeAppState(AppState::WIFI_CONNECTING);
    } else {
        Serial.println("📋 No saved configuration, starting portal");
        startPortalMode();
    }
}

void handleWiFiConnectingState(unsigned long stateUptime) {
    // Check WiFi state
    WiFiState wifiState = portal.getWiFiState();
    
    switch (wifiState) {
        case WiFiState::CONNECTED:
            Serial.println("✅ WiFi connected successfully");
            changeAppState(AppState::WIFI_CONNECTED);
            wifiRetryCount = 0; // Reset retry counter
            break;
            
        case WiFiState::FAILED:
            Serial.println("❌ WiFi connection failed");
            wifiRetryCount++;
            
            if (wifiRetryCount >= MAX_WIFI_RETRIES) {
                Serial.printf("⚠️ Max retries (%d) reached, starting portal\n", MAX_WIFI_RETRIES);
                startPortalMode();
            } else {
                Serial.printf("🔄 Retry %d/%d in %d seconds\n", 
                             wifiRetryCount, MAX_WIFI_RETRIES, RECONNECT_DELAY/1000);
                changeAppState(AppState::ERROR_STATE);
            }
            break;
            
        case WiFiState::CONNECTING:
            // Check for timeout
            if (stateUptime > WIFI_CONNECT_TIMEOUT) {
                Serial.println("⏰ WiFi connection timeout");
                portal.reset(); // Stop current connection attempt
                wifiRetryCount++;
                
                if (wifiRetryCount >= MAX_WIFI_RETRIES) {
                    Serial.printf("⚠️ Max retries (%d) reached, starting portal\n", MAX_WIFI_RETRIES);
                    startPortalMode();
                } else {
                    changeAppState(AppState::ERROR_STATE);
                }
            }
            break;
            
        case WiFiState::DISCONNECTED:
            // Still waiting for connection to start
            break;
    }
}

void handleWiFiConnectedState() {
    // Verify connection is stable
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("🌐 IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("📶 Signal Strength: %d dBm\n", WiFi.RSSI());
        changeAppState(AppState::RUNNING);
    } else {
        Serial.println("⚠️ WiFi connection lost during verification");
        changeAppState(AppState::ERROR_STATE);
    }
}

void handlePortalActiveState(unsigned long stateUptime) {
    PortalState portalState = portal.getPortalState();
    
    switch (portalState) {
        case PortalState::ACTIVE:
            // Check for timeout
            if (stateUptime > PORTAL_TIMEOUT) {
                Serial.println("⏰ Portal timeout reached");
                changeAppState(AppState::PORTAL_TIMEOUT);
            }
            break;
            
        case PortalState::STOPPED:
            // Portal was stopped (likely due to successful connection)
            Serial.println("🌐 Portal stopped");
            
            // Check if we're now connected
            if (WiFi.status() == WL_CONNECTED) {
                changeAppState(AppState::WIFI_CONNECTED);
            } else {
                // Portal stopped but no connection
                changeAppState(AppState::ERROR_STATE);
            }
            break;
            
        case PortalState::STARTING:
            // Still starting up
            break;
            
        case PortalState::STOPPING:
            // In the process of stopping
            break;
    }
}

void handlePortalTimeoutState() {
    Serial.println("⏰ Portal timed out, attempting saved connection again");
    portal.stopPortal();
    
    // Try saved connection one more time
    if (portal.loadConfig()) {
        wifiRetryCount = 0; // Reset retry counter
        changeAppState(AppState::WIFI_CONNECTING);
    } else {
        changeAppState(AppState::ERROR_STATE);
    }
}

void handleErrorState(unsigned long stateUptime) {
    // Wait for retry delay
    if (stateUptime > RECONNECT_DELAY) {
        Serial.printf("🔄 Retrying WiFi connection (attempt %d/%d)\n", 
                     wifiRetryCount, MAX_WIFI_RETRIES);
        
        // Attempt reconnection
        if (portal.loadConfig()) {
            changeAppState(AppState::WIFI_CONNECTING);
        } else {
            Serial.println("❌ No saved config available for retry");
            startPortalMode();
        }
    }
}

void handleRunningState() {
    // Main application logic runs here
    static unsigned long lastHeartbeat = 0;
    
    // Check WiFi connection periodically
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi connection lost during operation");
        changeAppState(AppState::ERROR_STATE);
        return;
    }
    
    // Heartbeat every 30 seconds
    if (millis() - lastHeartbeat > 30000) {
        Serial.println("💓 Application heartbeat - all systems operational");
        
        // Here you would run your main application logic:
        // - Send sensor data
        // - Check for updates
        // - Process MQTT messages
        // - Handle web requests
        
        lastHeartbeat = millis();
    }
}

void startPortalMode() {
    Serial.println("🌐 Starting captive portal mode");
    
    if (portal.startPortal("FlexifiDevice-Setup")) {
        changeAppState(AppState::PORTAL_ACTIVE);
    } else {
        Serial.println("❌ Failed to start portal");
        changeAppState(AppState::ERROR_STATE);
    }
}

void changeAppState(AppState newState) {
    if (newState != currentAppState) {
        Serial.printf("🔄 State change: %s -> %s\n", 
                     getAppStateName(currentAppState).c_str(),
                     getAppStateName(newState).c_str());
        
        currentAppState = newState;
        stateStartTime = millis();
        
        // State entry actions
        onStateEntry(newState);
    }
}

void onStateEntry(AppState state) {
    switch (state) {
        case AppState::WIFI_CONNECTING:
            Serial.println("📡 Attempting WiFi connection...");
            // Portal will handle the actual connection attempt
            break;
            
        case AppState::PORTAL_ACTIVE:
            Serial.println("📱 Portal active - connect to 'FlexifiDevice-Setup'");
            Serial.println("🌍 Open browser to: http://192.168.4.1");
            break;
            
        case AppState::RUNNING:
            Serial.println("🚀 Application running - all systems operational");
            break;
            
        case AppState::ERROR_STATE:
            Serial.println("⚠️ Entered error state - will retry shortly");
            break;
            
        default:
            break;
    }
}

void handleLEDStatus() {
    unsigned long now = millis();
    unsigned long blinkInterval = 1000; // Default slow blink
    
    // Set blink pattern based on current state
    switch (currentAppState) {
        case AppState::INITIALIZING:
            blinkInterval = 200; // Fast blink
            break;
            
        case AppState::WIFI_CONNECTING:
            blinkInterval = 500; // Medium blink
            break;
            
        case AppState::WIFI_CONNECTED:
            digitalWrite(LED_PIN, HIGH); // Solid on
            return;
            
        case AppState::PORTAL_ACTIVE:
            blinkInterval = 250; // Fast blink
            break;
            
        case AppState::ERROR_STATE:
            blinkInterval = 100; // Very fast blink
            break;
            
        case AppState::RUNNING:
            digitalWrite(LED_PIN, HIGH); // Solid on
            return;
            
        default:
            blinkInterval = 1000; // Slow blink
            break;
    }
    
    // Handle blinking
    if (now - lastLEDToggle > blinkInterval) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        lastLEDToggle = now;
    }
}

void handleButton() {
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;
    
    bool currentButtonState = digitalRead(BUTTON_PIN);
    
    // Detect button press (falling edge with debounce)
    if (lastButtonState == HIGH && currentButtonState == LOW) {
        unsigned long now = millis();
        if (now - lastButtonPress > 500) { // 500ms debounce
            Serial.println("🔘 Button pressed - triggering portal mode");
            
            // Stop current operations and start portal
            portal.stopPortal();
            portal.reset();
            delay(1000);
            startPortalMode();
            
            lastButtonPress = now;
        }
    }
    
    lastButtonState = currentButtonState;
}

void printCurrentStatus() {
    Serial.printf("📊 Status: %s | Portal: %s | WiFi: %s | Uptime: %lus\n",
                 getAppStateName(currentAppState).c_str(),
                 getPortalStateName(portal.getPortalState()).c_str(),
                 getWiFiStateName(portal.getWiFiState()).c_str(),
                 millis() / 1000);
}

// Utility functions for state names
String getAppStateName(AppState state) {
    switch (state) {
        case AppState::INITIALIZING: return "INITIALIZING";
        case AppState::WIFI_CONNECTING: return "WIFI_CONNECTING";
        case AppState::WIFI_CONNECTED: return "WIFI_CONNECTED";
        case AppState::PORTAL_ACTIVE: return "PORTAL_ACTIVE";
        case AppState::PORTAL_TIMEOUT: return "PORTAL_TIMEOUT";
        case AppState::ERROR_STATE: return "ERROR_STATE";
        case AppState::RUNNING: return "RUNNING";
        default: return "UNKNOWN";
    }
}

String getPortalStateName(PortalState state) {
    switch (state) {
        case PortalState::STOPPED: return "STOPPED";
        case PortalState::STARTING: return "STARTING";
        case PortalState::ACTIVE: return "ACTIVE";
        case PortalState::STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

String getWiFiStateName(WiFiState state) {
    switch (state) {
        case WiFiState::DISCONNECTED: return "DISCONNECTED";
        case WiFiState::CONNECTING: return "CONNECTING";
        case WiFiState::CONNECTED: return "CONNECTED";
        case WiFiState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

void setupCallbacks() {
    // Set up minimal callbacks for state transitions
    portal.onWiFiConnect([](const String& ssid) {
        Serial.printf("✅ WiFi callback: Connected to %s\n", ssid.c_str());
    });
    
    portal.onWiFiDisconnect([]() {
        Serial.println("❌ WiFi callback: Disconnected");
    });
    
    portal.onPortalStart([]() {
        Serial.println("🌐 Portal callback: Started");
    });
    
    portal.onPortalStop([]() {
        Serial.println("🌐 Portal callback: Stopped");
    });
}

/*
 * State Machine Flow:
 * 
 * INITIALIZING
 *     ↓ (has saved config)
 * WIFI_CONNECTING ←──────┐
 *     ↓ (success)        │ (retry)
 * WIFI_CONNECTED         │
 *     ↓                  │
 * RUNNING ───────────────┘ (disconnect)
 *     ↓ (fail/timeout)
 * ERROR_STATE ───────────┘
 *     ↓ (max retries)
 * PORTAL_ACTIVE
 *     ↓ (timeout)
 * PORTAL_TIMEOUT ────────┘
 * 
 * Features Demonstrated:
 * 
 * 1. State-Driven Logic:
 *    - Clean separation of concerns
 *    - Timeout handling per state
 *    - Automatic retry logic
 *    - Error recovery
 * 
 * 2. Visual Feedback:
 *    - LED status indicators
 *    - Serial state monitoring
 *    - Real-time status updates
 * 
 * 3. Manual Control:
 *    - Button-triggered portal mode
 *    - Debounced input handling
 *    - Emergency override
 * 
 * 4. Robust Connection Management:
 *    - Configurable retry limits
 *    - Connection verification
 *    - Automatic reconnection
 *    - Graceful error handling
 * 
 * Usage Instructions:
 * 1. Upload sketch to ESP32
 * 2. Monitor Serial output for state changes
 * 3. Watch LED for visual status indication
 * 4. Press button (GPIO 0) to manually trigger portal
 * 5. Connect to "FlexifiDevice-Setup" when portal is active
 * 6. Configure WiFi via http://192.168.4.1
 */