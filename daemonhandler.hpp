#ifndef DAEMON_MANAGER_HPP
#define DAEMON_MANAGER_HPP
//---------------------------------------------------------------------------------------------------------------------

#include <mutex>
#include <cctype>
#include <chrono>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <csignal>
#include <syslog.h>
#include <algorithm>
#include <filesystem>
#include <condition_variable>
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include "httpclient.h"
#include "sysutils.hpp"
#include "configmanager.h"
//---------------------------------------------------------------------------------------------------------------------

namespace Utils
{
    class DaemonHandler
    {
    private:

        static constexpr std::string_view CXP_REPORTS_DIR = "/tmp/diagnostic/";
        static constexpr std::string_view CXP_DAEMON_CONF = "diagnostic";

        std::mutex m_loopMutex;
        DiagnosticConfig m_config;
        boost::asio::io_context m_ioCtx;
        boost::asio::signal_set m_signals;
        std::condition_variable m_loopCv;
        std::atomic<bool> m_running{true};

        void SetFactoryDefaults()
        {
            m_config.macInterface     = "br-lan";
            m_config.dnsHost          = "nxtview.com";
            m_config.latencyHost      = "nxtview.com";
            m_config.latencyPort      = 443;
            m_config.apiUrl           = "";
            m_config.apiToken         = "";
            m_config.checkInterval    = 60;
            m_config.maxQueuedReports = 10;
        }

        std::string ExtractHealthStatus(const std::string &reportJson)
        {
            size_t pos = reportJson.find("\"health\"");
            if (pos == std::string::npos) return "unknown";

            pos = reportJson.find("\"status\"", pos);
            if (pos == std::string::npos) return "unknown";

            pos = reportJson.find(':', pos);
            if (pos == std::string::npos) return "unknown";

            pos = reportJson.find('"', pos);
            if (pos == std::string::npos) return "unknown";

            size_t endPos = reportJson.find('"', pos + 1);
            if (endPos == std::string::npos) return "unknown";

            return reportJson.substr(pos + 1, endPos - pos - 1);
        }

        bool ParseIntervalResponse(const std::string &responseBody, uint64_t &newInterval)
        {
            size_t pos = responseBody.find("\"actions\"");
            if (pos == std::string::npos) return false;

            pos = responseBody.find("\"type\"", pos);
            if (pos == std::string::npos) return false;

            pos = responseBody.find("\"set_interval\"", pos);
            if (pos == std::string::npos) return false;

            pos = responseBody.find("\"value\"", pos);
            if (pos == std::string::npos) return false;

            pos += 7;
            pos = responseBody.find(':', pos);
            if (pos == std::string::npos) return false;
            pos++;

            while (pos < responseBody.length() && std::isspace(static_cast<unsigned char>(responseBody[pos])))
                pos++;

            if (pos >= responseBody.length() || !std::isdigit(static_cast<unsigned char>(responseBody[pos])))
                return false;

            char *endPtr = nullptr;
            newInterval  = std::strtoull(&responseBody[pos], &endPtr, 10);

            return true;
        }

        void ProcessPendingQueue()
        {
            if (m_config.apiUrl.empty()) return;

            std::error_code ec;
            if (!std::filesystem::exists(CXP_REPORTS_DIR, ec)) return;

            std::vector<std::filesystem::directory_entry> reports;
            for (const auto &entry : std::filesystem::directory_iterator(CXP_REPORTS_DIR, ec))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                    reports.push_back(entry);
            }

            std::sort(reports.begin(), reports.end(), [](const auto &a, const auto &b) {
                return a.last_write_time() < b.last_write_time();
            });

            for (const auto &reportFile : reports)
            {
                std::ifstream file(reportFile.path());
                if (!file.is_open()) continue;

                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();

                if (SendReportToRestAPI(content))
                {
                    std::filesystem::remove(reportFile.path(), ec);
                    syslog(LOG_INFO, "Pending queued report %s successfully pushed and cleared", reportFile.path().filename().string().c_str());
                }
                else
                    break;
            }
        }

