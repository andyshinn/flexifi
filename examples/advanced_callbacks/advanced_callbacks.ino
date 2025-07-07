/*
 * Flexifi Advanced Callbacks Example
 * 
 * This example demonstrates advanced usage of the Flexifi library including:
 * - Custom parameters for device configuration
 * - Network quality filtering
 * - Advanced event handling
 * - Parameter validation and storage
 * 
 * Hardware Requirements:
 * - ESP32 development board
 * 
 * Library Dependencies:
 * - ESPAsyncWebServer
 * - ArduinoJson
 * - LittleFS (included with ESP32 core)
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Flexifi.h>
#include <FlexifiParameter.h>

// Create AsyncWebServer instance
AsyncWebServer server(80);

// Create Flexifi portal instance
Flexifi portal(&server);

// Configuration variables that will be set via custom parameters
String deviceName = "FlexifiDevice";
String mqttServer = "";
int mqttPort = 1883;
String mqttUser = "";
String mqttPassword = "";
bool enableOTA = false;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("Flexifi Advanced Callbacks Example");
    Serial.println("==================================");
    
    // Configure portal settings
    portal.setTemplate("modern");
    portal.setMinSignalQuality(-75); // Only show networks with signal > -75 dBm
    
    // Add custom parameters for device configuration
    setupCustomParameters();
    
    // Set up comprehensive event callbacks
    setupEventCallbacks();
    
    // Try to load saved configuration
    if (portal.loadConfig()) {
        Serial.println("Found saved WiFi configuration");
        loadCustomParameterValues();
        
        // Try to connect with saved credentials
        if (attemptSavedConnection()) {
            Serial.println("✓ Connected using saved credentials");
            startMainApplication();
        } else {
            Serial.println("✗ Failed to connect with saved credentials");
            startPortalMode();
        }
    } else {
        Serial.println("No saved configuration found");
        startPortalMode();
    }
    
    // Start the web server
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    // Handle portal events
    portal.loop();
    
    // Your main application code here
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 30000) { // Every 30 seconds
        printDeviceStatus();
        lastStatus = millis();
    }
    
    delay(10);
}

void setupCustomParameters() {
    Serial.println("Setting up custom parameters...");
    
    // Device Name parameter
    portal.addParameter("device_name", "Device Name", deviceName, 32);
    portal.getParameter("device_name")->setPlaceholder("Enter device name");
    portal.getParameter("device_name")->setRequired(true);
    
    // MQTT Server parameter
    portal.addParameter("mqtt_server", "MQTT Server", mqttServer, 64);
    portal.getParameter("mqtt_server")->setPlaceholder("mqtt.example.com");
    
    // MQTT Port parameter
    FlexifiParameter* mqttPortParam = new FlexifiParameter("mqtt_port", "MQTT Port", 
                                                          String(mqttPort), 5, ParameterType::NUMBER);
    mqttPortParam->setPlaceholder("1883");
    portal.addParameter(mqttPortParam);
    
    // MQTT Username parameter
    portal.addParameter("mqtt_user", "MQTT Username", mqttUser, 32);
    portal.getParameter("mqtt_user")->setPlaceholder("Optional");
    
    // MQTT Password parameter
    FlexifiParameter* mqttPassParam = new FlexifiParameter("mqtt_password", "MQTT Password", 
                                                          mqttPassword, 64, ParameterType::PASSWORD);
    mqttPassParam->setPlaceholder("Optional");
    portal.addParameter(mqttPassParam);
    
    // OTA Enable checkbox
    FlexifiParameter* otaParam = new FlexifiParameter("enable_ota", "Enable OTA Updates", 
                                                     enableOTA ? "1" : "0", 1, ParameterType::CHECKBOX);
    portal.addParameter(otaParam);
    
    Serial.printf("Added %d custom parameters\n", portal.getParameterCount());
}

void setupEventCallbacks() {
    Serial.println("Setting up event callbacks...");
    
    // Portal lifecycle events
    portal.onPortalStart([]() {
        Serial.println("🌐 Portal started");
        Serial.println("📱 Connect to WiFi: 'FlexifiDevice-Setup'");
        Serial.println("🌍 Open browser: http://192.168.4.1");
    });
    
    portal.onPortalStop([]() {
        Serial.println("🌐 Portal stopped");
    });
    
    // WiFi connection events
    portal.onWiFiConnect([](const String& ssid) {
        Serial.printf("✅ Connected to WiFi: %s\n", ssid.c_str());
        Serial.printf("📡 IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("📶 Signal Strength: %d dBm\n", WiFi.RSSI());
        
        // Stop portal after successful connection
        portal.stopPortal();
        
        // Load and apply custom parameters
        loadCustomParameterValues();
        
        // Start main application
        startMainApplication();
    });
    
    portal.onWiFiDisconnect([]() {
        Serial.println("❌ WiFi disconnected");
        
        // Optionally restart portal on disconnection
        if (!portal.isPortalActive()) {
            Serial.println("🔄 Restarting portal due to disconnection...");
            delay(5000); // Wait a bit before restarting
            startPortalMode();
        }
    });
    
    portal.onConnectStart([](const String& ssid) {
        Serial.printf("🔄 Attempting to connect to: %s\n", ssid.c_str());
    });
    
    portal.onConnectFailed([](const String& ssid) {
        Serial.printf("❌ Failed to connect to: %s\n", ssid.c_str());
    });
    
    // Scan events
    portal.onScanComplete([](int networkCount) {
        Serial.printf("📡 Network scan completed: %d networks found\n", networkCount);
        if (networkCount == 0) {
            Serial.println("⚠️ No networks found with sufficient signal quality");
            Serial.printf("   Minimum signal quality: %d dBm\n", portal.getMinSignalQuality());
        }
    });
    
    // Configuration save events
    portal.onConfigSave([](const String& ssid, const String& password) {
        Serial.printf("💾 Configuration saved for: %s\n", ssid.c_str());
        
        // Validate and save custom parameters
        if (validateCustomParameters()) {
            saveCustomParameterValues();
            Serial.println("✅ Custom parameters validated and saved");
        } else {
            Serial.println("⚠️ Some custom parameters failed validation");
        }
    });
}

void loadCustomParameterValues() {
    Serial.println("Loading custom parameter values...");
    
    deviceName = portal.getParameterValue("device_name");
    if (deviceName.isEmpty()) deviceName = "FlexifiDevice";
    
    mqttServer = portal.getParameterValue("mqtt_server");
    
    String portStr = portal.getParameterValue("mqtt_port");
    if (!portStr.isEmpty()) {
        mqttPort = portStr.toInt();
        if (mqttPort <= 0 || mqttPort > 65535) mqttPort = 1883;
    }
    
    mqttUser = portal.getParameterValue("mqtt_user");
    mqttPassword = portal.getParameterValue("mqtt_password");
    enableOTA = (portal.getParameterValue("enable_ota") == "1");
    
    // Apply the loaded configuration
    WiFi.setHostname(deviceName.c_str());
    
    Serial.println("📋 Loaded configuration:");
    Serial.printf("   Device Name: %s\n", deviceName.c_str());
    Serial.printf("   MQTT Server: %s:%d\n", mqttServer.c_str(), mqttPort);
    Serial.printf("   MQTT User: %s\n", mqttUser.isEmpty() ? "(none)" : mqttUser.c_str());
    Serial.printf("   OTA Enabled: %s\n", enableOTA ? "Yes" : "No");
}

bool validateCustomParameters() {
    bool valid = true;
    
    // Validate device name
    String name = portal.getParameterValue("device_name");
    if (name.isEmpty() || name.length() > 32) {
        Serial.println("❌ Invalid device name");
        valid = false;
    }
    
    // Validate MQTT port if provided
    String portStr = portal.getParameterValue("mqtt_port");
    if (!portStr.isEmpty()) {
        int port = portStr.toInt();
        if (port <= 0 || port > 65535) {
            Serial.println("❌ Invalid MQTT port");
            valid = false;
        }
    }
    
    return valid;
}

void saveCustomParameterValues() {
    // In a real application, you would save these to NVS or LittleFS
    // For this example, we'll just print them
    Serial.println("💾 Saving custom parameters...");
    Serial.printf("   Device Name: %s\n", portal.getParameterValue("device_name").c_str());
    Serial.printf("   MQTT Server: %s\n", portal.getParameterValue("mqtt_server").c_str());
    Serial.printf("   MQTT Port: %s\n", portal.getParameterValue("mqtt_port").c_str());
    Serial.printf("   MQTT User: %s\n", portal.getParameterValue("mqtt_user").c_str());
    Serial.printf("   OTA Enabled: %s\n", portal.getParameterValue("enable_ota").c_str());
}

bool attemptSavedConnection() {
    Serial.println("🔄 Attempting connection with saved credentials...");
    
    WiFi.begin();
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    
    return WiFi.status() == WL_CONNECTED;
}

void startPortalMode() {
    Serial.println("🌐 Starting captive portal mode...");
    portal.startPortal("FlexifiDevice-Setup");
}

void startMainApplication() {
    Serial.println("🚀 Starting main application...");
    
    // Here you would start your main application logic
    // For example:
    // - Connect to MQTT broker using saved parameters
    // - Initialize OTA if enabled
    // - Start sensors or other peripherals
    // - Set up additional web server routes
    
    if (!mqttServer.isEmpty()) {
        Serial.printf("📡 Would connect to MQTT: %s:%d\n", mqttServer.c_str(), mqttPort);
    }
    
    if (enableOTA) {
        Serial.println("🔄 Would initialize OTA updates");
    }
    
    Serial.println("✅ Main application started successfully");
}

void printDeviceStatus() {
    Serial.println("\n📊 Device Status:");
    Serial.printf("   WiFi: %s (%s)\n", 
                  WiFi.isConnected() ? "Connected" : "Disconnected",
                  WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "No IP");
    if (WiFi.isConnected()) {
        Serial.printf("   Signal: %d dBm\n", WiFi.RSSI());
    }
    Serial.printf("   Portal: %s\n", portal.isPortalActive() ? "Active" : "Inactive");
    Serial.printf("   Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("   Uptime: %lu seconds\n", millis() / 1000);
}

/*
 * Advanced Features Demonstrated:
 * 
 * 1. Custom Parameters:
 *    - Device name configuration
 *    - MQTT server settings with validation
 *    - Checkbox for feature toggles
 *    - Different input types (text, number, password, checkbox)
 * 
 * 2. Network Quality Filtering:
 *    - Minimum signal strength threshold (-75 dBm)
 *    - Automatic filtering of weak networks
 *    - Signal strength indicators in UI
 * 
 * 3. Advanced Event Handling:
 *    - Comprehensive callback system
 *    - Parameter validation
 *    - Custom configuration storage
 *    - Error handling and recovery
 * 
 * 4. Real-world Integration:
 *    - Hostname configuration
 *    - MQTT parameter handling
 *    - OTA feature toggling
 *    - Status monitoring
 * 
 * Usage Instructions:
 * 1. Upload sketch to ESP32
 * 2. Connect to "FlexifiDevice-Setup" WiFi
 * 3. Open http://192.168.4.1
 * 4. Fill in WiFi credentials and custom parameters
 * 5. Submit form to connect and configure device
 * 6. Device will remember settings for future boots
 */