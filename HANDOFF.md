# Handoff Notes

Date: 2026-06-30

## Current Project State

- Firmware builds successfully with PlatformIO.
- Web UI is embedded and working.
- Mock preview page is available at `mock.html`.
- MQTT JSON publishing is implemented (`config`, `status`, `angle`, `scan`, `telemetry`, `targets`).
- mDNS is enabled (`lidar-scanner.local`).
- ROS 2 bridge package exists under `ros2_bridge/`.
- Repository is on GitHub and up to date.

## Quick Start (Tomorrow)

1. Flash over USB once.
2. Open serial monitor at 115200 and verify WiFi + mDNS.
3. Open UI by IP or `http://lidar-scanner.local`.
4. Validate scan + motion in UI.
5. Validate MQTT topics and ROS bridge.

## Next Planned Task

Add OTA firmware updates.

Planned OTA steps:

1. Add `ArduinoOTA` setup in firmware startup.
2. Call `ArduinoOTA.handle()` in the main loop.
3. Add OTA upload env in `platformio.ini`.
4. Test USB upload once, then test OTA upload over WiFi.

## Important Note

Current code publishes `scan`/`telemetry`/`targets` from the scan broadcast path, which is tied to active WebSocket clients. If needed, decouple MQTT publishing to support fully headless MQTT + ROS operation.
