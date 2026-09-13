#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace RadiosondePI::Web {

struct WebServerConfig {
    uint16_t port{8080};
    std::string bindAddress{"0.0.0.0"};
    std::string webRoot{"web"};
};

class WebDashboard {
public:
    explicit WebDashboard(const WebServerConfig& config = WebServerConfig{})
        : m_config(config) {}

    void updateTelemetry(const Telemetry::TelemetryFrame& frame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latestFrame = frame;
        m_history.push_back(frame);
        if (m_history.size() > 500) {
            m_history.erase(m_history.begin());
        }
    }

    [[nodiscard]] std::string getLatestTelemetryJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& f = m_latestFrame;

        std::stringstream json;
        json << std::fixed << std::setprecision(5);
        json << "{\n";
        json << "  \"type\": \"" << Telemetry::sondeTypeToString(f.type) << "\",\n";
        json << "  \"serial\": \"" << f.serialNumber << "\",\n";
        json << "  \"frame\": " << f.frameNumber << ",\n";
        json << "  \"freq_hz\": " << f.frequencyHz << ",\n";
        json << "  \"lat\": " << f.latitude << ",\n";
        json << "  \"lon\": " << f.longitude << ",\n";
        json << "  \"alt\": " << std::setprecision(1) << f.altitudeMeters << ",\n";
        json << "  \"climb\": " << std::setprecision(2) << f.climbRateMps << ",\n";
        json << "  \"speed\": " << std::setprecision(2) << f.speedMps * 3.6f << ",\n";
        json << "  \"heading\": " << std::setprecision(1) << f.headingDeg << ",\n";
        json << "  \"temp\": " << (f.temperatureC.has_value() ? std::to_string(*f.temperatureC) : "null") << ",\n";
        json << "  \"rh\": " << (f.relativeHumidityPercent.has_value() ? std::to_string(*f.relativeHumidityPercent) : "null") << ",\n";
        json << "  \"batt\": " << (f.batteryVoltageV.has_value() ? std::to_string(*f.batteryVoltageV) : "null") << ",\n";
        json << "  \"gps_valid\": " << (f.gpsValid ? "true" : "false") << "\n";
        json << "}";
        return json.str();
    }

    [[nodiscard]] std::string getFlightPathJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::stringstream json;
        json << std::fixed << std::setprecision(5);
        json << "[\n";
        for (size_t i = 0; i < m_history.size(); ++i) {
            const auto& pt = m_history[i];
            if (!pt.gpsValid) continue;
            json << "  {\"lat\": " << pt.latitude 
                 << ", \"lon\": " << pt.longitude 
                 << ", \"alt\": " << std::setprecision(1) << pt.altitudeMeters << "}";
            if (i + 1 < m_history.size()) json << ",";
            json << "\n";
        }
        json << "]";
        return json.str();
    }

private:
#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <atomic>
#include <iostream>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#define CLOSE_SOCK(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
#define CLOSE_SOCK(s) close(s)
#endif

namespace RadiosondePI::Web {

struct WebServerConfig {
    uint16_t port{8080};
    std::string bindAddress{"0.0.0.0"};
    std::string webRoot{"web"};
};

class WebDashboard {
public:
    explicit WebDashboard(const WebServerConfig& config = WebServerConfig{})
        : m_config(config) {}

    ~WebDashboard() {
        stop();
    }

    void updateTelemetry(const Telemetry::TelemetryFrame& frame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latestFrame = frame;
        m_history.push_back(frame);
        if (m_history.size() > 500) {
            m_history.erase(m_history.begin());
        }
    }

    [[nodiscard]] std::string getLatestTelemetryJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& f = m_latestFrame;

        std::stringstream json;
        json << std::fixed << std::setprecision(5);
        json << "{\n";
        json << "  \"type\": \"" << Telemetry::sondeTypeToString(f.type) << "\",\n";
        json << "  \"serial\": \"" << f.serialNumber << "\",\n";
        json << "  \"frame\": " << f.frameNumber << ",\n";
        json << "  \"freq_hz\": " << f.frequencyHz << ",\n";
        json << "  \"lat\": " << f.latitude << ",\n";
        json << "  \"lon\": " << f.longitude << ",\n";
        json << "  \"alt\": " << std::setprecision(1) << f.altitudeMeters << ",\n";
        json << "  \"climb\": " << std::setprecision(2) << f.climbRateMps << ",\n";
        json << "  \"speed\": " << std::setprecision(2) << f.speedMps * 3.6f << ",\n";
        json << "  \"heading\": " << std::setprecision(1) << f.headingDeg << ",\n";
        json << "  \"temp\": " << (f.temperatureC.has_value() ? std::to_string(*f.temperatureC) : "null") << ",\n";
        json << "  \"rh\": " << (f.relativeHumidityPercent.has_value() ? std::to_string(*f.relativeHumidityPercent) : "null") << ",\n";
        json << "  \"batt\": " << (f.batteryVoltageV.has_value() ? std::to_string(*f.batteryVoltageV) : "null") << ",\n";
        json << "  \"gps_valid\": " << (f.gpsValid ? "true" : "false") << "\n";
        json << "}";
        return json.str();
    }

