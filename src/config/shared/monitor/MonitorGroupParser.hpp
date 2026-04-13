#pragma once

#include "MonitorGroupRule.hpp"

#include <optional>
#include <string>

namespace Config {
    class CMonitorGroupParser {
      public:
        CMonitorGroupParser();

        bool                       parse(const std::string& value);
        const CMonitorGroupRule&   rule() const;
        std::optional<std::string> getError() const;

      private:
        bool parseMembers(const std::string& value);
        bool parseLayout(const std::string& value);
        bool parseLayoutEntry(const std::string& entry);
        bool parseDefaultWorkspace(const std::string& value);

        CMonitorGroupRule          m_rule;
        std::optional<std::string> m_error;
    };
};
