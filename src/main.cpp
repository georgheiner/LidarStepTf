/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  LiDAR Room Scanner – ESP32 firmware
 *
 *  Hardware
 *  ─────────────────────────────────────────────────────────────────────────
 *  ESP32 DevKit V1 (30-pin)
 *
 *  TFMini (UART, 115200 baud, 9-byte frames)
 *    TFMini TX  →  GPIO16  (ESP32 UART2 RX)
 *    TFMini RX  →  GPIO17  (ESP32 UART2 TX)   ← optional, needed only for
 *                                                 firmware config commands
 *
 *  Generic step/dir driver wiring
 *    STEP  →  GPIO25
 *    DIR   →  GPIO26
 *    EN    →  GPIO27   (polarity configurable in app_config.h)
 *
 *  Runtime-adjustable values are stored in ESP32 NVS and can be changed from
 *  the browser UI without reflashing: pulley teeth, microsteps, scan speed,
 *  and smoothing radius.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FastAccelStepper.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <math.h>
#include "app_config.h"
#include "webpage.h"

static_assert(AppConfig::DRIVE_PULLEY_TEETH > 0, "Drive pulley teeth must be > 0");
static_assert(AppConfig::LIDAR_PULLEY_TEETH > 0, "LiDAR pulley teeth must be > 0");
static_assert(AppConfig::SCAN_BINS > 0, "SCAN_BINS must be > 0");

static constexpr int BINS = AppConfig::SCAN_BINS;

struct RuntimeSettings {
    uint16_t motorMicrosteps;
    uint16_t drivePulleyTeeth;
    uint16_t lidarPulleyTeeth;
    float lidarRevsPerSec;
    uint8_t smoothingRadius;
    bool mqttEnabled;
    uint16_t mqttPort;
    char mqttHost[AppConfig::MQTT_HOST_MAX_LEN];
    char mqttBaseTopic[AppConfig::MQTT_TOPIC_MAX_LEN];
    char mqttUsername[AppConfig::MQTT_USERNAME_MAX_LEN];
    char mqttPassword[AppConfig::MQTT_PASSWORD_MAX_LEN];
};

struct DerivedSettings {
    int32_t motorStepsPerRev;
    int32_t stepsPerLidarRev;
    float motorRevsPerLidarRev;
    uint32_t scanSpeedHz;
    uint32_t scanAccelHz2;
    float sweepDegPerSec;
};

struct ScanFrame {
    uint16_t dist[BINS];
    uint16_t strength[BINS];
};

struct TargetCluster {
    float angleDeg;
    float distanceCm;
    float spanDeg;
    uint16_t points;
    uint16_t motionCm;
};

static RuntimeSettings runtimeSettings{};
static DerivedSettings derivedSettings{};
static Preferences preferences;

static ScanFrame frames[2];
static uint8_t writeFr = 0;
static uint8_t readFr = 1;
static volatile bool newFrameReady = false;

static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper* stepper = nullptr;
static WiFiClient mqttWiFiClient;
static PubSubClient mqttClient(mqttWiFiClient);

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static float currentAngle = 0.0f;
static int32_t lastStepPos = 0;
static int32_t accumSteps = 0;
static uint32_t lastAngleSendMs = 0;

static uint16_t tfDist = 0;
static uint16_t tfStrength = 0;
static uint32_t lastMqttConnectAttemptMs = 0;
static uint32_t lastMqttStatusMs = 0;
static uint32_t scanSequence = 0;
static bool lastReportedMqttConnected = false;
static uint16_t lastFilteredScan[BINS] = {0};
static bool hasLastFilteredScan = false;

static void sendConfig(AsyncWebSocketClient* client = nullptr);

static inline int32_t abs32(int32_t value) {
    return (value < 0) ? -value : value;
}