    [[nodiscard]] std::string getFlightPathJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::stringstream json;
        json << std::fixed << std::setprecision(5);
        json << "[\n";
        for (size_t i = 0; i < m_history.size(); ++i) {
            const auto& pt = m_history[i];
            if (!pt.gpsValid) continue;
            json << "  {\"lat\": " << pt.latitude 
                 << ", \"lon\": " << pt.longitude 
                 << ", \"alt\": " << std::setprecision(1) << pt.altitudeMeters << "}";
            if (i + 1 < m_history.size()) json << ",";
            json << "\n";
        }
        json << "]";
        return json.str();
    }

    bool start() {
        if (m_running) return true;

#if defined(_WIN32)
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "[WEB] WSAStartup failed" << std::endl;
            return false;
        }
#endif

        m_serverSock = socket(AF_INET, SOCK_STREAM, 0);
        if (m_serverSock == INVALID_SOCK) {
            std::cerr << "[WEB] Failed to create server socket" << std::endl;
            return false;
        }

        int opt = 1;
#if defined(_WIN32)
        setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_config.port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            std::cerr << "[WEB] Failed to bind port " << m_config.port << std::endl;
            CLOSE_SOCK(m_serverSock);
            m_serverSock = INVALID_SOCK;
            return false;
        }

        if (listen(m_serverSock, 10) != 0) {
            std::cerr << "[WEB] Failed to listen on socket" << std::endl;
            CLOSE_SOCK(m_serverSock);
            m_serverSock = INVALID_SOCK;
            return false;
        }

        m_running = true;
        m_serverThread = std::thread(&WebDashboard::serverLoop, this);
        std::cout << "[WEB] Embedded HTTP Server running at http://" << m_config.bindAddress << ":" << m_config.port << std::endl;
        return true;
    }

    void stop() {
        if (!m_running) return;
        m_running = false;

        if (m_serverSock != INVALID_SOCK) {
            CLOSE_SOCK(m_serverSock);
            m_serverSock = INVALID_SOCK;
        }

        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }

#if defined(_WIN32)
        WSACleanup();
#endif
    }

private:
    void serverLoop() {
        while (m_running) {
            sockaddr_in clientAddr{};
#if defined(_WIN32)
            int addrLen = sizeof(clientAddr);
#else
            socklen_t addrLen = sizeof(clientAddr);
#endif
            socket_t clientSock = accept(m_serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
            if (clientSock == INVALID_SOCK) {
                if (!m_running) break;
                continue;
            }

            char reqBuf[2048]{};
            int bytesRead = recv(clientSock, reqBuf, sizeof(reqBuf) - 1, 0);
            if (bytesRead > 0) {
                reqBuf[bytesRead] = '\0';
                handleHttpRequest(clientSock, reqBuf);
            }
            CLOSE_SOCK(clientSock);
        }
    }

    void handleHttpRequest(socket_t clientSock, const char* request) {
        std::string req(request);
        std::string method, path;
        std::stringstream ss(req);
        ss >> method >> path;

        if (method != "GET") {
            sendResponse(clientSock, 405, "Method Not Allowed", "text/plain", "Method Not Allowed");
            return;
        }

        if (path == "/api/telemetry") {
            sendResponse(clientSock, 200, "OK", "application/json", getLatestTelemetryJson());
        } else if (path == "/api/flightpath") {
            sendResponse(clientSock, 200, "OK", "application/json", getFlightPathJson());
        } else {
            // Static file serving
            if (path == "/" || path.empty()) {
                path = "/index.html";
            }

            std::string filePath = m_config.webRoot + path;
            std::string contentType = "text/plain";
            if (path.ends_with(".html")) contentType = "text/html";
            else if (path.ends_with(".css")) contentType = "text/css";
            else if (path.ends_with(".js")) contentType = "application/javascript";
            else if (path.ends_with(".json")) contentType = "application/json";

            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                // Try relative to workspace
                file.open("../" + filePath, std::ios::binary);
            }

            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                sendResponse(clientSock, 200, "OK", contentType, buffer.str());
            } else {
                sendResponse(clientSock, 404, "Not Found", "text/plain", "404 Not Found");
            }
        }
    }

    void sendResponse(socket_t clientSock, int statusCode, const std::string& statusText,
                      const std::string& contentType, const std::string& body) {
        std::stringstream resp;
        resp << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
        resp << "Content-Type: " << contentType << "\r\n";
        resp << "Content-Length: " << body.size() << "\r\n";
        resp << "Access-Control-Allow-Origin: *\r\n";
        resp << "Connection: close\r\n\r\n";
        resp << body;

        std::string str = resp.str();
        send(clientSock, str.data(), static_cast<int>(str.size()), 0);
    }

    WebServerConfig m_config;
    mutable std::mutex m_mutex;
    Telemetry::TelemetryFrame m_latestFrame;
    std::vector<Telemetry::TelemetryFrame> m_history;

    socket_t m_serverSock{INVALID_SOCK};
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
};

} // namespace RadiosondePI::Web
