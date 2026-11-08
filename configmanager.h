#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H
//---------------------------------------------------------------------------------------------------------------------

#include <string>
#include <cstdint>
#include <string_view>

#include "sysutils.hpp"
//---------------------------------------------------------------------------------------------------------------------

struct uci_context;

namespace Utils
{
    class ConfigManager
    {
    private:

        struct uci_context *m_uciCtx;
        const std::string m_packageName{"diagnostic"};

        bool GetOptionString(const std::string &section, const std::string &option, std::string &outValue);

    public:

        ConfigManager(std::string_view packageName);
        ~ConfigManager();

        ConfigManager(const ConfigManager &) = delete;
        ConfigManager &operator=(const ConfigManager &) = delete;

        /**
         * @brief Loads the full telemetry configurations from /etc/config/diagnostic into memory.
         */
        ExecErr::SEECode LoadConfiguration(DiagnosticConfig &config);

        /**
         * @brief Overwrites and commits the check_interval value into the schedule section.
         */
        ExecErr::SEECode WriteCheckInterval(uint64_t newIntervalSeconds);
    };
}
//---------------------------------------------------------------------------------------------------------------------
#endif
