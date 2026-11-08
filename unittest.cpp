
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "sysutils.hpp"
#include "configmanager.h"
//---------------------------------------------------------------------------------------------------------------------

class UnitTests
{
private:

    static constexpr std::string_view CXP_RESET   = "\033[0m";
    static constexpr std::string_view CXP_RED     = "\033[31m";
    static constexpr std::string_view CXP_GREEN   = "\033[32m";
    static constexpr std::string_view CXP_YELLOW  = "\033[33m";
    static constexpr std::string_view CXP_BLUE    = "\033[34m";
    static constexpr std::string_view CXP_MAGENTA = "\033[35m";
    static constexpr std::string_view CXP_CYAN    = "\033[36m";
    static constexpr std::string_view CXP_WHITE   = "\033[37m";

    template <typename... Args>
    void PrintData(Args&&... args) {
        (std::cout << ... << std::forward<Args>(args)) << std::endl;
    }

    void PrintResult(const ExecErr::SEECode &result)
    {
        if (result)
            PrintData("Message: ", result.message, " Error Code: ", result.code, " Family: ", result.name, "\nTEST RESULT: [",CXP_RED,"FAILED",CXP_RESET,"]\n");
        else
            PrintData("TEST RESULT: [",CXP_GREEN,"PASSED",CXP_RESET,"]\n");
    }

    std::string strData;
    uint64_t intData;
    double floatData;

public:

    ExecErr::SEECode CollectHostname()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectHostname(strData);

        if (ecode)
            return ecode;

        PrintData("HOSTNAME: ", strData);

