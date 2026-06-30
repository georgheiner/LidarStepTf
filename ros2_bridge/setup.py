from setuptools import setup

package_name = "lidar_step_tf_bridge"

setup(
    name=package_name,
    version="0.1.0",
    packages=[package_name],
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
    ],
    install_requires=["setuptools", "paho-mqtt"],
    zip_safe=True,
    maintainer="Local User",
    maintainer_email="local@example.invalid",
    description="ROS 2 bridge for LiDAR room scanner MQTT topics",
    license="MIT",
    entry_points={
        "console_scripts": [
            "bridge_node = lidar_step_tf_bridge.bridge_node:main",
        ],
    },
)