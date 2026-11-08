#ifndef SYS_UTILS_H
#define SYS_UTILS_H
//---------------------------------------------------------------------------------------------------------------------

#include <map>
#include <string>
#include <fcntl.h>
#include <cstdint>
#include <fstream>
#include <netdb.h>
#include <unistd.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <algorithm>
#include <filesystem>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string_view>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>

#include "exerrdef.hpp"
#include "ansistrutils.hpp"
//---------------------------------------------------------------------------------------------------------------------

namespace Utils
{
    struct ServiceInfo
    {
        bool running{false};
        int32_t pid{-1};
    };

    struct NetworkInterface
    {
        std::string status;  // "UP" or "DOWN"
        std::string ipv4;
        std::string ipv6;
        std::string gateway;
    };

    struct VpnTunnel
    {
        std::string type;    // "OpenVPN" or "WireGuard"
        std::string ip;
    };

    struct DiagnosticConfig
    {
        std::string macInterface{"br-lan"};
        std::string dnsHost;
        std::string latencyHost;
        uint16_t latencyPort{443};
        std::string apiUrl;
        std::string apiToken;
        uint64_t checkInterval{60};
        uint64_t maxQueuedReports{10};
    };

    class SysUtils
    {
    private:

        /**
        * @brief Parses /proc/net/route to find the default gateway for a specific interface.
        */
        static void ParseDefaultGateway(std::map<std::string, NetworkInterface> &interfacesMap)
        {
            std::ifstream routeFile("/proc/net/route");
            if (!routeFile.is_open())
                return;

            std::string line;
            std::string iface, dest, gw;
            std::getline(routeFile, line); // Skip the table header

            while (routeFile >> iface >> dest >> gw)
            {
                // "00000000" represents the default route (0.0.0.0)
                if (dest == "00000000")
                {
                    auto it = interfacesMap.find(iface);
                    if (it != interfacesMap.end())
                    {
                        // Convert hex Little Endian string to network byte order
                        uint32_t gwHex = std::stoul(gw, nullptr, 16);
                        struct in_addr addr;
                        addr.s_addr = gwHex;

                        char ipStr[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN))
                            it->second.gateway = ipStr;
                    }
                }

                std::getline(routeFile, line); // Discard any remaining columns on the current line
            }
        }

    public:

        /// ################### Block 1: Fixed Hardware / OS Metadata ###################
        /**
         * @brief Collects the system hostname using POSIX gethostname
         */
        static ExecErr::SEECode CollectHostname(std::string& hostname)
        {
            char buffer[256];

            if (gethostname(buffer, sizeof(buffer)) != 0)
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to get hostname via POSIX");

            hostname = buffer;
            return ExecErr::SEECode();
        }

        /**
         * @brief Collects the OpenWrt board name from the system temporary metadata
         */
        static ExecErr::SEECode CollectBoardName(std::string &boardName)
        {
            std::ifstream file("/tmp/sysinfo/board_name");

            if (!file.is_open())
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Could not open /tmp/sysinfo/board_name");
            if (!std::getline(file, boardName))
                return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Read error or empty file in board_name");

            return ExecErr::SEECode();
        }

        /**
         * @brief Collects the OpenWrt board name from the system temporary metadata
         */
        static ExecErr::SEECode CollectModelName(std::string &modelName)
        {
            std::ifstream file("/tmp/sysinfo/model");

            if (!file.is_open())
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Could not open /tmp/sysinfo/model_name");
            if (!std::getline(file, modelName))
                return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Read error or empty file in model_name");

            return ExecErr::SEECode();
        }

        /**
         * @brief Collects and parses the OpenWrt release description version
         */
        static ExecErr::SEECode CollectOpenWrtVersion(std::string &version)
        {
            std::ifstream file("/etc/openwrt_release");

            if (!file.is_open())
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Could not open /etc/openwrt_release");

            std::string line;
            std::string releasePrefix = "DISTRIB_DESCRIPTION=";

            while (std::getline(file, line))
            {
                if (line.rfind(releasePrefix, 0) == 0)
                {
                    std::string rawDesc = line.substr(releasePrefix.length());

                    if (rawDesc.size() >= 2 && rawDesc.front() == '\'' && rawDesc.back() == '\'')
                        version = rawDesc.substr(1, rawDesc.size() - 2);
                    else if (rawDesc.size() >= 2 && rawDesc.front() == '"' && rawDesc.back() == '"')
                        version = rawDesc.substr(1, rawDesc.size() - 2);
                    else
                        version = rawDesc;

                    return ExecErr::SEECode();
                }
            }

            return EXECERR_EECODE(true, 2, ExecErr::ErrorFamily::Logic(), "DISTRIB_DESCRIPTION key not found in release file");
        }