        return {};
    }

    ExecErr::SEECode CollectBoardName()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectBoardName(strData);

        if (ecode)
            return ecode;

        PrintData("BOARD NAME: ", strData);

        return {};
    }

    ExecErr::SEECode CollectModelName()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectModelName(strData);

        if (ecode)
            return ecode;

        PrintData("MODEL NAME: ", strData);

        return {};
    }

    ExecErr::SEECode CollectOpenWrtVersion()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectOpenWrtVersion(strData);

        if (ecode)
            return ecode;

        PrintData("VERSION: ", strData);

        return {};
    }

    ExecErr::SEECode CollectUptime()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectUptime(intData);

        if (ecode)
            return ecode;

        PrintData("UPTIME: ", intData);

        return {};
    }

    ExecErr::SEECode CollectLoadAverage()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectLoadAverage(floatData);

        if (ecode)
            return ecode;

        PrintData("Load Average: ", floatData);

        return {};
    }

    ExecErr::SEECode CollectMemoryInfo()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);

        uint64_t totalMem = 0, freeMem = 0;
        ExecErr::SEECode ecode = Utils::SysUtils::CollectMemoryInfo(totalMem, freeMem);

        if (ecode)
            return ecode;

        PrintData("Fee RAM (KB): ", intData);

        return {};
    }

    ExecErr::SEECode CollectStorageFree()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);

        uint64_t tmpFree = 0, overlayFree = 0;
        ExecErr::SEECode ecodeTmp = Utils::SysUtils::CollectStorageFree("/tmp",tmpFree);
        ExecErr::SEECode ecodeOvl = Utils::SysUtils::CollectStorageFree("/overlay",overlayFree);

        PrintData("Free /tmp (KB): ", tmpFree);
        PrintData("Free /overlay (KB): ", overlayFree);

        if (ecodeTmp)
            return ecodeTmp;
        if (ecodeOvl)
            return ecodeOvl;

        return {};
    }

    ExecErr::SEECode CollectMacInterface()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        ExecErr::SEECode ecode = Utils::SysUtils::CollectMacInterface("br-lan",strData);

        for (const auto &interface : {"br-lan","eth0","enp0s3"}) {
             ecode = Utils::SysUtils::CollectMacInterface(interface,strData);
             if (!ecode) {
                 PrintData("MAC: ", strData);
                 return {};
             }
        }

        return ecode;
    }

    ExecErr::SEECode CollectInterfaces()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        std::map<std::string, Utils::NetworkInterface> interfaces;
        ExecErr::SEECode ecode = Utils::SysUtils::CollectInterfaces(interfaces);

        if (ecode)
            return ecode;

        for (const auto &interface : interfaces)
        {
             const Utils::NetworkInterface &iData = interface.second;

             PrintData("INTERFACE: ", interface.first, " STATUS: ", iData.status,
                       " IPV4: ", iData.ipv4 ,  " IPV6: ", iData.ipv6, " GATEWAY: ", iData.gateway);
        }

        return {};
    }

    ExecErr::SEECode CollectVpnTunnels()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        std::map<std::string, Utils::VpnTunnel> vpns;
        ExecErr::SEECode ecode = Utils::SysUtils::CollectVpnTunnels(vpns);

        if (ecode)
            return ecode;

        for (const auto &vpn : vpns)
        {
            const Utils::VpnTunnel &vData = vpn.second;

            PrintData("VPN: ", vpn.first, " TYPE: ", vData.type, " IP: ", vData.ip);
        }

        return {};
    }

    ExecErr::SEECode CollectServicesStatus()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);
        std::map<std::string, Utils::ServiceInfo> daemons;
        ExecErr::SEECode ecode = Utils::SysUtils::CollectServicesStatus(daemons);

        if (ecode)
            return ecode;

        for (const auto &daemon : daemons)
             PrintData("DAEMON: ", daemon.first, " PID: ", daemon.second.pid);

        return {};
    }

    ExecErr::SEECode TestDnsResolution()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);

        const char *domain = "nxtview.com";
        ExecErr::SEECode ecode = Utils::SysUtils::TestDnsResolution(domain,strData);

        if (ecode)
            return ecode;

        PrintData("DOMAIN: ", domain, " RESOLVED TO: ", strData);

        return {};
    }

    ExecErr::SEECode TestTcpLatency()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);

        const char *host = "nxtview.com";
        ExecErr::SEECode ecode = Utils::SysUtils::TestTcpLatency(host,443,floatData);

        if (ecode)
            return ecode;

        PrintData("DOMAIN: ", host, " LATENCY(ms): ", floatData);

        return {};
    }

    ExecErr::SEECode RetrieveDeviceDiagnostic()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);

        Utils::DiagnosticConfig config = {"br-lan","nxtview.com","nxtview.com",443,"http://tu-servidor-api.local","tu_bearer_token_secreto_aqui",120,10};
        ExecErr::SEECode ecode = Utils::SysUtils::RetrieveDeviceDiagnostic(config,strData);

        if (ecode)
            return ecode;

        PrintData("DIAGNOSTIC: ", strData);

        return {};
    }

    ExecErr::SEECode ManageConfiguration()
    {
        PrintData(CXP_WHITE,"TEST [",CXP_RESET,CXP_BLUE,__func__,CXP_RESET,CXP_WHITE,"]",CXP_RESET);

        Utils::DiagnosticConfig config;
        Utils::ConfigManager cmanager("sysdiagnostic");
        ExecErr::SEECode ecode = cmanager.LoadConfiguration(config);

        PrintData("##### LOADING DAEMON CONFIGURATION #####");
        if (ecode)
            return ecode;

        PrintData("INTERFACE: "   , config.macInterface  , " DNS HOST: "         , config.dnsHost         , " LATENCY HOST: ", config.latencyHost,
                  " LATENCY PORT:", config.latencyPort   , " URL API: "          ,config.apiUrl           , " API TOKEN: ", config.apiToken,
                  " INTERVAL: "   , config.checkInterval , " MAX QUEUE REPORTS: ", config.maxQueuedReports);

        PrintData("##### WRITING NEW INTERVAL CONFIGURATION IN DAEMON CONFIG FILE #####");
        ecode = cmanager.WriteCheckInterval(900);
        if (ecode)
            return ecode;

        PrintData("New interval configuration set successfully");

        return {};
    }

    void Execute()
    {
        ExecErr::SEECode result;
        using TestFunc = ExecErr::SEECode(UnitTests::*)();
        std::vector<TestFunc> tests
        {
             &UnitTests::CollectHostname,
             &UnitTests::CollectBoardName,
             &UnitTests::CollectModelName,
             &UnitTests::CollectOpenWrtVersion,
             &UnitTests::CollectUptime,
             &UnitTests::CollectLoadAverage,
             &UnitTests::CollectMemoryInfo,
             &UnitTests::CollectStorageFree,
             &UnitTests::CollectMacInterface,
             &UnitTests::CollectInterfaces,
             &UnitTests::CollectVpnTunnels,
             &UnitTests::CollectServicesStatus,
             &UnitTests::TestDnsResolution,
             &UnitTests::TestTcpLatency,
             &UnitTests::RetrieveDeviceDiagnostic,
             &UnitTests::ManageConfiguration
        };

        for (auto ptrFunc : tests) {
             result = (this->*ptrFunc)();
             PrintResult(result);
        }
    }
};

int main()
{
    try
    {
        UnitTests tests;

        tests.Execute();
    }
    catch (std::exception& e) {
        std::cout << "Exception: " <<  e.what() << std::endl;
    }

    return 0;
}