        void SaveReportToQueue(const std::string &reportJson)
        {
            std::error_code ec;
            std::filesystem::create_directories(CXP_REPORTS_DIR, ec);

            std::vector<std::filesystem::directory_entry> fileList;
            for (const auto &entry : std::filesystem::directory_iterator(CXP_REPORTS_DIR, ec))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                    fileList.push_back(entry);
            }

            if (fileList.size() >= m_config.maxQueuedReports)
            {
                std::sort(fileList.begin(), fileList.end(), [](const auto &a, const auto &b) {
                    return a.last_write_time() < b.last_write_time();
                });

                size_t purgeCount = (fileList.size() - m_config.maxQueuedReports) + 1;
                for (size_t i = 0; i < purgeCount && i < fileList.size(); ++i)
                {
                    std::filesystem::remove(fileList[i].path(), ec);
                    syslog(LOG_WARNING, "Queue capacity limit reached. Purged oldest offline report: %s", fileList[i].path().filename().string().c_str());
                }
            }

            std::string timeStr  = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            std::string fileName = std::string(CXP_REPORTS_DIR) + "report_" + timeStr + ".json";

            std::ofstream outFile(fileName);
            if (outFile.is_open())
            {
                outFile << reportJson;
                outFile.close();
                syslog(LOG_INFO, "Local report archived successfully in queue: %s", fileName.c_str());
            }
        }

        bool SendReportToRestAPI(const std::string &reportJson)
        {
            if (m_config.apiUrl.empty())
                return false;

            // Dinamic URL parsing driven by UCI configuration parameters
            Stream::Http::ProtocolType protocol = Stream::Http::ProtocolType::http;
            std::string port   = "80";
            std::string host   = m_config.apiUrl;
            std::string target = "/";

            // Deduce the port and protocol type from the string prefix
            size_t protoPos = host.find("://");
            if (protoPos != std::string::npos)
            {
                std::string protoStr = host.substr(0, protoPos);
                if (protoStr == "https")
                {
                    protocol = Stream::Http::ProtocolType::https;
                    port = "443";
                }
                host.erase(0, protoPos + 3);
            }

            size_t slashPos = host.find('/');
            if (slashPos != std::string::npos)
            {
                target = host.substr(slashPos);
                host = host.substr(0, slashPos);
            }

            boost::system::error_code ec;
            Stream::Http::HttpClient client(protocol, false);

            client.Connect(host, port, ec);
            if (ec)
            {
                syslog(LOG_ERR, "HTTP client connection failed to %s:%s. Error: %s", host.c_str(), port.c_str(), ec.message().c_str());
                return false;
            }

            boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::post, target, 11};
            req.set(boost::beast::http::field::host, host);
            req.set(boost::beast::http::field::user_agent, "OpenWrt-C++-Daemon/1.0.0");
            req.set(boost::beast::http::field::content_type, "application/json");
            req.set(boost::beast::http::field::authorization, "Bearer " + m_config.apiToken);
            req.body() = reportJson;
            req.prepare_payload();

            client.Write(std::move(req), ec);
            if (ec)
            {
                syslog(LOG_ERR, "HTTP write transmission payload failed: %s", ec.message().c_str());
                client.Close(ec);
                return false;
            }

            boost::beast::http::response_parser<boost::beast::http::string_body> res_parser;
            client.Read(res_parser, ec);
            if (ec)
            {
                syslog(LOG_ERR, "HTTP read network response handshake failed: %s", ec.message().c_str());
                client.Close(ec);
                return false;
            }

            // Verify if the HTTP status code is a success (2xx)
            auto status = res_parser.get().result();
            if (status != boost::beast::http::status::ok && status != boost::beast::http::status::accepted)
            {
                syslog(LOG_WARNING, "REST API endpoint rejected report with HTTP status code: %u", static_cast<unsigned int>(status));
                client.Close(ec);
                return false;
            }

            auto contentType = res_parser.get()[boost::beast::http::field::content_type];
            if (contentType.find("application/json") != std::string_view::npos)
            {
                uint64_t intervalUpdate = 0;
                if (ParseIntervalResponse(res_parser.get().body(), intervalUpdate))
                {
                    if (intervalUpdate != m_config.checkInterval && intervalUpdate > 0)
                    {
                        syslog(LOG_NOTICE, "API requested telemetry check interval update to: %llu seconds.", (unsigned long long)intervalUpdate);

                        m_config.checkInterval = intervalUpdate;

                        try
                        {
                            ConfigManager configMgr(CXP_DAEMON_CONF);
                            ExecErr::SEECode uciCode = configMgr.WriteCheckInterval(intervalUpdate);
                            if (uciCode.fail)
                                syslog(LOG_WARNING, "Unable to update new configuration interval in configuration file. %s", uciCode.message.c_str());
                        }
                        catch (const std::exception &e) { syslog(LOG_CRIT, "UCI critical exception caught on initialization: %s.", e.what()); }
                    }
                }
            }

            client.Close(ec);
            return true;
        }

    public:

        DaemonHandler() : m_signals(m_ioCtx)
        {
            openlog("sysdiagnostic-daemon", LOG_PID, LOG_DAEMON);
        }

        ~DaemonHandler()
        {
            closelog();
        }

        int Run()
        {
            syslog(LOG_INFO, "System Diagnostic Daemon initialized");

            m_signals.add(SIGINT);
            m_signals.add(SIGTERM);

            m_signals.async_wait([this](const boost::system::error_code &ec, int) {
                if (!ec)
                {
                    syslog(LOG_NOTICE, "Termination signal captured via Boost.Asio. Initializing graceful exit.");
                    m_ioCtx.stop();
                    m_running = false;
                    m_loopCv.notify_one();
                }
            });

            std::thread signalThread([this]() {
                m_ioCtx.run();
            });

            std::error_code fsEc;
            std::filesystem::create_directories(CXP_REPORTS_DIR, fsEc);

            try
            {
                ConfigManager configMgr(CXP_DAEMON_CONF);
                ExecErr::SEECode uciCode = configMgr.LoadConfiguration(m_config);
                if (uciCode.fail)
                {
                    syslog(LOG_WARNING, "Configuration package error or missing. Enforcing factory defaults.");
                    SetFactoryDefaults();
                }
            }
            catch (const std::exception &e)
            {
                syslog(LOG_CRIT, "UCI critical exception caught on initialization: %s. Process aborting.", e.what());
                m_ioCtx.stop();

                if (signalThread.joinable())
                    signalThread.join();

                return 1;
            }

            while (m_running)
            {
                ProcessPendingQueue();

                std::string currentReport;
                ExecErr::SEECode diagCode = SysUtils::RetrieveDeviceDiagnostic(m_config, currentReport);

                if (diagCode.fail)
                    syslog(LOG_CRIT, "Telemetry sweep critical error. Could not retrieve system hardware metrics.");
                else
                {
                    std::string healthStatus = ExtractHealthStatus(currentReport);
                    syslog(LOG_INFO, "Current device health diagnostic evaluation: [%s]", healthStatus.c_str());

                    if (m_config.apiUrl.empty())
                        SaveReportToQueue(currentReport);
                    else
                    {
                        if (!SendReportToRestAPI(currentReport)) {
                            syslog(LOG_WARNING, "REST API connection link failure. Telemetry redirected to flash memory queue.");
                            SaveReportToQueue(currentReport);
                        }
                    }
                }

                if (m_running) {
                    std::unique_lock<std::mutex> lock(m_loopMutex);
                    m_loopCv.wait_for(lock, std::chrono::seconds(m_config.checkInterval), [this] { return !m_running; });
                }
            }

            m_ioCtx.stop();
            if (signalThread.joinable())
                signalThread.join();

            syslog(LOG_INFO, "Clean shutdown completed successfully. Daemon resource context destroyed.");
            return 0;
        }
    };
}
//---------------------------------------------------------------------------------------------------------------------
#endif
