#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../../../helpers/math/Math.hpp"

namespace Config {
    struct SMonitorGroupLayoutEntry {
        enum eRelation : uint8_t {
            REL_ABSOLUTE = 0,
            REL_RIGHT_OF,
            REL_LEFT_OF,
            REL_ABOVE,
            REL_BELOW,
        };

        std::string member;       // referenced physical monitor
        std::string anchor;       // relative-anchor member name; empty when REL_ABSOLUTE
        Vector2D    offset{0, 0}; // absolute offset; only used when relation == REL_ABSOLUTE
        eRelation   relation = REL_ABSOLUTE;
    };

    class CMonitorGroupRule {
      public:
        CMonitorGroupRule()  = default;
        ~CMonitorGroupRule() = default;

        std::string                           m_name;
        std::vector<std::string>              m_members;
        std::vector<SMonitorGroupLayoutEntry> m_layout;           // may be empty => use members' monitor = positions
        std::optional<std::string>            m_defaultWorkspace; // raw spec, e.g. "name:rdp" or "8"
    };
};
