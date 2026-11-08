#include <uci.h>
#include <cstdlib>
#include "configmanager.h"
//---------------------------------------------------------------------------------------------------------------------

namespace Utils
{
    ConfigManager::ConfigManager(std::string_view packageName) : m_uciCtx(nullptr), m_packageName(packageName)
    {
        m_uciCtx = uci_alloc_context();
        if (!m_uciCtx)
            EXECERR_THROW(ENOMEM, ExecErr::ErrorFamily::StdLib(), "Failed to allocate UCI context - Out of memory");
    }

    ConfigManager::~ConfigManager()
    {
        if (m_uciCtx)
            uci_free_context(m_uciCtx);
    }

    bool ConfigManager::GetOptionString(const std::string &section, const std::string &option, std::string &outValue)
    {
        struct uci_ptr ptr = {};

        outValue.clear();
        // Format path matching package.section.option
        std::string lookupPath = m_packageName + "." + section + "." + option;

        // uci_lookup_ptr modifies the string input internally during tokenization
        char *rawPath = const_cast<char *>(lookupPath.c_str());

        if (uci_lookup_ptr(m_uciCtx, &ptr, rawPath, true) != UCI_OK)
            return false;

        // Cascade pointers validation to secure against Segmentation Faults
        if (ptr.p && ptr.s && ptr.o && ptr.o->v.string)
        {
            outValue = ptr.o->v.string;
            return true;
        }

        return false;
    }

    ExecErr::SEECode ConfigManager::LoadConfiguration(DiagnosticConfig &config)
    {
        struct uci_package *pkg = nullptr;

        // Load the 'diagnostic' package into memory
        int res = uci_load(m_uciCtx, m_packageName.c_str(), &pkg);
        if (res != UCI_OK)
            return EXECERR_EECODE(true, res, ExecErr::ErrorFamily::StdLib(), "Failed to load diagnostic file");

        // Temporary buffers for safe extraction
        std::string valueStr;

        // 1. Section: 'system' -> option 'mac_interface'
        if (!GetOptionString("system", "mac_interface", valueStr))
            config.macInterface = "br-lan";
        else if (!valueStr.empty())
            config.macInterface = valueStr;

        // 2. Section: 'dns_test' -> option 'host'
        if (GetOptionString("dns_test", "host", valueStr) && !valueStr.empty())
            config.dnsHost = valueStr;

        // 3. Section: 'latency_test' -> options 'host' & 'port'
        if (GetOptionString("latency_test", "host", valueStr) && !valueStr.empty())
            config.latencyHost = valueStr;

        if (GetOptionString("latency_test", "port", valueStr) && !valueStr.empty())
            config.latencyPort = static_cast<uint16_t>(std::strtoul(valueStr.c_str(), nullptr, 10));

        // 4. Section: 'api' -> options 'url' & 'token'
        if (GetOptionString("api", "url", valueStr) && !valueStr.empty())
            config.apiUrl = valueStr;

        if (GetOptionString("api", "token", valueStr) && !valueStr.empty())
            config.apiToken = valueStr;

        // 5. Section: 'schedule' -> options 'check_interval' & 'max_queued_reports'
        if (GetOptionString("schedule", "check_interval", valueStr) && !valueStr.empty())
            config.checkInterval = std::strtoull(valueStr.c_str(), nullptr, 10);

        if (GetOptionString("schedule", "max_queued_reports", valueStr) && !valueStr.empty())
            config.maxQueuedReports = std::strtoull(valueStr.c_str(), nullptr, 10);

        // Unload package to free runtime UCI tracking buffers
        uci_unload(m_uciCtx, pkg);

        return ExecErr::SEECode();
    }

    ExecErr::SEECode ConfigManager::WriteCheckInterval(uint64_t newIntervalSeconds)
    {
        struct uci_package *pkg = nullptr;
        int res = uci_load(m_uciCtx, m_packageName.c_str(), &pkg);
        if (res != UCI_OK)
            return EXECERR_EECODE(true, res, ExecErr::ErrorFamily::StdLib(), "Failed to open config file for write");

        struct uci_ptr ptr = {};
        std::string pathStr = m_packageName + ".schedule.check_interval";
        std::string valueStr = std::to_string(newIntervalSeconds);

        // Prepare the lookup pointer path
        char *rawPath = const_cast<char *>(pathStr.c_str());
        if (uci_lookup_ptr(m_uciCtx, &ptr, rawPath, true) != UCI_OK)
        {
            uci_unload(m_uciCtx, pkg);
            return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Failed to lookup check_interval pointer");
        }

        // Overwrite option value
        ptr.value = valueStr.c_str();
        if (uci_set(m_uciCtx, &ptr) != UCI_OK)
        {
            uci_unload(m_uciCtx, pkg);
            return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Failed to set new interval value in UCI memory");
        }

        // Commit changes physically back to flash memory disk (/etc/config/diagnostic)
        if (uci_commit(m_uciCtx, &pkg, false) != UCI_OK)
        {
            uci_unload(m_uciCtx, pkg);
            return EXECERR_EECODE(true, 1, ExecErr::ErrorFamily::Logic(), "Failed to commit UCI changes to disk");
        }

        uci_unload(m_uciCtx, pkg);
        return ExecErr::SEECode();
    }
}
