/**
 * ╔══════════════════════════════════════════════════════════════╗
 * ║     IoT Smart Agriculture – ESP32 Firmware v2.4               ║
 * ║  DHT22 · Water Level · Rain · Touch · 2× Servo · Buzzer · OLED
 * ║  WiFiManager portal + Preferences + Min/Max + Fast reconnect    ║
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

// ======================= WiFiManager + Preferences =======================
#include <WiFiManager.h>
#include <Preferences.h>

Preferences prefs;
bool shouldSaveConfig = false;
WiFiManager wm;
// ========================================================================

// Server
String serverHost = "https://socstudentmusicforlife.com/fish_agriculture";
String deviceID   = "esp32_01";

// GPIO Pin Map
#define PIN_SERVO_LID          4
#define PIN_SERVO_DRAIN       16
#define PIN_WATER_LEVEL_POWER 14
#define PIN_WATER_LEVEL_SIGNAL 32
#define PIN_RAIN_POWER        33
#define PIN_RAIN_ANALOG       34
#define PIN_DHT               19
#define PIN_OLED_SDA          21
#define PIN_OLED_SCL          22
#define PIN_BUZZER             5
#define PIN_TOUCH             17

#define DHT_TYPE DHT22

// Water Level Calibration
#define SENSOR_RANGE_CM      4
#define SENSOR_TOP_CM        28
#define SENSOR_BOTTOM_CM     26.0
#define WATER_THRESHOLD_CM   26.5

// Rain Sensor Constants
#define RAIN_WET_THRESHOLD   1500
#define RAIN_DRY_THRESHOLD   3500
#define RAIN_INVALID_MAX     100
#define RAIN_DEBOUNCE_MS     500

// Servo and Buzzer Configuration
#define SERVO_FREQUENCY   50
#define SERVO_RESOLUTION  16
#define BUZZER_FREQUENCY  2000
#define BUZZER_RESOLUTION  8

// Servo Positions
#define LID_CLOSED_ANGLE      0
#define LID_OPEN_ANGLE      180
#define DRAIN_CLOSED_ANGLE    0
#define DRAIN_OPEN_ANGLE    180

// Touch Debounce
#define TOUCH_COOLDOWN_MS  3000

// Objects
DHT dhtSensor(PIN_DHT, DHT_TYPE);
Adafruit_SSD1306 oledScreen(128, 64, &Wire, -1);

// Timing
unsigned long previousMillis      = 0;
unsigned long lastSettingsFetch   = 0;
unsigned long lastSensorReadTime  = 0;
unsigned long oledMessageTimeout  = 0;
unsigned long lastTouchToggleTime  = 0;
int           uploadInterval      = 10;

// Sensor Values
float temperature = NAN;
float humidity    = NAN;
int   waterLevel  = 0;
float waterHeightCm = 0.0;
int   rainAnalogValue = 4095;
bool  isRaining   = false;
bool  isTouched   = false;

// Min/Max Tracking
float minTemp = NAN, maxTemp = NAN;
float minHum  = NAN, maxHum  = NAN;
bool  statsReady = false;

// Settings (synced from server)
float tempThreshold     = 31.0;
float humidityThreshold = 70.0;
int   waterThreshold    = 500;
bool  buzzerEnabled     = true;
bool  drainOpenOverride = false;
bool  lidOpenOverride   = false;

// Manual / State Flags
bool lidManualOpen      = false;
bool lastRainState      = false;
bool rainBuzzerFired    = false;
bool isOnlineMode       = false;

// Servo Tracking
int lidServoPosition   = LID_CLOSED_ANGLE;
int drainServoPosition = DRAIN_CLOSED_ANGLE;

// Non-blocking Buzzer Queue
struct BuzzPattern {
    uint32_t toneFrequency;
    uint16_t beepDuration;
    uint16_t pauseDuration;
    uint8_t  repeatCount;
};
BuzzPattern buzzQueue[4];
uint8_t buzzQueueHead     = 0;
uint8_t buzzQueueCount    = 0;
uint8_t buzzRepeatCounter = 0;
bool    buzzerIsOn        = false;
unsigned long buzzerNextAction = 0;

// ──────────────────────────────────────────────
// Servo Helpers
// ──────────────────────────────────────────────
uint32_t angleToDuty(int angle) {
    uint32_t pulseMicros = 1000 + ((uint32_t)(2000 - 1000) * angle / 180);
    return (uint32_t)(pulseMicros * 65535UL / 20000UL);
}
void openLid()   { lidServoPosition = LID_OPEN_ANGLE;   ledcWrite(PIN_SERVO_LID,   angleToDuty(lidServoPosition)); }
void closeLid()  { lidServoPosition = LID_CLOSED_ANGLE; ledcWrite(PIN_SERVO_LID,   angleToDuty(lidServoPosition)); }
void openDrain() { drainServoPosition = DRAIN_OPEN_ANGLE;   ledcWrite(PIN_SERVO_DRAIN, angleToDuty(drainServoPosition)); }
void closeDrain(){ drainServoPosition = DRAIN_CLOSED_ANGLE; ledcWrite(PIN_SERVO_DRAIN, angleToDuty(drainServoPosition)); }
bool isLidOpen()   { return lidServoPosition == LID_OPEN_ANGLE; }
bool isDrainOpen() { return drainServoPosition == DRAIN_OPEN_ANGLE; }

// ──────────────────────────────────────────────
// Buzzer Helpers
// ──────────────────────────────────────────────
void buzzerTone(uint32_t freq) {
    if (freq == 0) ledcWrite(PIN_BUZZER, 0);
    else           ledcWriteTone(PIN_BUZZER, freq);
}
void queueBuzzer(uint32_t freq, uint16_t onMs, uint16_t offMs, uint8_t repeats) {
    if (!buzzerEnabled || buzzQueueCount >= 4) return;
    buzzQueue[(buzzQueueHead + buzzQueueCount) % 4] = { freq, onMs, offMs, repeats };
    buzzQueueCount++;
}
void updateBuzzer() {
    if (buzzQueueCount == 0) return;
    if (millis() < buzzerNextAction) return;
    BuzzPattern& current = buzzQueue[buzzQueueHead];
    if (!buzzerIsOn) {
        buzzerTone(current.toneFrequency);
        buzzerIsOn = true;
        buzzerNextAction = millis() + current.beepDuration;
    } else {
        buzzerTone(0);
        buzzerIsOn = false;
        buzzRepeatCounter++;
        if (buzzRepeatCounter >= current.repeatCount) {
            buzzRepeatCounter = 0;
            buzzQueueHead = (buzzQueueHead + 1) % 4;
            buzzQueueCount--;
        } else {
            buzzerNextAction = millis() + current.pauseDuration;
        }
    }
}

// ──────────────────────────────────────────────
// OLED Helpers
// ──────────────────────────────────────────────
void showStartupScreen() {
    oledScreen.clearDisplay();
    oledScreen.setTextColor(WHITE);
    oledScreen.setTextSize(1);
    oledScreen.setCursor(14, 10);
    oledScreen.println("SmartAgriculture");
    oledScreen.setCursor(32, 26);
    oledScreen.println("Starting...");
    oledScreen.display();
}
void showTemporaryMessage(String message, unsigned long durationMs) {
    oledMessageTimeout = millis() + durationMs;
    oledScreen.clearDisplay();
    oledScreen.setTextColor(WHITE);
    oledScreen.setTextSize(1);
    oledScreen.setCursor(0, 0);
    oledScreen.println(message);
    oledScreen.display();
}
void refreshOledScreen() {
    if (millis() < oledMessageTimeout) return;
    oledScreen.clearDisplay();
    oledScreen.setTextColor(WHITE);
    oledScreen.setTextSize(1);
    oledScreen.setCursor(0,  0); oledScreen.printf("RoomT:%.1fC",    temperature);
    oledScreen.setCursor(0, 12); oledScreen.printf("RoomH:%.1f%%",   humidity);
    oledScreen.setCursor(0, 24); oledScreen.printf("Water:%.1fcm",   waterHeightCm);
    oledScreen.setCursor(0, 36); oledScreen.printf("Rain:%s",        isRaining ? "YES" : "no");
    oledScreen.setCursor(0, 48); oledScreen.printf("Lid:%s  Drain:%s",
        isLidOpen()   ? "OPEN" : "CLSD",
        isDrainOpen() ? "OPEN" : "CLSD");
    oledScreen.setCursor(0, 56);
    oledScreen.print(isOnlineMode ? "Mode:ONLINE" : "Mode:OFFLINE");
    oledScreen.display();
}

// ──────────────────────────────────────────────
// WiFiManager Callback
// ──────────────────────────────────────────────
void saveConfigCallback() {
    Serial.println("WiFiManager: Config should be saved");
    shouldSaveConfig = true;
}

// ──────────────────────────────────────────────
// WiFi (WiFiManager + Preferences)
// ──────────────────────────────────────────────
void connectWiFi() {
    // 1. Try saved credentials from Preferences
    prefs.begin("smartagriculture", false);
    String savedSSID = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");
    prefs.end();

    if (savedSSID.length() > 0) {
        Serial.print("Loaded saved WiFi: "); Serial.println(savedSSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        int retry = 0;
        // ═══════════════════════════════════════════════════════
        //  delay(250) for faster boot
        // ═══════════════════════════════════════════════════════
        while (WiFi.status() != WL_CONNECTED && retry < 30) {
            delay(250);
            Serial.print(".");
            retry++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            isOnlineMode = true;
            Serial.println("\nWiFi connected (saved creds).");
            Serial.print("IP: "); Serial.println(WiFi.localIP());
            showTemporaryMessage("ONLINE\n" + WiFi.localIP().toString(), 3000);
            return;
        }
        Serial.println("\nSaved WiFi failed. Starting portal...");
    }

    // 2. Start captive portal for configuration
    Serial.println("Starting WiFiManager captive portal...");
    showTemporaryMessage("AP Mode\nConnect to setup", 3000);

    wm.setConfigPortalTimeout(180);
    wm.setAPCallback([](WiFiManager* myWiFiManager) {
        Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
        showTemporaryMessage("AP: SmartAgriculture\nIP: 192.168.4.1", 5000);
    });
    wm.setSaveConfigCallback(saveConfigCallback);

    WiFiManagerParameter custom_device_id("device_id", "Device ID", deviceID.c_str(), 20);
    wm.addParameter(&custom_device_id);

    bool res = wm.autoConnect("SmartAgriculture-Setup");

    if (!res) {
        Serial.println("Failed to connect. Rebooting...");
        showTemporaryMessage("Setup failed\nRebooting...", 3000);
        delay(3000);
        ESP.restart();
    }

    // 3. Connected via portal
    isOnlineMode = true;
    Serial.println("WiFi connected via portal.");
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    // 4. Save new credentials to Preferences
    if (shouldSaveConfig) {
        prefs.begin("smartagriculture", false);
        prefs.putString("ssid", WiFi.SSID());
        prefs.putString("pass", WiFi.psk());
        prefs.putString("device_id", custom_device_id.getValue());
        prefs.end();
        Serial.println("Config saved to flash.");
        deviceID = String(custom_device_id.getValue());
        shouldSaveConfig = false;
    }

    showTemporaryMessage("ONLINE\n" + WiFi.localIP().toString(), 3000);
}

// ──────────────────────────────────────────────
// setup()
// ──────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nESP32 Smart Agriculture v2.4");
    Serial.println("-----------------------------");

    ledcAttach(PIN_BUZZER, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
    buzzerTone(0);

    ledcAttach(PIN_SERVO_LID,   SERVO_FREQUENCY, SERVO_RESOLUTION);
    ledcAttach(PIN_SERVO_DRAIN, SERVO_FREQUENCY, SERVO_RESOLUTION);
    closeLid();
    closeDrain();

    pinMode(PIN_WATER_LEVEL_POWER, OUTPUT); digitalWrite(PIN_WATER_LEVEL_POWER, LOW);
    pinMode(PIN_RAIN_POWER,        OUTPUT); digitalWrite(PIN_RAIN_POWER,        LOW);
    pinMode(PIN_TOUCH, INPUT);
    dhtSensor.begin();

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!oledScreen.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed");
        while (true) delay(100);
    }
    showStartupScreen();
    connectWiFi();
}

// ═══════════════════════════════════════════════════════════════════════
// ║  loop() — FIXED: Non-blocking WiFi reconnect for fast response    ║
// ═══════════════════════════════════════════════════════════════════════
void loop() {
    unsigned long now = millis();

    // ═══════════════════════════════════════════════════════
    // WiFi Management: NON-BLOCKING reconnect
    // ═══════════════════════════════════════════════════════
    static unsigned long lastReconnectAttempt = 0;
    static bool wasOffline = false;

    if (WiFi.status() != WL_CONNECTED) {
        // Try to reconnect every 10 seconds — but DON'T block
        if (now - lastReconnectAttempt > 10000) {
            lastReconnectAttempt = now;

            if (isOnlineMode) {
                Serial.println("WiFi LOST. Switching to offline...");
                isOnlineMode = false;
                wasOffline = true;
                showTemporaryMessage("OFFLINE\nWiFi lost", 3000);
            } else if (wasOffline) {
                Serial.println("Still offline. Retrying...");
            }

            // Start reconnect (non-blocking — returns immediately)
            if (WiFi.getMode() != WIFI_STA) {
                WiFi.mode(WIFI_STA);
            }
            WiFi.reconnect();
        }

        // Check if reconnected (without blocking)
        if (WiFi.status() == WL_CONNECTED) {
            isOnlineMode = true;
            wasOffline = false;
            Serial.println("WiFi RECONNECTED!");
            Serial.print("IP: "); Serial.println(WiFi.localIP());
            showTemporaryMessage("ONLINE\n" + WiFi.localIP().toString(), 3000);
            previousMillis = now;
            lastSettingsFetch = now;
        }
    }

    // ═══════════════════════════════════════════════════════
    // Sensor reading & local control (works in BOTH modes)
    // ═══════════════════════════════════════════════════════
    if (now - lastSensorReadTime >= 1000) {
        lastSensorReadTime = now;
        readAllSensors();
        runDecisionLogic();
    }

    // ═══════════════════════════════════════════════════════
    // Online-only tasks
    // ═══════════════════════════════════════════════════════
    if (isOnlineMode) {
        if (now - previousMillis >= (unsigned long)uploadInterval * 1000) {
            previousMillis = now;
            sendSensorData();
        }

        if (now - lastSettingsFetch >= 2000) {
            lastSettingsFetch = now;
            fetchServerSettings();
        }
    }

    // ═══════════════════════════════════════════════════════
    // Always-running tasks
    // ═══════════════════════════════════════════════════════
    updateBuzzer();

    static unsigned long lastScreenUpdate = 0;
    if (now - lastScreenUpdate >= 1000) {
        lastScreenUpdate = now;
        refreshOledScreen();
    }
}

// ──────────────────────────────────────────────
// Read Sensors
// ──────────────────────────────────────────────
void readAllSensors() {
    float t = dhtSensor.readTemperature();
    float h = dhtSensor.readHumidity();

    // Update values and track min/max
    if (!isnan(t)) {
        temperature = t;
        if (!statsReady || isnan(minTemp) || t < minTemp) minTemp = t;
        if (!statsReady || isnan(maxTemp) || t > maxTemp) maxTemp = t;
    }
    if (!isnan(h)) {
        humidity = h;
        if (!statsReady || isnan(minHum) || h < minHum) minHum = h;
        if (!statsReady || isnan(maxHum) || h > maxHum) maxHum = h;
    }
    if (!isnan(t) || !isnan(h)) statsReady = true;

    digitalWrite(PIN_WATER_LEVEL_POWER, HIGH);
    delay(200);
    waterLevel = analogRead(PIN_WATER_LEVEL_SIGNAL);
    digitalWrite(PIN_WATER_LEVEL_POWER, LOW);

    waterHeightCm = SENSOR_BOTTOM_CM + ((float)waterLevel / 4095.0) * SENSOR_RANGE_CM;

    digitalWrite(PIN_RAIN_POWER, HIGH);
    delay(5);
    int rawAnalog = analogRead(PIN_RAIN_ANALOG);
    digitalWrite(PIN_RAIN_POWER, LOW);

    if (rawAnalog <= RAIN_INVALID_MAX) {
        rainAnalogValue = 4095;
        isRaining = false;
    } else {
        rainAnalogValue = rawAnalog;
        bool rawRain = (rainAnalogValue < RAIN_WET_THRESHOLD);
        static unsigned long rainSince = 0;
        static unsigned long drySince  = 0;
        if (rawRain) {
            drySince = 0;
            if (rainSince == 0) rainSince = millis();
            else if (millis() - rainSince >= RAIN_DEBOUNCE_MS) isRaining = true;
        } else {
            rainSince = 0;
            if (drySince == 0) drySince = millis();
            else if (millis() - drySince >= 10000) isRaining = false;
        }
    }

    isTouched = (digitalRead(PIN_TOUCH) == HIGH);

    Serial.printf("RoomT:%.1fC  RoomH:%.1f%%  Water:%.1fcm (raw:%d)  Rain:%s  Touch:%s\n",
                  temperature, humidity, waterHeightCm, waterLevel,
                  isRaining ? "YES" : "No", isTouched ? "YES" : "No");
}

// ──────────────────────────────────────────────
// Decision Logic
// ──────────────────────────────────────────────
void runDecisionLogic() {
    // Drain cap
    bool waterTooHigh = (waterHeightCm > WATER_THRESHOLD_CM);
    bool shouldOpenDrain = waterTooHigh || drainOpenOverride;
    if (shouldOpenDrain && !isDrainOpen()) {
        openDrain();
        if (!drainOpenOverride) {
            showTemporaryMessage("Water HIGH\nDraining...", 3000);
            queueBuzzer(3000, 200, 150, 3);
        }
        Serial.println("Drain OPENED.");
    } else if (!shouldOpenDrain && isDrainOpen()) {
        closeDrain();
        Serial.println("Drain CLOSED.");
    }

    // Touch handling (with long-press WiFi reset)
    static bool lastTouchRaw = false;
    static unsigned long touchHoldStart = 0;
    bool touchRaw = isTouched;
    bool resetTriggered = false;

    if (touchRaw) {
        if (touchHoldStart == 0) touchHoldStart = millis();
        else if (millis() - touchHoldStart > 5000) {
            resetTriggered = true;
            showTemporaryMessage("Resetting WiFi...", 3000);
            queueBuzzer(2000, 300, 200, 3);
            delay(1000);
            wm.resetSettings();
            prefs.begin("smartagriculture", false);
            prefs.clear();
            prefs.end();
            ESP.restart();
        }
    } else {
        touchHoldStart = 0;
    }

    if (!resetTriggered && touchRaw && !lastTouchRaw) {
        if (millis() - lastTouchToggleTime > TOUCH_COOLDOWN_MS) {
            lidManualOpen = !lidManualOpen;
            lastTouchToggleTime = millis();
            if (lidManualOpen) {
                showTemporaryMessage("Container\nOPEN (Manual)", 2000);
                queueBuzzer(1200, 150, 100, 1);
                Serial.println("Touch: lid MANUAL OPEN.");
            } else {
                showTemporaryMessage("Container\nCLOSED (Manual)", 2000);
                queueBuzzer(800, 150, 100, 1);
                Serial.println("Touch: lid MANUAL CLOSE.");
            }
        }
    }
    lastTouchRaw = touchRaw;

    // Rain buzzer
    if (isRaining && !lastRainState) {
        if (!rainBuzzerFired) {
            queueBuzzer(2500, 400, 100, 1);
            rainBuzzerFired = true;
            showTemporaryMessage("Rain detected!\nOpening lid", 3000);
            Serial.println("Rain STARTED → buzz once + open lid.");
        }
    }
    if (!isRaining && lastRainState) {
        rainBuzzerFired = false;
        showTemporaryMessage("Rain stopped", 2000);
        Serial.println("Rain STOPPED → reset buzzer arm.");
    }
    lastRainState = isRaining;

    // Environmental lid control
    bool tooHumid = (!isnan(humidity) && humidity > humidityThreshold);
    bool tooHot   = (!isnan(temperature) && temperature > tempThreshold);

    bool desiredOpen;
    if (lidOpenOverride) {
        desiredOpen = true;
    } else if (isRaining) {
        desiredOpen = true;
    } else if (tooHot) {
        desiredOpen = true;
    } else if (lidManualOpen) {
        desiredOpen = true;
    } else {
        desiredOpen = false;
    }

    if (desiredOpen && !isLidOpen()) {
        openLid();
        Serial.println("Lid OPENED.");
    } else if (!desiredOpen && isLidOpen()) {
        closeLid();
        Serial.println("Lid CLOSED.");
    }
}

// ──────────────────────────────────────────────
// Send Data to Server
// ──────────────────────────────────────────────
void sendSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Skip upload.");
        return;
    }

    bool tooHumid = (!isnan(humidity) && humidity > humidityThreshold);
    bool tooHot   = (!isnan(temperature) && temperature > tempThreshold);

    String status = "Normal";
    if (waterHeightCm > WATER_THRESHOLD_CM) {
        status = "Danger";
    } else if (isRaining || tooHot || tooHumid) {
        status = "Warning";
    }

    String url = serverHost + "/php/api_post_data.php";
    Serial.print("Upload → "); Serial.println(url);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"device_id\":\"" + deviceID + "\",";
    json += "\"temperature\":" + String(temperature, 1) + ",";
    json += "\"humidity\":"    + String(humidity, 1)    + ",";
    json += "\"water_level\":" + String(waterLevel)     + ",";
    json += "\"water_drop\":"  + String(isRaining ? 1 : 0) + ",";
    if (!isnan(minTemp)) {
        json += "\"min_temp\":" + String(minTemp, 1) + ",";
        json += "\"max_temp\":" + String(maxTemp, 1) + ",";
    }
    if (!isnan(minHum)) {
        json += "\"min_hum\":" + String(minHum, 1) + ",";
        json += "\"max_hum\":" + String(maxHum, 1) + ",";
    }
    json += "\"status\":\""    + status + "\"";
    json += "}";

    int code = http.POST(json);
    Serial.printf("HTTP %d, Water: %.1f cm (raw:%d)\n", code, waterHeightCm, waterLevel);
    http.end();
}

// ──────────────────────────────────────────────
// Fetch Settings from Server
// ──────────────────────────────────────────────
void fetchServerSettings() {
    if (WiFi.status() != WL_CONNECTED) return;

    String url = serverHost + "/php/api_get_settings.php?device_id=" + deviceID;
    Serial.print("Sync settings → "); Serial.println(url);

    HTTPClient http;
    http.begin(url);

    int code = http.GET();
    if (code > 0) {
        String response = http.getString();
        tempThreshold     = extractFloat(response, "temp_threshold",        tempThreshold);
        humidityThreshold = extractFloat(response, "humidity_threshold",    humidityThreshold);
        waterThreshold    = extractInt  (response, "water_level_threshold", waterThreshold);
        uploadInterval    = extractInt  (response, "upload_interval",       uploadInterval);
        buzzerEnabled     = extractInt  (response, "buzzer_enabled",        buzzerEnabled ? 1 : 0) == 1;
        drainOpenOverride = extractInt  (response, "servo2_open",           drainOpenOverride ? 1 : 0) == 1;
        lidOpenOverride   = extractInt  (response, "servo1_open",           lidOpenOverride   ? 1 : 0) == 1;

        Serial.println("Settings synced:");
        Serial.printf("  TempThr:%.1f  HumThr:%.1f  WaterThr:%d  Int:%d  Buzzer:%s  LidOv:%s  DrainOv:%s\n",
                      tempThreshold, humidityThreshold, waterThreshold, uploadInterval,
                      buzzerEnabled ? "ON" : "OFF",
                      lidOpenOverride ? "ON" : "OFF",
                      drainOpenOverride ? "ON" : "OFF");
    } else {
        Serial.printf("Settings fetch failed: %d\n", code);
    }
    http.end();
}

// ──────────────────────────────────────────────
// JSON Extraction Helpers
// ──────────────────────────────────────────────
String extractValue(const String& json, const String& key) {
    String search = "\"" + key + "\":";
    int idx = json.indexOf(search);
    if (idx == -1) return "";
    idx += search.length();
    while (idx < (int)json.length() && json[idx] == ' ') idx++;
    int end = idx;
    while (end < (int)json.length() && json[end] != ',' && json[end] != '}') end++;
    String val = json.substring(idx, end);
    val.trim();
    if (val.startsWith("\"") && val.endsWith("\""))
        val = val.substring(1, val.length() - 1);
    return val;
}
float extractFloat(const String& json, const String& key, float defaultVal) {
    String v = extractValue(json, key);
    return v.length() ? v.toFloat() : defaultVal;
}
int extractInt(const String& json, const String& key, int defaultVal) {
    String v = extractValue(json, key);
    return v.length() ? v.toInt() : defaultVal;
}