        /**
         * @brief Collects and mac from desired interface
         */
        static ExecErr::SEECode CollectMacInterface(std::string_view interfaceName, std::string &macAddress)
        {
            std::string path = "/sys/class/net/" + std::string(interfaceName) + "/address";
            std::ifstream file(path);

            if (!file.is_open())
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Could not open interface address file in sysfs");
            if (!std::getline(file, macAddress))
                return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Read error or empty address file");

            return ExecErr::SEECode();
        }

        /// ################### Block 2: Performance and System Resources ###################

        /**
         * @brief Collects uptime
         */
        static ExecErr::SEECode CollectUptime(uint64_t &uptimeSeconds)
        {
            std::ifstream file("/proc/uptime");
            if (!file.is_open())
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Could not open /proc/uptime");

            double rawUptime = 0.0;
            if (!(file >> rawUptime))
                return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Failed to parse uptime value from file");

            uptimeSeconds = static_cast<uint64_t>(rawUptime);
            return ExecErr::SEECode();
        }

        /**
         * @brief Collects the system load average and returns the combined decimal mean of 1, 5, and 15 minutes.
         */
        static ExecErr::SEECode CollectLoadAverage(double &loadAverageValue)
        {
            double samples[3];

            // getloadavg returns the number of samples acquired, or -1 on error
            if (getloadavg(samples, 3) != 3)
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to retrieve system load average via getloadavg");

            // Calculate the pure decimal average of the three intervals
            loadAverageValue = (samples[0] + samples[1] + samples[2]) / 3.0;

            return ExecErr::SEECode();
        }

        /**
         * @brief Collects both total and free system memory in Kilobytes.
         */
        static ExecErr::SEECode CollectMemoryInfo(uint64_t &totalMemKb, uint64_t &freeMemKb)
        {
            struct sysinfo si = {};

            if (sysinfo(&si) != 0)
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to retrieve system memory info via sysinfo");

            uint64_t memUnit = static_cast<uint64_t>(si.mem_unit);
            totalMemKb       = (static_cast<uint64_t>(si.totalram) * memUnit) / 1024;
            freeMemKb        = (static_cast<uint64_t>(si.freeram) * memUnit) / 1024;

            return ExecErr::SEECode();
        }

        /**
         * @brief Collects the free storage space available on a given mount path in Kilobytes.
         */
        static ExecErr::SEECode CollectStorageFree(const char *mountPath, uint64_t &freeSpaceKb)
        {
            struct statvfs vfs = {};

            if (!mountPath)
                return EXECERR_EECODE(true, -1, ExecErr::ErrorFamily::Logic(), "Invalid parameter mountPath");
            if (statvfs(mountPath, &vfs) != 0)
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to retrieve storage info via statvfs");

            uint64_t freeBytes = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
            freeSpaceKb = freeBytes / 1024;

            return ExecErr::SEECode();
        }

        /// ################### Block 3: Networks and Services Orchestration ###################

