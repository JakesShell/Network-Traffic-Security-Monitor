cmake_minimum_required(VERSION 3.16)
project(CloudNetworkTrafficMonitoringIncidentResponse)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cloud_network_traffic_monitor
    main.cpp
    TrafficMonitor.cpp
)
