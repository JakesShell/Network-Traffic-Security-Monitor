#ifndef TRAFFIC_MONITOR_H
#define TRAFFIC_MONITOR_H

#include <string>
#include <vector>

struct MonitorConfig {
    std::string monitorName = "Cloud Network Traffic Monitoring And Incident Response Simulation";
    std::string environment = "Simulated Cloud Web Application";
    std::string cloudRegion = "us-east-1";
    std::string outputFile = "network_events.ndjson";
    std::string alertLogFile = "logs.txt";
    std::string sampleDataFile = "sample_traffic_data.json";
    int intervalSeconds = 1;
    int maxEvents = 8;
    int requestAlertThreshold = 200;
    int byteAlertThreshold = 1000;
    std::string defaultSource = "10.0.0.15";
    std::string defaultDestination = "10.0.0.25";
};

struct SampleTrafficRecord {
    std::string ip;
    int requests;
};

class TrafficMonitor {
public:
    explicit TrafficMonitor(const std::string& configPath);
    void startMonitoring();

private:
    MonitorConfig config;
    int eventCounter;

    MonitorConfig loadConfig(const std::string& configPath);
    std::string readFile(const std::string& path);
    std::string extractString(const std::string& content, const std::string& key, const std::string& fallback);
    int extractInt(const std::string& content, const std::string& key, int fallback);

    std::string getCurrentTimestamp() const;
    std::vector<SampleTrafficRecord> parseSampleTrafficData(const std::string& content) const;
    void evaluateSampleTrafficData();

    std::string generateTrafficEvent();
    std::string classifySeverity(const std::string& protocol, int bytes, int requests) const;
    std::string determineIncidentCategory(const std::string& protocol, int bytes, int requests) const;
    std::string recommendResponseAction(const std::string& severity, const std::string& category) const;

    void resetRuntimeFiles();
    void persistEvent(const std::string& eventJson);
    void persistLog(const std::string& logMessage);
    void printSummary(int eventId, const std::string& severity, const std::string& category, bool alertTriggered) const;
};

#endif
