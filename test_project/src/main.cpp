/*
 * Flexifi Local Test Project
 *
 * This is a simple test to verify the Flexifi library works correctly
 * when loaded as a local dependency in PlatformIO, with NeoPixel status indication.
 */

#include "main.h"
#include "esp_log.h"

static const char* TAG = "FlexifiTest";

// Web server and Flexifi instances
AsyncWebServer server(80);
Flexifi portal(&server, true); // Enable password generation

// NeoPixel configuration
Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Status colors (must be defined after pixel object)
const uint32_t COLOR_OFF = pixel.Color(0, 0, 0);           // Black - off
const uint32_t COLOR_SCANNING = pixel.Color(0, 0, 255);    // Blue - scanning for networks
const uint32_t COLOR_CONNECTING = pixel.Color(255, 255, 0); // Yellow - connecting to WiFi
const uint32_t COLOR_CONNECTED = pixel.Color(0, 255, 0);   // Green - connected to WiFi
const uint32_t COLOR_PORTAL_STARTING = pixel.Color(255, 0, 255); // Magenta - portal starting
const uint32_t COLOR_PORTAL_ACTIVE = pixel.Color(255, 69, 0);    // Red-Orange - portal active (more distinct from yellow)
const uint32_t COLOR_ERROR = pixel.Color(255, 0, 0);       // Red - error state