static inline uint16_t clampU16(uint16_t value, uint16_t minValue, uint16_t maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline uint8_t clampU8(uint8_t value, uint8_t minValue, uint8_t maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline float clampFloat(float value, float minValue, float maxValue) {
    if (!isfinite(value)) return minValue;
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static void copyCString(char* dest, size_t destSize, const String& source) {
    if (destSize == 0) {
        return;
    }
    source.substring(0, destSize - 1).toCharArray(dest, destSize);
    dest[destSize - 1] = '\0';
}

static void copyCString(char* dest, size_t destSize, const char* source) {
    copyCString(dest, destSize, String(source ? source : ""));
}

static void deriveRuntimeSettings() {
    derivedSettings.motorStepsPerRev =
        static_cast<int32_t>(AppConfig::MOTOR_FULL_STEPS_PER_REV) *
        static_cast<int32_t>(runtimeSettings.motorMicrosteps);

    derivedSettings.stepsPerLidarRev =
        (derivedSettings.motorStepsPerRev * static_cast<int32_t>(runtimeSettings.lidarPulleyTeeth) +
         (static_cast<int32_t>(runtimeSettings.drivePulleyTeeth) / 2)) /
        static_cast<int32_t>(runtimeSettings.drivePulleyTeeth);

    if (derivedSettings.stepsPerLidarRev < 1) {
        derivedSettings.stepsPerLidarRev = 1;
    }

    derivedSettings.motorRevsPerLidarRev =
        static_cast<float>(runtimeSettings.lidarPulleyTeeth) /
        static_cast<float>(runtimeSettings.drivePulleyTeeth);

    derivedSettings.scanSpeedHz = static_cast<uint32_t>(
        (static_cast<float>(derivedSettings.stepsPerLidarRev) * runtimeSettings.lidarRevsPerSec) + 0.5f);
    if (derivedSettings.scanSpeedHz < 1) {
        derivedSettings.scanSpeedHz = 1;
    }

    const float accelTime = (AppConfig::STEPPER_ACCEL_TIME_SEC > 0.05f)
        ? AppConfig::STEPPER_ACCEL_TIME_SEC
        : 0.05f;
    derivedSettings.scanAccelHz2 = static_cast<uint32_t>(
        (static_cast<float>(derivedSettings.scanSpeedHz) / accelTime) + 0.5f);
    if (derivedSettings.scanAccelHz2 < 1) {
        derivedSettings.scanAccelHz2 = 1;
    }

    derivedSettings.sweepDegPerSec =
        (AppConfig::STEPPER_ROTATE_FORWARD ? 1.0f : -1.0f) * runtimeSettings.lidarRevsPerSec * 360.0f;
}

static void loadRuntimeSettings() {
    runtimeSettings.motorMicrosteps = AppConfig::MOTOR_MICROSTEPS;
    runtimeSettings.drivePulleyTeeth = AppConfig::DRIVE_PULLEY_TEETH;
    runtimeSettings.lidarPulleyTeeth = AppConfig::LIDAR_PULLEY_TEETH;
    runtimeSettings.lidarRevsPerSec = AppConfig::LIDAR_REVS_PER_SEC;
    runtimeSettings.smoothingRadius = AppConfig::SMOOTHING_RADIUS;
    runtimeSettings.mqttEnabled = AppConfig::MQTT_ENABLED_DEFAULT;
    runtimeSettings.mqttPort = AppConfig::MQTT_PORT_DEFAULT;
    copyCString(runtimeSettings.mqttHost, sizeof(runtimeSettings.mqttHost), AppConfig::MQTT_HOST_DEFAULT);
    copyCString(runtimeSettings.mqttBaseTopic, sizeof(runtimeSettings.mqttBaseTopic), AppConfig::MQTT_BASE_TOPIC_DEFAULT);
    copyCString(runtimeSettings.mqttUsername, sizeof(runtimeSettings.mqttUsername), AppConfig::MQTT_USERNAME_DEFAULT);
    copyCString(runtimeSettings.mqttPassword, sizeof(runtimeSettings.mqttPassword), AppConfig::MQTT_PASSWORD_DEFAULT);

    preferences.begin(AppConfig::PREF_NAMESPACE, false);
    runtimeSettings.motorMicrosteps = clampU16(
        preferences.getUShort("micro", runtimeSettings.motorMicrosteps),
        AppConfig::MOTOR_MICROSTEPS_MIN,
        AppConfig::MOTOR_MICROSTEPS_MAX);
    runtimeSettings.drivePulleyTeeth = clampU16(
        preferences.getUShort("driveT", runtimeSettings.drivePulleyTeeth),
        AppConfig::PULLEY_TEETH_MIN,
        AppConfig::PULLEY_TEETH_MAX);
    runtimeSettings.lidarPulleyTeeth = clampU16(
        preferences.getUShort("lidarT", runtimeSettings.lidarPulleyTeeth),
        AppConfig::PULLEY_TEETH_MIN,
        AppConfig::PULLEY_TEETH_MAX);
    runtimeSettings.lidarRevsPerSec = clampFloat(
        preferences.getFloat("lidarRps", runtimeSettings.lidarRevsPerSec),
        AppConfig::LIDAR_REVS_PER_SEC_MIN,
        AppConfig::LIDAR_REVS_PER_SEC_MAX);
    runtimeSettings.smoothingRadius = clampU8(
        preferences.getUChar("smooth", runtimeSettings.smoothingRadius),
        AppConfig::SMOOTHING_RADIUS_MIN,
        AppConfig::SMOOTHING_RADIUS_MAX);
    runtimeSettings.mqttEnabled = preferences.getBool("mqttOn", runtimeSettings.mqttEnabled);
    runtimeSettings.mqttPort = clampU16(
        preferences.getUShort("mqttPort", runtimeSettings.mqttPort),
        AppConfig::MQTT_PORT_MIN,
        AppConfig::MQTT_PORT_MAX);
    copyCString(runtimeSettings.mqttHost, sizeof(runtimeSettings.mqttHost),
                preferences.getString("mqttHost", runtimeSettings.mqttHost));
    copyCString(runtimeSettings.mqttBaseTopic, sizeof(runtimeSettings.mqttBaseTopic),
                preferences.getString("mqttBase", runtimeSettings.mqttBaseTopic));
    copyCString(runtimeSettings.mqttUsername, sizeof(runtimeSettings.mqttUsername),
                preferences.getString("mqttUser", runtimeSettings.mqttUsername));
    copyCString(runtimeSettings.mqttPassword, sizeof(runtimeSettings.mqttPassword),
                preferences.getString("mqttPass", runtimeSettings.mqttPassword));

    deriveRuntimeSettings();
}

static void persistRuntimeSettings() {
    preferences.putUShort("micro", runtimeSettings.motorMicrosteps);
    preferences.putUShort("driveT", runtimeSettings.drivePulleyTeeth);
    preferences.putUShort("lidarT", runtimeSettings.lidarPulleyTeeth);
    preferences.putFloat("lidarRps", runtimeSettings.lidarRevsPerSec);
    preferences.putUChar("smooth", runtimeSettings.smoothingRadius);
    preferences.putBool("mqttOn", runtimeSettings.mqttEnabled);
    preferences.putUShort("mqttPort", runtimeSettings.mqttPort);
    preferences.putString("mqttHost", runtimeSettings.mqttHost);
    preferences.putString("mqttBase", runtimeSettings.mqttBaseTopic);
    preferences.putString("mqttUser", runtimeSettings.mqttUsername);
    preferences.putString("mqttPass", runtimeSettings.mqttPassword);
}

static void setDriverEnabled(bool enabled) {
    if (!AppConfig::STEPPER_HAS_ENABLE_PIN || AppConfig::PIN_EN < 0) {
        return;
    }

    const bool pinHigh = AppConfig::STEPPER_ENABLE_ACTIVE_LOW ? !enabled : enabled;
    digitalWrite(AppConfig::PIN_EN, pinHigh ? HIGH : LOW);
}

static String buildConfigJson() {
    String json;
    json.reserve(640);
    json = F("{\"type\":\"config\",\"driver\":\"");
    json += AppConfig::STEPPER_DRIVER_LABEL;
    json += F("\",\"drivePulleyTeeth\":");
    json += runtimeSettings.drivePulleyTeeth;
    json += F(",\"lidarPulleyTeeth\":");
    json += runtimeSettings.lidarPulleyTeeth;
    json += F(",\"motorMicrosteps\":");
    json += runtimeSettings.motorMicrosteps;
    json += F(",\"gear\":\"");
    json += runtimeSettings.drivePulleyTeeth;
    json += ':';
    json += runtimeSettings.lidarPulleyTeeth;
    json += F("\",\"motorStepsPerRev\":");
    json += derivedSettings.motorStepsPerRev;
    json += F(",\"stepsPerLidarRev\":");
    json += derivedSettings.stepsPerLidarRev;
    json += F(",\"scanBins\":");
    json += BINS;
    json += F(",\"lidarRevPerSec\":");
    json += String(runtimeSettings.lidarRevsPerSec, 2);
    json += F(",\"sweepDegPerSec\":");
    json += String(derivedSettings.sweepDegPerSec, 1);
    json += F(",\"smoothingRadius\":");
    json += runtimeSettings.smoothingRadius;
    json += F(",\"mqttEnabled\":");
    json += runtimeSettings.mqttEnabled ? F("true") : F("false");
    json += F(",\"mqttConnected\":");
    json += mqttClient.connected() ? F("true") : F("false");
    json += F(",\"mqttHost\":\"");
    json += runtimeSettings.mqttHost;
    json += F("\",\"mqttPort\":");
    json += runtimeSettings.mqttPort;
    json += F(",\"mqttBaseTopic\":\"");
    json += runtimeSettings.mqttBaseTopic;
    json += F("\",\"mqttUsername\":\"");
    json += runtimeSettings.mqttUsername;
    json += F("\"");
    json += '}';
    return json;
}

static String mqttTopic(const char* suffix) {
    String topic = runtimeSettings.mqttBaseTopic;
    topic.trim();
    if (topic.endsWith("/")) {
        topic.remove(topic.length() - 1);
    }
    topic += '/';
    topic += suffix;
    return topic;
}

static bool mqttConfigured() {
    return runtimeSettings.mqttEnabled && runtimeSettings.mqttHost[0] != '\0' && runtimeSettings.mqttBaseTopic[0] != '\0';
}

static void mqttPublish(const String& topic, const String& payload, bool retained = false) {
    if (!mqttClient.connected()) {
        return;
    }
    mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

static String buildMqttStatusJson() {
    String json;
    json.reserve(256);
    json = F("{\"online\":true,\"uptime_ms\":");
    json += millis();
    json += F(",\"ip\":\"");
    json += WiFi.isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    json += F("\",\"rssi\":");
    json += WiFi.isConnected() ? WiFi.RSSI() : 0;
    json += F(",\"ws_clients\":");
    json += ws.count();
    json += F(",\"scan_hz\":");
    json += String(runtimeSettings.lidarRevsPerSec, 2);
    json += '}';
    return json;
}

static void publishMqttStatus() {
    mqttPublish(mqttTopic("status"), buildMqttStatusJson(), true);
}

static void publishMqttConfig() {
    mqttPublish(mqttTopic("config"), buildConfigJson(), true);
}

static void publishMqttAngle() {
    if (!mqttClient.connected()) {
        return;
    }
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"timestamp_ms\":%lu,\"angle_deg\":%.1f,\"sweep_deg_per_sec\":%.1f}",
             (unsigned long)millis(), currentAngle, derivedSettings.sweepDegPerSec);
    mqttClient.publish(mqttTopic("angle").c_str(), payload, false);
}

static uint8_t detectTargetClusters(const uint16_t* filtered, TargetCluster* targets, uint8_t maxTargets) {
    if (!hasLastFilteredScan || maxTargets == 0) {
        return 0;
    }

    bool changed[BINS] = {false};
    for (int i = 0; i < BINS; i++) {
        if (filtered[i] > 0 && lastFilteredScan[i] > 0 &&
            abs(static_cast<int>(filtered[i]) - static_cast<int>(lastFilteredScan[i])) >= AppConfig::TARGET_MOTION_THRESHOLD_CM) {
            changed[i] = true;
        }
    }

    uint8_t count = 0;
    int index = 0;
    while (index < BINS && count < maxTargets) {
        if (!changed[index]) {
            index++;
            continue;
        }

        const int start = index;
        uint32_t angleAccum = 0;
        uint32_t distanceAccum = 0;
        uint16_t points = 0;
        uint16_t maxMotion = 0;
        while (index < BINS && changed[index]) {
            const uint16_t motion = static_cast<uint16_t>(abs(static_cast<int>(filtered[index]) - static_cast<int>(lastFilteredScan[index])));
            angleAccum += static_cast<uint32_t>(index);
            distanceAccum += filtered[index];
            if (motion > maxMotion) {
                maxMotion = motion;
            }
            points++;
            index++;
        }

        if (points >= AppConfig::TARGET_MIN_CLUSTER_BINS) {
            TargetCluster& target = targets[count++];
            const int end = index - 1;
            target.angleDeg = (static_cast<float>(angleAccum) / points) * 360.0f / BINS;
            target.distanceCm = static_cast<float>(distanceAccum) / points;
            target.spanDeg = static_cast<float>(end - start + 1) * 360.0f / BINS;
            target.points = points;
            target.motionCm = maxMotion;
        }
    }

    return count;
}

static void publishMqttTargets(const uint16_t* filtered) {
    if (!mqttClient.connected()) {
        return;
    }

    TargetCluster targets[AppConfig::TARGET_MAX_COUNT];
    const uint8_t targetCount = detectTargetClusters(filtered, targets, AppConfig::TARGET_MAX_COUNT);

    String payload;
    payload.reserve(1024);
    payload = F("{\"timestamp_ms\":");
    payload += millis();
    payload += F(",\"count\":");
    payload += targetCount;
    payload += F(",\"targets\":[");
    for (uint8_t i = 0; i < targetCount; i++) {
        payload += F("{\"id\":");
        payload += i;
        payload += F(",\"angle_deg\":");
        payload += String(targets[i].angleDeg, 2);
        payload += F(",\"distance_cm\":");
        payload += String(targets[i].distanceCm, 1);
        payload += F(",\"span_deg\":");
        payload += String(targets[i].spanDeg, 2);
        payload += F(",\"points\":");
        payload += targets[i].points;
        payload += F(",\"motion_cm\":");
        payload += targets[i].motionCm;
        payload += '}';
        if (i + 1 < targetCount) {
            payload += ',';
        }
    }
    payload += F("]}");
    mqttPublish(mqttTopic("targets"), payload, false);
}

static void ensureMqttConnected() {
    if (!mqttConfigured() || !WiFi.isConnected()) {
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        return;
    }

    if (mqttClient.connected()) {
        return;
    }

    const uint32_t now = millis();
    if ((now - lastMqttConnectAttemptMs) < AppConfig::MQTT_RECONNECT_MS) {
        return;
    }
    lastMqttConnectAttemptMs = now;

    mqttClient.setServer(runtimeSettings.mqttHost, runtimeSettings.mqttPort);
    mqttClient.setBufferSize(AppConfig::MQTT_PACKET_SIZE);

    char clientId[AppConfig::MQTT_CLIENT_ID_MAX_LEN];
    snprintf(clientId, sizeof(clientId), "esp32-lidar-%08lx", (unsigned long)(ESP.getEfuseMac() & 0xffffffffUL));
    const String willTopic = mqttTopic("status");
    const char* user = runtimeSettings.mqttUsername[0] ? runtimeSettings.mqttUsername : nullptr;
    const char* pass = runtimeSettings.mqttPassword[0] ? runtimeSettings.mqttPassword : nullptr;

    const bool connected = mqttClient.connect(
        clientId,
        user,
        pass,
        willTopic.c_str(),
        1,
        true,
        "{\"online\":false}");

    if (connected) {
        publishMqttConfig();
        publishMqttStatus();
        sendConfig();
    }
}

static void sendConfig(AsyncWebSocketClient* client) {
    const String json = buildConfigJson();
    if (client) {
        client->text(json);
    } else {
        ws.textAll(json);
    }
}

static inline int stepsToBin(int32_t pos) {
    int32_t norm = pos % derivedSettings.stepsPerLidarRev;
    if (norm < 0) norm += derivedSettings.stepsPerLidarRev;
    return static_cast<int>(static_cast<int64_t>(norm) * BINS / derivedSettings.stepsPerLidarRev);
}

static inline float stepsToAngle(int32_t pos) {
    int32_t norm = pos % derivedSettings.stepsPerLidarRev;
    if (norm < 0) norm += derivedSettings.stepsPerLidarRev;
    return static_cast<float>(norm) * 360.0f / static_cast<float>(derivedSettings.stepsPerLidarRev);
}

static void resetScanState() {
    memset(frames, 0, sizeof(frames));
    writeFr = 0;
    readFr = 1;
    newFrameReady = false;
    accumSteps = 0;
    lastStepPos = stepper ? stepper->getCurrentPosition() : 0;
    currentAngle = stepper ? stepsToAngle(lastStepPos) : 0.0f;
}

static void applyStepperMotionConfig() {
    if (!stepper) {
        return;
    }

    stepper->setSpeedInHz(derivedSettings.scanSpeedHz);
    stepper->setAcceleration(derivedSettings.scanAccelHz2);
    if (AppConfig::STEPPER_ROTATE_FORWARD) {
        stepper->runForward();
    } else {
        stepper->runBackward();
    }
    resetScanState();
}

static bool readTFMini() {
    static uint8_t buf[9];
    static uint8_t idx = 0;

    while (Serial2.available()) {
        const uint8_t b = static_cast<uint8_t>(Serial2.read());

        if (idx == 0) { if (b != 0x59) { continue; } }
        else if (idx == 1) { if (b != 0x59) { idx = 0; continue; } }

        buf[idx++] = b;
        if (idx < 9) continue;
        idx = 0;

        uint16_t sum = 0;
        for (int i = 0; i < 8; i++) sum += buf[i];
        if ((sum & 0xFFu) != buf[8]) continue;

        tfDist = static_cast<uint16_t>(buf[2] | (buf[3] << 8));
        tfStrength = static_cast<uint16_t>(buf[4] | (buf[5] << 8));
        return true;
    }
    return false;
}

static inline bool tfDistValid() {
    return (tfStrength >= AppConfig::TFMINI_MIN_STRENGTH) &&
           (tfDist >= AppConfig::TFMINI_MIN_CM) &&
           (tfDist <= AppConfig::TFMINI_MAX_CM);
}

static uint16_t filteredDistanceAt(const ScanFrame& frame, int index) {
    const uint8_t radius = runtimeSettings.smoothingRadius;
    if (radius == 0) {
        return frame.dist[index];
    }

    uint32_t weightedSum = 0;
    uint16_t totalWeight = 0;
    for (int offset = -radius; offset <= radius; offset++) {
        const int neighbor = (index + offset + BINS) % BINS;
        const uint16_t sample = frame.dist[neighbor];
        if (sample == 0) {
            continue;
        }

        const uint16_t weight = static_cast<uint16_t>(radius + 1 - abs(offset));
        weightedSum += static_cast<uint32_t>(sample) * weight;
        totalWeight += weight;
    }

    if (totalWeight == 0) {
        return 0;
    }

    return static_cast<uint16_t>((weightedSum + (totalWeight / 2)) / totalWeight);
}

static void broadcastScan() {
    const ScanFrame& frame = frames[readFr];
    uint16_t filtered[BINS];
    uint16_t nearest = 0;
    uint16_t farthest = 0;
    uint16_t validPoints = 0;
    for (int i = 0; i < BINS; i++) {
        filtered[i] = filteredDistanceAt(frame, i);
        if (filtered[i] > 0) {
            if (validPoints == 0 || filtered[i] < nearest) nearest = filtered[i];
            if (filtered[i] > farthest) farthest = filtered[i];
            validPoints++;
        }
    }

    String json;
    json.reserve(2200);
    json = F("{\"type\":\"scan\",\"d\":[");
    for (int i = 0; i < BINS; i++) {
        json += filtered[i];
        if (i < BINS - 1) json += ',';
    }
    json += F("]}");
    ws.textAll(json);

    if (mqttClient.connected()) {
        String mqttScan;
        mqttScan.reserve(2600);
        mqttScan = F("{\"timestamp_ms\":");
        mqttScan += millis();
        mqttScan += F(",\"sequence\":");
        mqttScan += scanSequence++;
        mqttScan += F(",\"angle_min_deg\":0,\"angle_max_deg\":360,\"angle_increment_deg\":");
        mqttScan += String(360.0f / BINS, 3);
        mqttScan += F(",\"range_unit\":\"cm\",\"ranges_cm\":[");
        for (int i = 0; i < BINS; i++) {
            mqttScan += filtered[i];
            if (i < BINS - 1) mqttScan += ',';
        }
        mqttScan += F("]}");
        mqttPublish(mqttTopic("scan"), mqttScan, false);

        String telemetry;
        telemetry.reserve(256);
        telemetry = F("{\"timestamp_ms\":");
        telemetry += millis();
        telemetry += F(",\"valid_points\":");
        telemetry += validPoints;
        telemetry += F(",\"nearest_cm\":");
        telemetry += nearest;
        telemetry += F(",\"farthest_cm\":");
        telemetry += farthest;
        telemetry += F(",\"scan_speed_rps\":");
        telemetry += String(runtimeSettings.lidarRevsPerSec, 2);
        telemetry += F(",\"steps_per_lidar_rev\":");
        telemetry += derivedSettings.stepsPerLidarRev;
        telemetry += F(",\"smoothing_radius\":");
        telemetry += runtimeSettings.smoothingRadius;
        telemetry += '}';
        mqttPublish(mqttTopic("telemetry"), telemetry, false);
        publishMqttTargets(filtered);
    }

    memcpy(lastFilteredScan, filtered, sizeof(lastFilteredScan));
    hasLastFilteredScan = true;
}

static void applyPostedSettings(AsyncWebServerRequest* req) {
    RuntimeSettings next = runtimeSettings;

    if (req->hasParam("microsteps", true)) {
        next.motorMicrosteps = clampU16(
            static_cast<uint16_t>(req->getParam("microsteps", true)->value().toInt()),
            AppConfig::MOTOR_MICROSTEPS_MIN,
            AppConfig::MOTOR_MICROSTEPS_MAX);
    }
    if (req->hasParam("drivePulleyTeeth", true)) {
        next.drivePulleyTeeth = clampU16(
            static_cast<uint16_t>(req->getParam("drivePulleyTeeth", true)->value().toInt()),
            AppConfig::PULLEY_TEETH_MIN,
            AppConfig::PULLEY_TEETH_MAX);
    }
    if (req->hasParam("lidarPulleyTeeth", true)) {
        next.lidarPulleyTeeth = clampU16(
            static_cast<uint16_t>(req->getParam("lidarPulleyTeeth", true)->value().toInt()),
            AppConfig::PULLEY_TEETH_MIN,
            AppConfig::PULLEY_TEETH_MAX);
    }
    if (req->hasParam("lidarRevPerSec", true)) {
        next.lidarRevsPerSec = clampFloat(
            req->getParam("lidarRevPerSec", true)->value().toFloat(),
            AppConfig::LIDAR_REVS_PER_SEC_MIN,
            AppConfig::LIDAR_REVS_PER_SEC_MAX);
    }
    if (req->hasParam("smoothingRadius", true)) {
        next.smoothingRadius = clampU8(
            static_cast<uint8_t>(req->getParam("smoothingRadius", true)->value().toInt()),
            AppConfig::SMOOTHING_RADIUS_MIN,
            AppConfig::SMOOTHING_RADIUS_MAX);
    }
    next.mqttEnabled = req->hasParam("mqttEnabled", true);
    if (req->hasParam("mqttHost", true)) {
        copyCString(next.mqttHost, sizeof(next.mqttHost), req->getParam("mqttHost", true)->value());
    }
    if (req->hasParam("mqttPort", true)) {
        next.mqttPort = clampU16(
            static_cast<uint16_t>(req->getParam("mqttPort", true)->value().toInt()),
            AppConfig::MQTT_PORT_MIN,
            AppConfig::MQTT_PORT_MAX);
    }
    if (req->hasParam("mqttBaseTopic", true)) {
        copyCString(next.mqttBaseTopic, sizeof(next.mqttBaseTopic), req->getParam("mqttBaseTopic", true)->value());
    }
    if (req->hasParam("mqttUsername", true)) {
        copyCString(next.mqttUsername, sizeof(next.mqttUsername), req->getParam("mqttUsername", true)->value());
    }
    if (req->hasParam("mqttPassword", true)) {
        copyCString(next.mqttPassword, sizeof(next.mqttPassword), req->getParam("mqttPassword", true)->value());
    }

    runtimeSettings = next;
    deriveRuntimeSettings();
    persistRuntimeSettings();
    applyStepperMotionConfig();
    mqttClient.disconnect();
    lastMqttConnectAttemptMs = 0;
    sendConfig();
}

static void onWsEvent(AsyncWebSocket* /*server*/,
                      AsyncWebSocketClient* client,
                      AwsEventType type,
                      void* /*arg*/,
                      uint8_t* /*data*/,
                      size_t /*len*/) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u connected\n", client->id());
        sendConfig(client);
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println(F("\n[BOOT] LiDAR Room Scanner"));

    loadRuntimeSettings();
    Serial.printf("[BOOT] Driver: %s\n", AppConfig::STEPPER_DRIVER_LABEL);
    Serial.printf("[BOOT] Mechanics: %u full steps, %u microsteps, pulleys %u:%u -> %.2f motor rev/lidar rev\n",
                  AppConfig::MOTOR_FULL_STEPS_PER_REV,
                  runtimeSettings.motorMicrosteps,
                  runtimeSettings.drivePulleyTeeth,
                  runtimeSettings.lidarPulleyTeeth,
                  derivedSettings.motorRevsPerLidarRev);
    Serial.printf("[BOOT] Scan: %ld steps/lidar rev, %u bins, %.2f lidar rev/s, smoothing %u\n",
                  static_cast<long>(derivedSettings.stepsPerLidarRev),
                  BINS,
                  runtimeSettings.lidarRevsPerSec,
                  runtimeSettings.smoothingRadius);
    Serial.printf("[BOOT] MQTT: %s %s:%u base '%s'\n",
                  runtimeSettings.mqttEnabled ? "enabled" : "disabled",
                  runtimeSettings.mqttHost,
                  runtimeSettings.mqttPort,
                  runtimeSettings.mqttBaseTopic);

    Serial2.begin(AppConfig::TFMINI_BAUD, SERIAL_8N1, AppConfig::PIN_LIDAR_RX, AppConfig::PIN_LIDAR_TX);
    Serial.println(F("[BOOT] TFMini UART2 ready (GPIO16=RX, GPIO17=TX)"));

    if (AppConfig::STEPPER_HAS_ENABLE_PIN && AppConfig::PIN_EN >= 0) {
        pinMode(AppConfig::PIN_EN, OUTPUT);
        setDriverEnabled(true);
    }

    engine.init();
    stepper = engine.stepperConnectToPin(AppConfig::PIN_STEP);
    if (stepper) {
        stepper->setDirectionPin(AppConfig::PIN_DIR);
        applyStepperMotionConfig();
        Serial.printf("[BOOT] Stepper started: %lu steps/s, accel %lu steps/s²\n",
                      (unsigned long)derivedSettings.scanSpeedHz,
                      (unsigned long)derivedSettings.scanAccelHz2);
    } else {
        Serial.println(F("[BOOT] ERROR – stepper init failed. Check AppConfig::PIN_STEP"));
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(AppConfig::MDNS_HOSTNAME);
    WiFi.begin(AppConfig::WIFI_STA_SSID, AppConfig::WIFI_STA_PASS);
    Serial.printf("[WiFi] Connecting to '%s'", AppConfig::WIFI_STA_SSID);
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < AppConfig::WIFI_CONNECT_TIMEOUT_MS) {
        delay(AppConfig::WIFI_RETRY_INTERVAL_MS);
        Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected — open  http://%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println(F("\n[WiFi] STA failed → starting AP"));
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AppConfig::WIFI_AP_SSID, AppConfig::WIFI_AP_PASS);
        Serial.printf("[WiFi] AP '%s' ready — open  http://%s\n",
                      AppConfig::WIFI_AP_SSID,
                      WiFi.softAPIP().toString().c_str());
    }

    if (MDNS.begin(AppConfig::MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[NET] mDNS ready — open  http://%s.local\n", AppConfig::MDNS_HOSTNAME);
    } else {
        Serial.println(F("[NET] mDNS startup failed"));
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", WEBPAGE_HTML);
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", buildConfigJson());
    });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req) {
        applyPostedSettings(req);
        req->send(200, "application/json", buildConfigJson());
    });

    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println(F("[HTTP] Server listening on port 80"));

    resetScanState();
    mqttClient.setBufferSize(AppConfig::MQTT_PACKET_SIZE);
}

