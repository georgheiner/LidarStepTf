#pragma once

namespace AppConfig {

static constexpr const char* PREF_NAMESPACE = "lidar-scan";

// WiFi
static constexpr const char* WIFI_STA_SSID = "YOUR_SSID";
static constexpr const char* WIFI_STA_PASS = "YOUR_PASSWORD";
static constexpr const char* WIFI_AP_SSID  = "LidarScanner";
static constexpr const char* WIFI_AP_PASS  = "lidar1234";

// GPIO wiring
static constexpr int PIN_STEP     = 25;
static constexpr int PIN_DIR      = 26;
static constexpr int PIN_EN       = 27;
static constexpr int PIN_LIDAR_RX = 16;
static constexpr int PIN_LIDAR_TX = 17;

// Generic step/dir driver configuration. This works for TMC2208, TMC2209,
// A4988, DRV8825, and similar drivers when used in STEP/DIR mode.
static constexpr const char* STEPPER_DRIVER_LABEL = "STEP/DIR driver (TMC2208/TMC2209 compatible)";
static constexpr bool STEPPER_HAS_ENABLE_PIN      = true;
static constexpr bool STEPPER_ENABLE_ACTIVE_LOW   = true;
static constexpr bool STEPPER_ROTATE_FORWARD      = true;

// Mechanics. Adjust these values when the CAD design changes.
static constexpr uint16_t MOTOR_FULL_STEPS_PER_REV = 200;
static constexpr uint16_t MOTOR_MICROSTEPS         = 8;
static constexpr uint16_t DRIVE_PULLEY_TEETH       = 20;
static constexpr uint16_t LIDAR_PULLEY_TEETH       = 60;

// Scan quality / speed.
static constexpr uint16_t SCAN_BINS               = 360;
static constexpr float    LIDAR_REVS_PER_SEC      = 1.0f;
static constexpr float    STEPPER_ACCEL_TIME_SEC  = 0.5f;
static constexpr uint8_t  SMOOTHING_RADIUS       = 1;

// Runtime setting limits.
static constexpr uint16_t MOTOR_MICROSTEPS_MIN   = 1;
static constexpr uint16_t MOTOR_MICROSTEPS_MAX   = 256;
static constexpr uint16_t PULLEY_TEETH_MIN       = 8;
static constexpr uint16_t PULLEY_TEETH_MAX       = 400;
static constexpr float    LIDAR_REVS_PER_SEC_MIN = 0.10f;
static constexpr float    LIDAR_REVS_PER_SEC_MAX = 3.00f;
static constexpr uint8_t  SMOOTHING_RADIUS_MIN   = 0;
static constexpr uint8_t  SMOOTHING_RADIUS_MAX   = 3;

// TFMini UART and validity checks.
static constexpr uint32_t TFMINI_BAUD         = 115200;
static constexpr uint16_t TFMINI_MIN_CM       = 30;
static constexpr uint16_t TFMINI_MAX_CM       = 1200;
static constexpr uint16_t TFMINI_MIN_STRENGTH = 100;

// Local network discovery.
static constexpr const char* MDNS_HOSTNAME = "lidar-scanner";

// Motion clustering for MQTT / ROS consumers.
static constexpr uint16_t TARGET_MOTION_THRESHOLD_CM = 20;
static constexpr uint8_t  TARGET_MIN_CLUSTER_BINS    = 2;
static constexpr uint8_t  TARGET_MAX_COUNT           = 24;

// Network / UI timing.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS  = 400;
static constexpr uint32_t WS_ANGLE_UPDATE_MS      = 50;
static constexpr uint32_t WS_CLEANUP_MS           = 3000;

// MQTT defaults and limits.
static constexpr bool     MQTT_ENABLED_DEFAULT            = false;
static constexpr const char* MQTT_HOST_DEFAULT            = "192.168.1.10";
static constexpr uint16_t MQTT_PORT_DEFAULT               = 1883;
static constexpr const char* MQTT_BASE_TOPIC_DEFAULT      = "lidar/room_scanner";
static constexpr const char* MQTT_USERNAME_DEFAULT        = "";
static constexpr const char* MQTT_PASSWORD_DEFAULT        = "";
static constexpr uint16_t MQTT_PORT_MIN                   = 1;
static constexpr uint16_t MQTT_PORT_MAX                   = 65535;
static constexpr uint16_t MQTT_RECONNECT_MS               = 5000;
static constexpr uint16_t MQTT_STATUS_INTERVAL_MS         = 5000;
static constexpr uint16_t MQTT_PACKET_SIZE                = 8192;
static constexpr size_t   MQTT_HOST_MAX_LEN               = 64;
static constexpr size_t   MQTT_TOPIC_MAX_LEN              = 96;
static constexpr size_t   MQTT_USERNAME_MAX_LEN           = 64;
static constexpr size_t   MQTT_PASSWORD_MAX_LEN           = 64;
static constexpr size_t   MQTT_CLIENT_ID_MAX_LEN          = 48;

}  // namespace AppConfig