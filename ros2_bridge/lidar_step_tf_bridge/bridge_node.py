import json
import math
import queue
import threading

import paho.mqtt.client as mqtt
import rclpy
from geometry_msgs.msg import Pose, PoseArray
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
from std_msgs.msg import String


class LidarMqttBridge(Node):
    def __init__(self) -> None:
        super().__init__("lidar_mqtt_bridge")
        self.declare_parameter("mqtt_host", "127.0.0.1")
        self.declare_parameter("mqtt_port", 1883)
        self.declare_parameter("mqtt_base_topic", "lidar/room_scanner")
        self.declare_parameter("frame_id", "lidar_link")
        self.declare_parameter("scan_topic", "scan")
        self.declare_parameter("targets_topic", "targets")
        self.declare_parameter("status_topic", "status_json")

        self.frame_id = self.get_parameter("frame_id").get_parameter_value().string_value
        self.base_topic = self.get_parameter("mqtt_base_topic").get_parameter_value().string_value.rstrip("/")

        self.scan_pub = self.create_publisher(LaserScan, self.get_parameter("scan_topic").value, 10)
        self.targets_pub = self.create_publisher(PoseArray, self.get_parameter("targets_topic").value, 10)
        self.status_pub = self.create_publisher(String, self.get_parameter("status_topic").value, 10)

        self._queue = queue.Queue()
        self._client = mqtt.Client()
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message

        host = self.get_parameter("mqtt_host").value
        port = int(self.get_parameter("mqtt_port").value)
        self._client.connect(host, port, keepalive=30)
        self._thread = threading.Thread(target=self._client.loop_forever, daemon=True)
        self._thread.start()

        self.create_timer(0.02, self._drain_queue)

    def _on_connect(self, client, userdata, flags, rc):
        if rc != 0:
            self.get_logger().error(f"MQTT connect failed: rc={rc}")
            return
        client.subscribe(f"{self.base_topic}/scan")
        client.subscribe(f"{self.base_topic}/targets")
        client.subscribe(f"{self.base_topic}/status")
        self.get_logger().info(f"Subscribed to {self.base_topic}/scan, /targets, /status")

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except json.JSONDecodeError:
            self.get_logger().warning(f"Invalid JSON on {msg.topic}")
            return
        self._queue.put((msg.topic, payload))

    def _drain_queue(self):
        while not self._queue.empty():
            topic, payload = self._queue.get_nowait()
            if topic.endswith("/scan"):
                self._publish_scan(payload)
            elif topic.endswith("/targets"):
                self._publish_targets(payload)
            elif topic.endswith("/status"):
                msg = String()
                msg.data = json.dumps(payload)
                self.status_pub.publish(msg)

    def _publish_scan(self, payload):
        ranges_cm = payload.get("ranges_cm", [])
        count = len(ranges_cm)
        if count == 0:
            return

        increment = (2.0 * math.pi) / count
        scan = LaserScan()
        scan.header.stamp = self.get_clock().now().to_msg()
        scan.header.frame_id = self.frame_id
        scan.angle_min = -math.pi
        scan.angle_max = math.pi - increment
        scan.angle_increment = increment
        scan.time_increment = 0.0
        scan.scan_time = 1.0 / max(float(payload.get("sequence", 1) or 1), 1.0)
        scan.range_min = 0.30
        scan.range_max = 12.0

        ros_ranges = []
        for i in range(count):
            ros_angle = -math.pi + i * increment
            firmware_deg = (-math.degrees(ros_angle)) % 360.0
            fw_index = int(round((firmware_deg / 360.0) * count)) % count
            dist_cm = ranges_cm[fw_index]
            ros_ranges.append(float("inf") if not dist_cm else dist_cm / 100.0)
        scan.ranges = ros_ranges
        self.scan_pub.publish(scan)

    def _publish_targets(self, payload):
        pose_array = PoseArray()
        pose_array.header.stamp = self.get_clock().now().to_msg()
        pose_array.header.frame_id = self.frame_id

        for target in payload.get("targets", []):
            angle_rad = math.radians(float(target.get("angle_deg", 0.0)))
            dist_m = float(target.get("distance_cm", 0.0)) / 100.0
            pose = Pose()
            pose.position.x = dist_m * math.cos(angle_rad)
            pose.position.y = -dist_m * math.sin(angle_rad)
            pose.position.z = 0.0
            pose.orientation.w = 1.0
            pose_array.poses.append(pose)

        self.targets_pub.publish(pose_array)


def main():
    rclpy.init()
    node = LidarMqttBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()