void loop() {
    ensureMqttConnected();
    mqttClient.loop();

    const bool mqttConnected = mqttClient.connected();
    if (mqttConnected != lastReportedMqttConnected) {
        lastReportedMqttConnected = mqttConnected;
        sendConfig();
    }

    if (stepper) {
        const int32_t pos = stepper->getCurrentPosition();
        const int32_t delta = pos - lastStepPos;

        if (delta != 0) {
            lastStepPos = pos;
            accumSteps += abs32(delta);
            currentAngle = stepsToAngle(pos);

            while (accumSteps >= derivedSettings.stepsPerLidarRev) {
                accumSteps -= derivedSettings.stepsPerLidarRev;
                readFr = writeFr;
                writeFr = writeFr ^ 1u;
                memset(&frames[writeFr], 0, sizeof(ScanFrame));
                newFrameReady = true;
            }
        }
    }

    if (readTFMini() && stepper) {
        const int32_t pos = stepper->getCurrentPosition();
        const int bin = stepsToBin(pos);
        if (bin >= 0 && bin < BINS) {
            frames[writeFr].dist[bin] = tfDistValid() ? tfDist : 0u;
            frames[writeFr].strength[bin] = tfStrength;
        }
    }

    if (newFrameReady) {
        newFrameReady = false;
        if (ws.count() > 0) {
            broadcastScan();
        }
    }

    const uint32_t now = millis();
    if ((now - lastAngleSendMs) >= AppConfig::WS_ANGLE_UPDATE_MS && ws.count() > 0) {
        lastAngleSendMs = now;
        char buf[36];
        snprintf(buf, sizeof(buf), "{\"type\":\"angle\",\"a\":%.1f}", currentAngle);
        ws.textAll(buf);
        if (mqttClient.connected()) {
            publishMqttAngle();
        }
    }

    if (mqttClient.connected() && (now - lastMqttStatusMs) >= AppConfig::MQTT_STATUS_INTERVAL_MS) {
        lastMqttStatusMs = now;
        publishMqttStatus();
    }

    static uint32_t lastCleanMs = 0;
    if ((now - lastCleanMs) >= AppConfig::WS_CLEANUP_MS) {
        lastCleanMs = now;
        ws.cleanupClients();
    }
}
