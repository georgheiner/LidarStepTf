# LiDAR Room Scanner

ESP32 firmware for a rotating TFMini scanner with a built-in web UI, MQTT publishing, and a ROS 2 bridge.

## Features

- ESP32-hosted radar UI with light mode by default and optional dark mode
- Runtime configuration stored in ESP32 NVS
- Generic STEP/DIR support for TMC2208, TMC2209, A4988, DRV8825, and similar drivers
- MQTT publishing for raw scans, telemetry, angle, status, config, and moving-target clusters
- mDNS hostname support at `http://lidar-scanner.local`
- ROS 2 bridge package for converting MQTT JSON into `sensor_msgs/LaserScan` and `geometry_msgs/PoseArray`

## Firmware Setup

1. Open the repo root in VS Code with PlatformIO installed.
2. Edit defaults in [src/app_config.h](src/app_config.h) if needed.
3. Build or upload with PlatformIO:

```bash
platformio run
platformio run -t upload
platformio device monitor
```

If the device joins your WiFi network, the UI is reachable at the printed IP and, on a typical local network, also at `http://lidar-scanner.local`.

If WiFi STA fails, the ESP32 falls back to its own AP.

## Web UI Configuration

The built-in web UI lets you change and persist:

- drive pulley teeth
- LiDAR pulley teeth
- microsteps
- LiDAR revolutions per second
- smoothing radius
- MQTT enabled state
- MQTT host, port, base topic, username, password

All values are stored in NVS on the ESP32.

## Mock Preview (No Hardware)

The repository includes a browser-only simulator at [mock.html](mock.html).

Quick start options:

1. Open the file directly in your browser.
2. Or run a local static server from the repo root:

```bash
python3 -m http.server 8080
```

Then open `http://localhost:8080/mock.html`.

The mock page uses the same visual style as the embedded UI and simulates room geometry, moving objects, scan speed, and motion highlighting.

## MQTT Topics

Base topic defaults to `lidar/room_scanner` and is configurable.

- `BASE/config`: retained JSON with active scanner config and MQTT state
- `BASE/status`: retained JSON with online state, uptime, IP, RSSI, websocket clients, scan rate
- `BASE/angle`: live sweep angle JSON
- `BASE/scan`: full filtered scan JSON for ROS or other consumers
- `BASE/telemetry`: per-scan summary JSON
- `BASE/targets`: clustered moving-object JSON

Example target payload:

```json
{
  "timestamp_ms": 123456,
  "count": 2,
  "targets": [
    {
      "id": 0,
      "angle_deg": 41.25,
      "distance_cm": 182.5,
      "span_deg": 6.0,
      "points": 6,
      "motion_cm": 28
    }
  ]
}
```

All MQTT payloads are JSON.

## ROS 2 Bridge

The ROS 2 bridge package is in [ros2_bridge](ros2_bridge).

It subscribes to the MQTT topics and publishes:

- `scan` as `sensor_msgs/LaserScan`
- `targets` as `geometry_msgs/PoseArray`
- `status_json` as `std_msgs/String`

### Build in a ROS 2 workspace

Copy `ros2_bridge` into your ROS 2 workspace `src/` folder and build with `colcon`:

```bash
colcon build --packages-select lidar_step_tf_bridge
source install/setup.bash
ros2 run lidar_step_tf_bridge bridge_node
```

### ROS 2 parameters

- `mqtt_host`
- `mqtt_port`
- `mqtt_base_topic`
- `frame_id`
- `scan_topic`
- `targets_topic`
- `status_topic`

Example:

```bash
ros2 run lidar_step_tf_bridge bridge_node --ros-args \
  -p mqtt_host:=192.168.1.10 \
  -p mqtt_base_topic:=lidar/room_scanner \
  -p frame_id:=lidar_link
```

## Notes

- JSON is implemented throughout the web UI API and MQTT interface.
- mDNS is implemented for local discovery with `lidar-scanner.local`.
- TLS is not included because the intended deployment is a trusted local network.