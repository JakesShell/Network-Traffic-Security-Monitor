#include "TrafficMonitor.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

TrafficMonitor::TrafficMonitor(const std::string& configPath)
    : config(loadConfig(configPath)), eventCounter(0) {
}

std::string TrafficMonitor::readFile(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string TrafficMonitor::extractString(const std::string& content, const std::string& key, const std::string& fallback) {
    std::string quotedKey = "\"" + key + "\"";
    size_t keyPos = content.find(quotedKey);

    if (keyPos == std::string::npos) {
        return fallback;
    }

    size_t colonPos = content.find(':', keyPos);
    size_t firstQuote = content.find('"', colonPos + 1);
    size_t secondQuote = content.find('"', firstQuote + 1);

    if (colonPos == std::string::npos || firstQuote == std::string::npos || secondQuote == std::string::npos) {
        return fallback;
    }

    return content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

int TrafficMonitor::extractInt(const std::string& content, const std::string& key, int fallback) {
    std::string quotedKey = "\"" + key + "\"";
    size_t keyPos = content.find(quotedKey);

    if (keyPos == std::string::npos) {
        return fallback;
    }

    size_t colonPos = content.find(':', keyPos);

    if (colonPos == std::string::npos) {
        return fallback;
    }

    size_t numberStart = content.find_first_of("0123456789", colonPos + 1);
    size_t numberEnd = content.find_first_not_of("0123456789", numberStart);

    if (numberStart == std::string::npos) {
        return fallback;
    }

    return std::stoi(content.substr(numberStart, numberEnd - numberStart));
}

MonitorConfig TrafficMonitor::loadConfig(const std::string& configPath) {
    MonitorConfig loaded;
    std::string content = readFile(configPath);

    if (content.empty()) {
        return loaded;
    }

    loaded.monitorName = extractString(content, "monitor_name", loaded.monitorName);
    loaded.environment = extractString(content, "environment", loaded.environment);
    loaded.cloudRegion = extractString(content, "cloud_region", loaded.cloudRegion);
    loaded.outputFile = extractString(content, "output_file", loaded.outputFile);
    loaded.alertLogFile = extractString(content, "alert_log_file", loaded.alertLogFile);
    loaded.sampleDataFile = extractString(content, "sample_data_file", loaded.sampleDataFile);
    loaded.intervalSeconds = extractInt(content, "interval_seconds", loaded.intervalSeconds);
    loaded.maxEvents = extractInt(content, "max_events", loaded.maxEvents);
    loaded.requestAlertThreshold = extractInt(content, "request_alert_threshold", loaded.requestAlertThreshold);
    loaded.byteAlertThreshold = extractInt(content, "byte_alert_threshold", loaded.byteAlertThreshold);
    loaded.defaultSource = extractString(content, "default_source", loaded.defaultSource);
    loaded.defaultDestination = extractString(content, "default_destination", loaded.defaultDestination);

    return loaded;
}

std::string TrafficMonitor::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif

    std::ostringstream timestamp;
    timestamp << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S");
    return timestamp.str();
}

std::vector<SampleTrafficRecord> TrafficMonitor::parseSampleTrafficData(const std::string& content) const {
    std::vector<SampleTrafficRecord> records;
    size_t position = 0;

    while ((position = content.find("\"ip\"", position)) != std::string::npos) {
        size_t ipColon = content.find(':', position);
        size_t ipFirstQuote = content.find('"', ipColon + 1);
        size_t ipSecondQuote = content.find('"', ipFirstQuote + 1);

        if (ipColon == std::string::npos || ipFirstQuote == std::string::npos || ipSecondQuote == std::string::npos) {
            break;
        }

        std::string ip = content.substr(ipFirstQuote + 1, ipSecondQuote - ipFirstQuote - 1);

        size_t requestsKey = content.find("\"requests\"", ipSecondQuote);
        size_t requestsColon = content.find(':', requestsKey);
        size_t requestsStart = content.find_first_of("0123456789", requestsColon + 1);
        size_t requestsEnd = content.find_first_not_of("0123456789", requestsStart);

        if (requestsKey == std::string::npos || requestsColon == std::string::npos || requestsStart == std::string::npos) {
            break;
        }

        int requests = std::stoi(content.substr(requestsStart, requestsEnd - requestsStart));
        records.push_back({ip, requests});

        position = requestsEnd;
    }

    return records;
}

void TrafficMonitor::resetRuntimeFiles() {
    std::ofstream eventOutput(config.outputFile, std::ios::trunc);
    std::ofstream logOutput(config.alertLogFile, std::ios::trunc);
}

void TrafficMonitor::persistLog(const std::string& logMessage) {
    std::ofstream output(config.alertLogFile, std::ios::app);
    output << "[" << getCurrentTimestamp() << "] " << logMessage << std::endl;
}

void TrafficMonitor::evaluateSampleTrafficData() {
    std::string content = readFile(config.sampleDataFile);

    if (content.empty()) {
        std::string message = "Sample traffic data file could not be opened: " + config.sampleDataFile;
        std::cout << "[Warning] " << message << std::endl;
        persistLog(message);
        return;
    }

    std::vector<SampleTrafficRecord> records = parseSampleTrafficData(content);

    std::cout << "Sample Investigation Data: " << config.sampleDataFile << std::endl;
    std::cout << "Request Alert Threshold: " << config.requestAlertThreshold << std::endl;
    std::cout << std::endl;

    persistLog("Started sample traffic investigation.");

    for (const SampleTrafficRecord& record : records) {
        std::ostringstream note;
        note << "Investigated source " << record.ip << " with " << record.requests << " requests.";
        persistLog(note.str());

        if (record.requests > config.requestAlertThreshold) {
            std::ostringstream alert;
            alert << "[ALERT] " << record.ip << " generated " << record.requests
                  << " requests. Threshold: " << config.requestAlertThreshold;

            std::cout << alert.str() << std::endl;
            persistLog(alert.str());
        } else {
            std::cout << "[Normal] " << record.ip << " generated " << record.requests << " requests." << std::endl;
        }
    }

    std::cout << std::endl;
}

std::string TrafficMonitor::classifySeverity(const std::string& protocol, int bytes, int requests) const {
    if (requests > config.requestAlertThreshold * 2 || bytes > config.byteAlertThreshold * 2) {
        return "Critical";
    }

    if (requests > config.requestAlertThreshold || (protocol == "ICMP" && bytes > config.byteAlertThreshold)) {
        return "High";
    }

    if (bytes > config.byteAlertThreshold || protocol == "SSH") {
        return "Medium";
    }

    return "Low";
}

std::string TrafficMonitor::determineIncidentCategory(const std::string& protocol, int bytes, int requests) const {
    if (protocol == "SSH" && requests > config.requestAlertThreshold) {
        return "Potential Brute-Force Attempt";
    }

    if (protocol == "ICMP" && bytes > config.byteAlertThreshold) {
        return "Possible Ping Flood";
    }

    if (requests > config.requestAlertThreshold) {
        return "Traffic Spike";
    }

    if (bytes > config.byteAlertThreshold) {
        return "Large Payload Transfer";
    }

    return "Normal Activity";
}

std::string TrafficMonitor::recommendResponseAction(const std::string& severity, const std::string& category) const {
    if (severity == "Critical") {
        return "Escalate immediately, review source IP activity, and prepare incident notes.";
    }

    if (severity == "High") {
        return "Investigate traffic source, validate service health, and monitor for repeat activity.";
    }

    if (category == "Large Payload Transfer") {
        return "Review payload size, destination service, and expected application behavior.";
    }

    return "Continue monitoring and retain event for trend analysis.";
}

std::string TrafficMonitor::generateTrafficEvent() {
    static std::vector<std::string> protocols = {"TCP", "UDP", "ICMP", "HTTPS", "SSH"};
    static std::vector<std::string> destinations = {
        config.defaultDestination,
        "10.0.0.40",
        "172.16.0.12",
        "192.168.1.50",
        "10.0.1.20"
    };

    static std::vector<std::string> services = {
        "api-gateway",
        "auth-service",
        "billing-service",
        "customer-portal",
        "internal-admin-service"
    };

    static std::mt19937 rng(static_cast<unsigned int>(
        std::chrono::system_clock::now().time_since_epoch().count()
    ));

    std::uniform_int_distribution<int> protocolDist(0, static_cast<int>(protocols.size() - 1));
    std::uniform_int_distribution<int> destinationDist(0, static_cast<int>(destinations.size() - 1));
    std::uniform_int_distribution<int> serviceDist(0, static_cast<int>(services.size() - 1));
    std::uniform_int_distribution<int> bytesDist(120, 2500);
    std::uniform_int_distribution<int> requestsDist(25, 450);

    std::string protocol = protocols[protocolDist(rng)];
    std::string destination = destinations[destinationDist(rng)];
    std::string service = services[serviceDist(rng)];
    int bytes = bytesDist(rng);
    int requests = requestsDist(rng);

    std::string severity = classifySeverity(protocol, bytes, requests);
    std::string category = determineIncidentCategory(protocol, bytes, requests);
    std::string action = recommendResponseAction(severity, category);
    bool alertTriggered = severity == "High" || severity == "Critical";

    eventCounter++;

    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << getCurrentTimestamp() << "\","
         << "\"event_id\":" << eventCounter << ","
         << "\"environment\":\"" << config.environment << "\","
         << "\"cloud_region\":\"" << config.cloudRegion << "\","
         << "\"source\":\"" << config.defaultSource << "\","
         << "\"destination\":\"" << destination << "\","
         << "\"service\":\"" << service << "\","
         << "\"protocol\":\"" << protocol << "\","
         << "\"requests\":" << requests << ","
         << "\"bytes\":" << bytes << ","
         << "\"severity\":\"" << severity << "\","
         << "\"incident_category\":\"" << category << "\","
         << "\"alert_triggered\":" << (alertTriggered ? "true" : "false") << ","
         << "\"recommended_action\":\"" << action << "\""
         << "}";

    if (alertTriggered) {
        std::ostringstream alert;
        alert << "[ALERT] Event " << eventCounter << " | Severity: " << severity
              << " | Category: " << category
              << " | Requests: " << requests
              << " | Bytes: " << bytes;

        std::cout << alert.str() << std::endl;
        persistLog(alert.str());
    }

    printSummary(eventCounter, severity, category, alertTriggered);

    return json.str();
}

void TrafficMonitor::persistEvent(const std::string& eventJson) {
    std::ofstream output(config.outputFile, std::ios::app);
    output << eventJson << std::endl;
}

void TrafficMonitor::printSummary(int eventId, const std::string& severity, const std::string& category, bool alertTriggered) const {
    std::cout << "[Monitor] Event " << eventId
              << " | Severity: " << severity
              << " | Category: " << category
              << " | Alert: " << (alertTriggered ? "true" : "false")
              << std::endl;
}

void TrafficMonitor::startMonitoring() {
    resetRuntimeFiles();

    std::cout << config.monitorName << std::endl;
    std::cout << "Environment: " << config.environment << std::endl;
    std::cout << "Cloud Region: " << config.cloudRegion << std::endl;
    std::cout << "Writing Events To: " << config.outputFile << std::endl;
    std::cout << "Writing Incident Logs To: " << config.alertLogFile << std::endl;
    std::cout << "Max Events: " << config.maxEvents << std::endl;
    std::cout << "Interval: " << config.intervalSeconds << " Seconds" << std::endl;
    std::cout << std::endl;

    persistLog("Cloud network traffic monitoring simulation started.");

    evaluateSampleTrafficData();

    for (int i = 0; i < config.maxEvents; ++i) {
        std::string trafficEvent = generateTrafficEvent();
        persistEvent(trafficEvent);

        if (i < config.maxEvents - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(config.intervalSeconds));
        }
    }

    std::cout << std::endl;
    std::cout << "Monitoring Complete." << std::endl;
    std::cout << "Events saved to " << config.outputFile << std::endl;
    std::cout << "Incident notes saved to " << config.alertLogFile << std::endl;

    persistLog("Cloud network traffic monitoring simulation completed.");
}