        /**
        * @brief Collects all network interfaces, their operational status, addresses, and default gateway.
        */
        static ExecErr::SEECode CollectInterfaces(std::map<std::string, NetworkInterface> &interfacesMap)
        {
            struct ifaddrs *ifaddr = nullptr;

            if (getifaddrs(&ifaddr) == -1)
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to retrieve network interfaces via getifaddrs");

            interfacesMap.clear();

            for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
            {
                if (!ifa->ifa_addr)
                    continue;

                std::string ifaName(ifa->ifa_name);
                NetworkInterface &net = interfacesMap[ifaName];

                net.status = (ifa->ifa_flags & IFF_UP) ? "UP" : "DOWN";

                char host[NI_MAXHOST];
                if (ifa->ifa_addr->sa_family == AF_INET)
                {
                    if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0)
                        net.ipv4 = host;
                }
                else if (ifa->ifa_addr->sa_family == AF_INET6)
                {
                    if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0)
                        net.ipv6 = host;
                }
            }

            freeifaddrs(ifaddr);
            ParseDefaultGateway(interfacesMap); // Inject gateway information into the map

            return ExecErr::SEECode();
        }

        /**
        * @brief Collects active VPN tunnels (OpenVPN/WireGuard) and their local IPs.
        */
        static ExecErr::SEECode CollectVpnTunnels(std::map<std::string, VpnTunnel> &vpnMap)
        {
            struct ifaddrs *ifaddr = nullptr;

            vpnMap.clear();
            if (getifaddrs(&ifaddr) == -1)
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to retrieve VPN interfaces via getifaddrs");

            for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
            {
                if (!ifa->ifa_addr || !ifa->ifa_name)
                    continue;

                std::string ifaName(ifa->ifa_name);
                std::string type;

                if (ifaName.rfind("tun", 0) == 0)
                    type = "OpenVPN";
                else if (ifaName.rfind("wg", 0) == 0)
                    type = "WireGuard";
                else
                    continue; // Not a targeted VPN tunnel

                VpnTunnel &vpn = vpnMap[ifaName];
                vpn.type = type;

                char host[NI_MAXHOST];
                if (ifa->ifa_addr->sa_family == AF_INET)
                {
                    if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0)
                        vpn.ip = host;
                }
                else if (ifa->ifa_addr->sa_family == AF_INET6)
                {
                    if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0)
                        vpn.ip = host;
                }
            }

            freeifaddrs(ifaddr);
            return ExecErr::SEECode();
        }

        /**
        * @brief Collects active services and their PIDs by scanning /proc using C++17 filesystem.
        */
        static ExecErr::SEECode CollectServicesStatus(std::map<std::string, ServiceInfo> &servicesMap)
        {
            std::error_code ec;

            servicesMap.clear();
            if (!std::filesystem::exists("/proc", ec))
                return EXECERR_EECODE(true, ec.value(), ExecErr::ErrorFamily::StdLib(), "The /proc directory does not exist");

            for (const auto &entry : std::filesystem::directory_iterator("/proc", ec))
            {
                if (!entry.is_directory())
                    continue;

                std::string folderName = entry.path().filename().string();

                // Validate if the folder name is numeric (a process ID)
                bool isPid = !folderName.empty() && std::all_of(folderName.begin(), folderName.end(), ::isdigit);
                if (!isPid)
                    continue;

                std::ifstream commFile(entry.path() / "comm");
                std::string processName;

                if (commFile && std::getline(commFile, processName))
                {
                    // Clean up any trailing whitespaces
                    while (!processName.empty() && ::isspace(processName.back()))
                        processName.pop_back();

                    ServiceInfo &info = servicesMap[processName];
                    info.running      = true;
                    info.pid          = std::stoi(folderName);
                }
            }

            if (ec)
                return EXECERR_EECODE(true, ec.value(), ExecErr::ErrorFamily::StdLib(), "Error iterating /proc directory");

            return ExecErr::SEECode();
        }

        /// ################### Block 4: Active Network Connectivity Tests ###################


        /**
        * @brief Resolves a domain name to its corresponding IP address string using POSIX getaddrinfo.
        */
        static ExecErr::SEECode TestDnsResolution(const char *domain, std::string &resolvedIp)
        {
            struct addrinfo hints   = {};
            struct addrinfo *result = nullptr;

            hints.ai_family   = AF_UNSPEC;   // Allow both IPv4 and IPv6
            hints.ai_socktype = SOCK_STREAM; // Stream socket (TCP)

            if (!domain)
                return EXECERR_EECODE(true, -1, ExecErr::ErrorFamily::Logic(), "Invalid parameter domain");

            int res = getaddrinfo(domain, nullptr, &hints, &result);
            if (res != 0)
                return EXECERR_EECODE(true, res, ExecErr::ErrorFamily::StdLib(), gai_strerror(res));

            char ipStr[INET6_ADDRSTRLEN];
            void *addr = nullptr;

            // Extract the first resolved address entry
            if (result->ai_family == AF_INET)
            {
                struct sockaddr_in *ipv4 = reinterpret_cast<struct sockaddr_in *>(result->ai_addr);
                addr = &(ipv4->sin_addr);
            }
            else if (result->ai_family == AF_INET6)
            {
                struct sockaddr_in6 *ipv6 = reinterpret_cast<struct sockaddr_in6 *>(result->ai_addr);
                addr = &(ipv6->sin6_addr);
            }

            if (addr && inet_ntop(result->ai_family, addr, ipStr, sizeof(ipStr)))
                resolvedIp = ipStr;

            freeaddrinfo(result);

            if (resolvedIp.empty())
                return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Failed to format resolved IP address");

            return ExecErr::SEECode();
        }

        /**
         * @brief Measures the exact TCP handshake latency to a given host and port with double floating-point precision.
        */
        static ExecErr::SEECode TestTcpLatency(const char *host, uint16_t port, double &latencyMs)
        {
            if (!host)
                return EXECERR_EECODE(true, -1, ExecErr::ErrorFamily::Logic(), "Invalid parameter host");

            struct addrinfo hints   = {};
            struct addrinfo *result = nullptr;
            hints.ai_family         = AF_UNSPEC;
            hints.ai_socktype       = SOCK_STREAM;
            std::string portStr     = std::to_string(port);


            int res = getaddrinfo(host, portStr.c_str(), &hints, &result);
            if (res != 0)
                return EXECERR_EECODE(true, res, ExecErr::ErrorFamily::StdLib(), gai_strerror(res));

            int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (sock < 0)
            {
                freeaddrinfo(result);
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Failed to create socket for latency test");
            }

            // Set socket to non-blocking mode to control the timeout strictly
            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);

            // Start the high resolution clock right before attempting connection
            auto startTime = std::chrono::high_resolution_clock::now();

            int connRes = connect(sock, result->ai_addr, result->ai_addrlen);
            if (connRes < 0 && errno != EINPROGRESS)
            {
                close(sock);
                freeaddrinfo(result);
                return EXECERR_EECODE(true, errno, ExecErr::ErrorFamily::StdLib(), "Connection immediate failure");
            }

            // Use select to wait for connection with a strict timeout (e.g., 5 seconds)
            struct timeval timeout = {};
            timeout.tv_sec  = 5;
            timeout.tv_usec = 0;

            fd_set writeFds;
            FD_ZERO(&writeFds);
            FD_SET(sock, &writeFds);

            int selectRes = select(sock + 1, nullptr, &writeFds, nullptr, &timeout);

            // Stop the clock immediately after select returns
            auto endTime = std::chrono::high_resolution_clock::now();

            if (selectRes <= 0)
            {
                close(sock);
                freeaddrinfo(result);
                return EXECERR_EECODE(true, (selectRes == 0) ? ETIMEDOUT : errno, ExecErr::ErrorFamily::StdLib(),
                                      (selectRes == 0) ? "TCP connection timeout reached" : "Select error during connection");
            }

            // Verify if the connection was actually successful or has errors
            int soError = 0;
            socklen_t len = sizeof(soError);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soError, &len) < 0 || soError != 0)
            {
                close(sock);
                freeaddrinfo(result);
                return EXECERR_EECODE(true, soError ? soError : errno, ExecErr::ErrorFamily::StdLib(), "Connection refused or failed at destination");
            }

            close(sock);
            freeaddrinfo(result);

            // Calculate exact duration in milliseconds with floating-point precision
            std::chrono::duration<double, std::milli> duration = endTime - startTime;
            latencyMs = duration.count();

            return ExecErr::SEECode();
        }

        /**
         * @brief Collects all system diagnostics driven by UCI configuration and formats a JSON string.
         */
        static ExecErr::SEECode RetrieveDeviceDiagnostic(const DiagnosticConfig &config, std::string &reportStr)
        {
            std::string hostname, boardName, modelName, version, macAddress, resolvedIp;
            uint64_t totalMem = 0, freeMem = 0, freeTmp = 0, freeOverlay = 0, uptime = 0;
            double loadAvg = 0.0, latencyMs = 0.0;

            std::map<std::string, NetworkInterface> interfaces;
            std::map<std::string, VpnTunnel> vpnTunnels;
            std::map<std::string, ServiceInfo> allServices;

            // Execute core system collections
            CollectHostname(hostname);
            CollectBoardName(boardName);
            CollectModelName(modelName);
            CollectOpenWrtVersion(version);
            CollectUptime(uptime);
            CollectLoadAverage(loadAvg);
            CollectMemoryInfo(totalMem, freeMem);
            CollectStorageFree("/tmp", freeTmp);
            CollectStorageFree("/overlay", freeOverlay);
            CollectMacInterface(config.macInterface.c_str(), macAddress);
            CollectInterfaces(interfaces);
            CollectVpnTunnels(vpnTunnels);
            CollectServicesStatus(allServices);

            // Extract the state of our targeted services from the global scan
            bool dnsmasqRunning  = allServices["dnsmasq"].running;
            bool dropbearRunning = allServices["dropbear"].running;
            bool firewallRunning = false;

            // Firewall. Because in new systems is not a daemon with PID we need to check subsystem filter file
            std::ifstream fwCheck("/proc/net/ip_tables_names");
            if (fwCheck.is_open()) {
                firewallRunning  = true;
                fwCheck.close();  // IS RAII BUT FREE RESOURCES NOW.
            }

            // Execute network connectivity tests using the hostnames injected from UCI config
            bool dnsSuccess = TestDnsResolution(config.dnsHost.c_str(), resolvedIp).fail == false;
            bool latencySuccess = TestTcpLatency(config.latencyHost.c_str(), config.latencyPort, latencyMs).fail == false;

            // --- Health Evaluation Logic (Python Spec Match) ---
            std::string healthStatus = "ok";
            std::vector<std::string> reasons;

            if ((totalMem > 0 && (static_cast<double>(freeMem) / totalMem) < 0.05) || (loadAvg > 4.0))
            {
                healthStatus = "critical";
                if (loadAvg > 4.0)
                    reasons.push_back("high_cpu_load");
                if (totalMem > 0 && (static_cast<double>(freeMem) / totalMem) < 0.05)
                    reasons.push_back("out_of_memory");
            }

            if (healthStatus != "critical")
            {
                if (!dnsSuccess)
                    reasons.push_back("dns_test_failed");
                if (!latencySuccess)
                    reasons.push_back("latency_test_failed");
                if (!dnsmasqRunning)
                    reasons.push_back("dnsmasq_service_down");
                if (!dropbearRunning)
                    reasons.push_back("dropbear_service_down");
                if (!firewallRunning)
                    reasons.push_back("firewall_service_down");

                if (!reasons.empty())
                    healthStatus = "degraded";
            }

            // Get standard ISO timestamp using your wrapper class
            std::string timeStamp = StrUtils::CAnsiStr::TimePointToUTCString(std::chrono::system_clock::now(), "%FT%TZ");

            // --- Manual JSON String Construction ---
            reportStr.clear();
            reportStr.reserve(2048);

            reportStr += "{\n";
            reportStr += "  \"agent_version\": \"1.0.0\",\n";
            reportStr += "  \"generated_at\": \"" + timeStamp + "\",\n";
            reportStr += "  \"check_interval\": " + std::to_string(config.checkInterval) + ",\n"; // Root interval field added

            // System Sub-object
            reportStr += "  \"system\": {\n";
            reportStr += "    \"hostname\": \"" + hostname + "\",\n";
            reportStr += "    \"model\": \"" + modelName + "\",\n";
            reportStr += "    \"board\": \"" + boardName + "\",\n";
            reportStr += "    \"version\": \"" + version + "\",\n";
            reportStr += "    \"uptime_seconds\": " + std::to_string(uptime) + ",\n";
            reportStr += "    \"load_average_summary\": " + std::to_string(static_cast<int32_t>(loadAvg)) + ",\n";
            reportStr += "    \"mac_principal\": \"" + macAddress + "\"\n";
            reportStr += "  },\n";

            // Resources Sub-object
            reportStr += "  \"resources\": {\n";
            reportStr += "    \"free_memory_kb\": " + std::to_string(freeMem) + ",\n";
            reportStr += "    \"free_overlay_kb\": " + std::to_string(freeOverlay) + ",\n";
            reportStr += "    \"free_tmp_kb\": " + std::to_string(freeTmp) + "\n";
            reportStr += "  },\n";

            // Network Interfaces Array
            reportStr += "  \"network_interfaces\": [\n";
            size_t ifaceIdx = 0;
            for (const auto &[name, net] : interfaces)
            {
                reportStr += "    {\n";
                reportStr += "      \"interface\": \"" + name + "\",\n";
                reportStr += "      \"status\": \"" + net.status + "\",\n";
                reportStr += "      \"protocol\": \"native\",\n";
                reportStr += "      \"ipv4\": " + (net.ipv4.empty() ? "null" : "\"" + net.ipv4 + "\"") + ",\n";
                reportStr += "      \"ipv6\": " + (net.ipv6.empty() ? "null" : "\"" + net.ipv6 + "\"") + ",\n";
                reportStr += "      \"dns_server\": null,\n";
                reportStr += "      \"gateway\": \"" + (net.gateway.empty() ? "0.0.0.0" : net.gateway) + "\"\n";
                reportStr += "    }";
                if (++ifaceIdx < interfaces.size()) reportStr += ",";
                reportStr += "\n";
            }
            reportStr += "  ],\n";

            // Filtered Services Status (Strict business rules mapping)
            reportStr += "  \"services\": {\n";
            reportStr += "    \"dnsmasq\": \"" + std::string(dnsmasqRunning ? "OK" : "fail") + "\",\n";
            reportStr += "    \"dropbear\": \"" + std::string(dropbearRunning ? "OK" : "fail") + "\",\n";
            reportStr += "    \"firewall\": \"" + std::string(firewallRunning ? "OK" : "fail") + "\"\n"; // Mapped from fw4 status
            reportStr += "  },\n";

            // VPNs Tunnels Object
            if (vpnTunnels.empty())
            {
                reportStr += "  \"vpns\": null,\n";
            }
            else
            {
                reportStr += "  \"vpns\": {\n";
                size_t vpnIdx = 0;
                for (const auto &[name, vpn] : vpnTunnels)
                {
                    reportStr += "    \"" + name + "\": {\"type\": \"" + vpn.type + "\", \"ip\": \"" + vpn.ip + "\"}";
                    if (++vpnIdx < vpnTunnels.size()) reportStr += ",";
                    reportStr += "\n";
                }
                reportStr += "  },\n";
            }

            // Diagnostics Tests driven by configuration parameters
            reportStr += "  \"diagnostics\": {\n";
            reportStr += "    \"dns_test\": {\n";
            reportStr += "      \"status\": \"" + std::string(dnsSuccess ? "success" : "failed") + "\",\n";
            reportStr += "      \"host\": \"" + config.dnsHost + "\",\n";
            reportStr += "      \"resolved_ip\": " + (resolvedIp.empty() ? "null" : "\"" + resolvedIp + "\"") + "\n";
            reportStr += "    },\n";
            reportStr += "    \"latency_test\": {\n";
            reportStr += "      \"status\": \"" + std::string(latencySuccess ? "success" : "failed") + "\",\n";
            reportStr += "      \"host\": \"" + config.latencyHost + "\",\n";
            reportStr += "      \"latency_ms\": " + (latencySuccess ? std::to_string(static_cast<int32_t>(latencyMs)) : "-1") + "\n";
            reportStr += "    }\n";
            reportStr += "  },\n";

            // Health Status Final Object
            reportStr += "  \"health\": {\n";
            reportStr += "    \"status\": \"" + healthStatus + "\",\n";
            reportStr += "    \"reasons\": [";
            for (size_t i = 0; i < reasons.size(); ++i)
            {
                reportStr += "\"" + reasons[i] + "\"";
                if (i + 1 < reasons.size()) reportStr += ", ";
            }
            reportStr += "]\n";
            reportStr += "  }\n";

            reportStr += "}";

            return ExecErr::SEECode();
        }
    };
}
//---------------------------------------------------------------------------------------------------------------------
#endif
