/*
 * Flexifi Basic Usage Example
 * 
 * This example demonstrates the basic usage of the Flexifi library
 * for creating a captive portal WiFi manager on ESP32.
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

// Create AsyncWebServer instance
AsyncWebServer server(80);

// Create Flexifi portal instance
Flexifi portal(&server);

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("Flexifi Basic Usage Example");
    Serial.println("===========================");
    
    // Configure portal template (optional)
    portal.setTemplate("modern");  // Options: modern, classic, minimal
    
    // Set up event callbacks
    portal.onPortalStart([]() {
        Serial.println("✓ Portal started");
        Serial.println("Connect to WiFi network: 'Flexifi-Setup'");
        Serial.println("Open browser to: http://192.168.4.1");
    });
    
    portal.onPortalStop([]() {
        Serial.println("✓ Portal stopped");
    });
    
    portal.onWiFiConnect([](const String& ssid) {
        Serial.printf("✓ Connected to WiFi: %s\n", ssid.c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        
        // Stop portal after successful connection
        portal.stopPortal();
        
        // Start your main application here
        Serial.println("Starting main application...");
    });
    
    portal.onWiFiDisconnect([]() {
        Serial.println("✗ WiFi disconnected");
        
        // Optionally restart portal
        if (!portal.isPortalActive()) {
            Serial.println("Restarting portal...");
            portal.startPortal("Flexifi-Setup");
        }
    });
    
    portal.onScanComplete([](int networkCount) {
        Serial.printf("Found %d WiFi networks\n", networkCount);
    });
    
    // Try to load saved configuration
    if (portal.loadConfig()) {
        Serial.println("Found saved WiFi configuration");
        
        // Try to connect with saved credentials
        String savedSSID = portal.getConnectedSSID();
        Serial.printf("Attempting to connect to: %s\n", savedSSID.c_str());
        
        // Set a timeout for connection attempt
        unsigned long connectStart = millis();
        WiFi.begin();
        
        while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 10000) {
            delay(100);
            Serial.print(".");
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("✓ Connected using saved credentials");
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("✗ Failed to connect with saved credentials");
            Serial.println("Starting captive portal...");
            portal.startPortal("Flexifi-Setup");
        }
    } else {
        Serial.println("No saved configuration found");
        Serial.println("Starting captive portal...");
        portal.startPortal("Flexifi-Setup");
    }
    
    // Start the web server
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    // Handle portal events
    portal.loop();
    
    // Your main application code here
    // This runs whether connected to WiFi or in portal mode
    
    delay(10); // Small delay to prevent watchdog issues
}

/*
 * Usage Instructions:
 * 
 * 1. Upload this sketch to your ESP32
 * 2. Open Serial Monitor at 115200 baud
 * 3. When portal starts, connect to "Flexifi-Setup" WiFi network
 * 4. Open web browser to http://192.168.4.1
 * 5. Scan for networks and select your WiFi
 * 6. Enter password and click Connect
 * 7. The ESP32 will connect and remember your settings
 * 
 * Features demonstrated:
 * - Automatic portal startup when no saved config
 * - Modern responsive web interface
 * - Real-time network scanning
 * - Credential persistence
 * - Event callbacks for custom logic
 * - Automatic reconnection handling
 */