void setup() {
    Serial.begin(115200);
    delay(1000); // Give serial time to initialize

    // Set ESP_LOG level to DEBUG to see all debug messages
    esp_log_level_set("*", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "    Flexifi Local Test Project   ");
    ESP_LOGI(TAG, "=================================");

    // Initialize NeoPixel power pin (if different from data pin)
    if (NEOPIXEL_POWER_PIN != NEOPIXEL_PIN) {
        pinMode(NEOPIXEL_POWER_PIN, OUTPUT);
        digitalWrite(NEOPIXEL_POWER_PIN, HIGH); // Pull power pin high
        delay(10); // Give power time to stabilize
        ESP_LOGI(TAG, "✅ NeoPixel power pin %d initialized", NEOPIXEL_POWER_PIN);
    } else {
        ESP_LOGW(TAG, "⚠️ NeoPixel power and data pins are the same - skipping separate power pin setup");
    }

    // Initialize NeoPixel
    pixel.begin();
    pixel.setBrightness(50); // Set to 50% brightness to avoid being too bright
    setNeoPixelColor(COLOR_OFF);
    ESP_LOGI(TAG, "🌈 NeoPixel initialized");

    // Initialize Flexifi
    ESP_LOGI(TAG, "🔧 Initializing Flexifi...");

    if (!portal.init()) {
        ESP_LOGE(TAG, "❌ Failed to initialize Flexifi");
        while(1) {
            setNeoPixelColor(COLOR_ERROR);
            delay(100);
            setNeoPixelColor(COLOR_OFF);
            delay(100);
        }
    }

    ESP_LOGI(TAG, "✅ Flexifi initialized successfully");

    // Configure Flexifi
    portal.setTemplate("modern");
    portal.setPortalTimeout(5 * 60 * 1000); // 5 minutes
    portal.setAutoConnectEnabled(true);

    // Set very low signal quality threshold for debugging
    // portal.setMinSignalQuality(-100); // Accept very weak signals

#ifdef FLEXIFI_MDNS
    // Add mDNS hostname parameter
    portal.addParameter("mdns_hostname", "mDNS Hostname", portal.getMDNSHostname(), 32);
#endif

    // Clear existing profiles to test "no profiles" scenario
    // Comment this out if you want to test with existing profiles
    // if (portal.getWiFiProfileCount() > 0) {
    //     ESP_LOGI(TAG, "🧹 Clearing existing WiFi profiles for testing...");
    //     portal.clearAllWiFiProfiles();
    // }

    // Add test WiFi profiles (uncomment to test auto-connect scenarios)
    // ESP_LOGI(TAG, "📋 Adding test WiFi profiles...");
    // portal.addWiFiProfile("TestNetwork1", "password123", 100);
    // portal.addWiFiProfile("TestNetwork2", "password456", 80);
    // portal.addWiFiProfile("TestNetwork3", "password789", 60);

    ESP_LOGI(TAG, "✅ WiFi profiles in storage: %d", portal.getWiFiProfileCount());

    // Debug: List all WiFi profiles
    ESP_LOGI(TAG, "📋 All saved WiFi profiles:");
    String profilesJSON = portal.getWiFiProfilesJSON();
    ESP_LOGI(TAG, "%s", profilesJSON.c_str());

    // Set up event callbacks
    portal.onPortalStart([]() {
        ESP_LOGI(TAG, "🚀 Portal started - Connect to 'Flexifi Test' AP");
        ESP_LOGI(TAG, "📱 Open http://192.168.4.1 in your browser");

        // Set orange color for active portal
        setNeoPixelColor(COLOR_PORTAL_ACTIVE);
    });

    portal.onWiFiConnect([](const String& ssid) {
        ESP_LOGI(TAG, "✅ Connected to WiFi: %s", ssid.c_str());
        ESP_LOGI(TAG, "🌐 IP Address: %s", WiFi.localIP().toString().c_str());

        // Set green color for connected
        setNeoPixelColor(COLOR_CONNECTED);

        // Stop portal after successful connection
        portal.stopPortal();
    });

    portal.onWiFiDisconnect([]() {
        ESP_LOGI(TAG, "📵 WiFi disconnected");
        setNeoPixelColor(COLOR_OFF);
    });

    portal.onConnectStart([](const String& ssid) {
        ESP_LOGI(TAG, "🔄 Attempting to connect to: %s", ssid.c_str());
        setNeoPixelColor(COLOR_CONNECTING);
    });

    portal.onConnectFailed([](const String& ssid) {
        ESP_LOGW(TAG, "❌ Failed to connect to: %s", ssid.c_str());
        setNeoPixelColor(COLOR_ERROR);
    });

    portal.onConfigSave([](const String& ssid, const String& password) {
        ESP_LOGI(TAG, "💾 New WiFi configuration saved: %s", ssid.c_str());

#ifdef FLEXIFI_MDNS
        // Check if mDNS hostname parameter was updated
        String newHostname = portal.getParameterValue("mdns_hostname");
        if (!newHostname.isEmpty() && newHostname != portal.getMDNSHostname()) {
            ESP_LOGI(TAG, "🏷️  Updating mDNS hostname to: %s", newHostname.c_str());
            portal.setMDNSHostname(newHostname);
        }
#endif
    });

    portal.onScanComplete([](int networkCount) {
        ESP_LOGD(TAG, "📡 Scan completed: found %d networks", networkCount);
        // Keep current color - don't change it just for scan completion
    });

    // WiFi connection logic based on profiles
    int profileCount = portal.getWiFiProfileCount();
    ESP_LOGI(TAG, "🔍 Profile count check: %d profiles found", profileCount);

    if (profileCount == 0) {
        ESP_LOGI(TAG, "📭 No WiFi profiles found, starting captive portal...");

        if (portal.startPortal("Flexifi Test")) { // No password - will use generated one
            ESP_LOGI(TAG, "✅ Captive portal started successfully");
            ESP_LOGI(TAG, "📶 SSID: Flexifi Test");
            ESP_LOGI(TAG, "🔐 Password: %s", portal.getGeneratedPassword().c_str());
            ESP_LOGI(TAG, "🌐 Portal URL: http://192.168.4.1");

            // Start initial WiFi scan immediately
            ESP_LOGI(TAG, "🔍 Starting initial WiFi scan...");
            setNeoPixelColor(COLOR_SCANNING);
            bool scanStarted = portal.scanNetworks(true); // Bypass throttle for initial scan
            ESP_LOGI(TAG, "🔍 Initial scan result: %s", scanStarted ? "SUCCESS" : "FAILED");
        } else {
            ESP_LOGE(TAG, "❌ Failed to start captive portal");
        }
    } else {
        ESP_LOGI(TAG, "🔍 Found %d WiFi profile(s), starting auto-connect (continuous retry)...", profileCount);
        ESP_LOGI(TAG, "🎯 Highest priority SSID: %s", portal.getHighestPrioritySSID().c_str());
        ESP_LOGI(TAG, "💡 Portal mode can only be triggered manually (e.g., button press)");
        setNeoPixelColor(COLOR_SCANNING);

        // Start auto-connect - it will continuously retry
        portal.autoConnect();
    }

    // Start the web server
    server.begin();
    ESP_LOGI(TAG, "🌐 Web server started");

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "✅ Setup completed - entering main loop");
    ESP_LOGI(TAG, "=================================");
    Serial.println("Generated WiFi password: " + portal.getGeneratedPassword());
}

void loop() {
    // Update Flexifi
    portal.loop();

#ifdef FLEXIFI_DEBUG_LEVEL
    // Update NeoPixel status every 500ms
    static unsigned long lastNeoPixelUpdate = 0;
    if (millis() - lastNeoPixelUpdate > 500) {
        updateNeoPixelStatus();
        lastNeoPixelUpdate = millis();
    }

    // Print status every 30 seconds
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 30000) {
        printStatus();
        lastStatusPrint = millis();
    }
#endif

    // delay(100);
}

// =============================================================================
// Status and Utility Functions
// =============================================================================

void printStatus() {
    ESP_LOGI(TAG, "📊 Status Report:");
    ESP_LOGI(TAG, "==================");

    // WiFi status
    WiFiState wifiState = portal.getWiFiState();
    ESP_LOGI(TAG, "WiFi State: %s", wifiStateToString(wifiState).c_str());

    if (wifiState == WiFiState::CONNECTED) {
        ESP_LOGI(TAG, "Connected SSID: %s", portal.getConnectedSSID().c_str());
        ESP_LOGI(TAG, "IP Address: %s", WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, "Signal Strength: %d dBm", WiFi.RSSI());
    }

    // Portal status
    PortalState portalState = portal.getPortalState();
    ESP_LOGI(TAG, "Portal State: %s", portalStateToString(portalState).c_str());

    if (portalState == PortalState::ACTIVE) {
        ESP_LOGI(TAG, "AP IP: %s", WiFi.softAPIP().toString().c_str());
        ESP_LOGI(TAG, "Connected clients: %d", WiFi.softAPgetStationNum());
    }

    // Profile status - cache values to prevent debug log interruption
    int profileCount = portal.getWiFiProfileCount();
    bool autoConnectEnabled = portal.isAutoConnectEnabled();
    String highestPrioritySSID = portal.getHighestPrioritySSID();

    ESP_LOGI(TAG, "WiFi Profiles: %d", profileCount);
    ESP_LOGI(TAG, "Auto-connect: %s", autoConnectEnabled ? "Enabled" : "Disabled");

    if (!highestPrioritySSID.isEmpty()) {
        ESP_LOGI(TAG, "Highest Priority: %s", highestPrioritySSID.c_str());
    }

    // Show generated password if portal is active (for LCD display demo)
    if (portalState == PortalState::ACTIVE && !portal.getGeneratedPassword().isEmpty()) {
        ESP_LOGI(TAG, "Generated Password: %s", portal.getGeneratedPassword().c_str());
    }

    ESP_LOGI(TAG, "Uptime: %lu seconds", millis() / 1000);
    ESP_LOGI(TAG, "==================");
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

// =============================================================================
// NeoPixel Status Functions
// =============================================================================

void setNeoPixelColor(uint32_t color, bool immediate) {
    pixel.setPixelColor(0, color);
    if (immediate) {
        pixel.show();
    }
}

void updateNeoPixelStatus() {
    static uint32_t lastColor = COLOR_OFF;
    static unsigned long lastBlinkTime = 0;
    static bool blinkState = false;
    static unsigned long lastDebugTime = 0;

    WiFiState wifiState = portal.getWiFiState();
    PortalState portalState = portal.getPortalState();

    // Debug logging every 2 seconds
    if (millis() - lastDebugTime > 2000) {
        ESP_LOGD(TAG, "🔍 WiFi:%s Portal:%s",
                     wifiStateToString(wifiState).c_str(),
                     portalStateToString(portalState).c_str());
        lastDebugTime = millis();
    }

    uint32_t targetColor = COLOR_OFF;
    bool shouldBlink = false;

    // Determine color based on current state (priority order matters)
    if (wifiState == WiFiState::CONNECTED) {
        targetColor = COLOR_CONNECTED;
    } else if (wifiState == WiFiState::CONNECTING) {
        targetColor = COLOR_CONNECTING;
        shouldBlink = true; // Blink yellow while connecting
    } else if (wifiState == WiFiState::FAILED) {
        targetColor = COLOR_ERROR;
        shouldBlink = true; // Blink red on error
    } else if (portalState == PortalState::ACTIVE) {
        targetColor = COLOR_PORTAL_ACTIVE; // Solid orange when portal is active
    } else if (portalState == PortalState::STARTING) {
        targetColor = COLOR_PORTAL_STARTING;
        shouldBlink = true; // Blink magenta while starting
    } else {
        // Check if we're scanning
        int scanStatus = WiFi.scanComplete();
        if (scanStatus == WIFI_SCAN_RUNNING) {
            targetColor = COLOR_SCANNING;
            shouldBlink = true; // Blink blue while scanning
        } else {
            // Default state - if portal is not active and not connected, show scanning color
            // This handles the case where auto-connect failed but portal hasn't started yet
            if (portalState == PortalState::STOPPED && wifiState == WiFiState::DISCONNECTED) {
                targetColor = COLOR_SCANNING; // Show blue when waiting/idle
                shouldBlink = true;
            } else {
                targetColor = COLOR_OFF;
            }
        }
    }

    // Handle blinking
    if (shouldBlink) {
        if (millis() - lastBlinkTime > 500) { // Blink every 500ms
            blinkState = !blinkState;
            lastBlinkTime = millis();

            String colorName = "UNKNOWN";
            if (targetColor == COLOR_CONNECTING) colorName = "YELLOW";
            else if (targetColor == COLOR_PORTAL_STARTING) colorName = "MAGENTA";
            else if (targetColor == COLOR_SCANNING) colorName = "BLUE";
            else if (targetColor == COLOR_ERROR) colorName = "RED";

            if (blinkState) {
                setNeoPixelColor(targetColor, true);
                ESP_LOGD(TAG, "💡 %s ON", colorName.c_str());
            } else {
                setNeoPixelColor(COLOR_OFF, true);
                ESP_LOGD(TAG, "💡 %s OFF", colorName.c_str());
            }
        }
    } else {
        // Solid color
        if (targetColor != lastColor) {
            setNeoPixelColor(targetColor, true);

            // Debug color changes
            String colorName = "UNKNOWN";
            if (targetColor == COLOR_OFF) colorName = "OFF";
            else if (targetColor == COLOR_CONNECTED) colorName = "GREEN";
            else if (targetColor == COLOR_CONNECTING) colorName = "YELLOW";
            else if (targetColor == COLOR_PORTAL_ACTIVE) colorName = "ORANGE";
            else if (targetColor == COLOR_PORTAL_STARTING) colorName = "MAGENTA";
            else if (targetColor == COLOR_SCANNING) colorName = "BLUE";
            else if (targetColor == COLOR_ERROR) colorName = "RED";

            ESP_LOGI(TAG, "💡 LED→%s (W:%s P:%s)",
                         colorName.c_str(),
                         wifiStateToString(wifiState).c_str(),
                         portalStateToString(portalState).c_str());

            lastColor = targetColor;
        }
    }
}
