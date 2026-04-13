#include "MonitorGroupParser.hpp"

#include "../../../debug/log/Logger.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>

#include <algorithm>
#include <cstring>

using namespace Config;
using namespace Hyprutils::String;

CMonitorGroupParser::CMonitorGroupParser() = default;

const CMonitorGroupRule& CMonitorGroupParser::rule() const {
    return m_rule;
}

std::optional<std::string> CMonitorGroupParser::getError() const {
    return m_error;
}

// Strip a single layer of surrounding `[` `]` brackets. Returns the trimmed inner
// string or nullopt if the input is not bracket-wrapped.
static std::optional<std::string> unwrapBrackets(const std::string& in) {
    const auto trimmed = std::string{trim(in)};
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
        return std::nullopt;
    return trimmed.substr(1, trimmed.size() - 2);
}

bool CMonitorGroupParser::parse(const std::string& value) {
    m_error.reset();
    m_rule = CMonitorGroupRule{};

    // Split top-level by comma, but NOT commas inside square brackets.
    std::vector<std::string> tokens;
    {
        std::string cur;
        int         depth = 0;
        for (char c : value) {
            if (c == '[')
                ++depth;
            else if (c == ']')
                --depth;
            if (c == ',' && depth == 0) {
                tokens.emplace_back(trim(cur));
                cur.clear();
                continue;
            }
            cur += c;
        }
        if (!cur.empty())
            tokens.emplace_back(trim(cur));
    }

    if (tokens.empty()) {
        m_error = "monitor_group: empty value";
        return false;
    }

    m_rule.m_name = std::string{trim(tokens[0])};
    if (m_rule.m_name.empty()) {
        m_error = "monitor_group: missing name";
        return false;
    }

    bool sawMembers = false;
    for (size_t i = 1; i < tokens.size(); ++i) {
        const auto& tok      = tokens[i];
        auto        colonPos = tok.find(':');
        if (colonPos == std::string::npos) {
            m_error = "monitor_group: expected key:value in `" + tok + "`";
            return false;
        }
        auto key = std::string{trim(tok.substr(0, colonPos))};
        auto val = std::string{trim(tok.substr(colonPos + 1))};

        if (key == "members") {
            if (!parseMembers(val))
                return false;
            sawMembers = true;
        } else if (key == "layout") {
            if (!parseLayout(val))
                return false;
        } else if (key == "default_workspace") {
            if (!parseDefaultWorkspace(val))
                return false;
        } else {
            m_error = "monitor_group: unknown key `" + key + "`";
            return false;
        }
    }

    if (!sawMembers) {
        m_error = "monitor_group: `members:[...]` is required";
        return false;
    }

    if (m_rule.m_members.size() < 2) {
        m_error = "monitor_group: at least 2 members are required";
        return false;
    }

    // Duplicate-member check.
    auto sorted = m_rule.m_members;
    std::ranges::sort(sorted);
    if (std::ranges::adjacent_find(sorted) != sorted.end()) {
        m_error = "monitor_group: duplicate member in `members`";
        return false;
    }

    return true;
}

bool CMonitorGroupParser::parseMembers(const std::string& value) {
    auto inner = unwrapBrackets(value);
    if (!inner) {
        m_error = "monitor_group: expected `[...]` after `members:`";
        return false;
    }
    CVarList2 list(std::string_view{*inner}, 0, ';');
    for (size_t i = 0; i < list.size(); ++i) {
        auto name = std::string{trim(std::string{list[i]})};
        if (name.empty()) {
            m_error = "monitor_group: empty member name in `members`";
            return false;
        }
        m_rule.m_members.push_back(std::move(name));
    }
    return true;
}

bool CMonitorGroupParser::parseLayout(const std::string& value) {
    auto inner = unwrapBrackets(value);
    if (!inner) {
        m_error = "monitor_group: expected `[...]` after `layout:`";
        return false;
    }
    CVarList2 list(std::string_view{*inner}, 0, ';');
    for (size_t i = 0; i < list.size(); ++i) {
        auto entry = std::string{trim(std::string{list[i]})};
        if (entry.empty())
            continue;
        if (!parseLayoutEntry(entry))
            return false;
    }
    return true;
}

bool CMonitorGroupParser::parseLayoutEntry(const std::string& entry) {
    // Accepted forms:
    //   <name>@<x>,<y>                  (absolute)
    //   <name>@right-of <other>         (relative)
    //   <name>@left-of  <other>
    //   <name>@above    <other>
    //   <name>@below    <other>

    auto atPos = entry.find('@');
    if (atPos == std::string::npos) {
        m_error = "monitor_group: layout entry `" + entry + "` missing `@`";
        return false;
    }
    SMonitorGroupLayoutEntry e;
    e.member = std::string{trim(entry.substr(0, atPos))};
    if (e.member.empty()) {
        m_error = "monitor_group: layout entry is missing a member name";
        return false;
    }

    auto rhs = std::string{trim(entry.substr(atPos + 1))};
    if (rhs.empty()) {
        m_error = "monitor_group: layout entry `" + entry + "` has no position";
        return false;
    }

    struct SRel {
        const char*                         keyword;
        SMonitorGroupLayoutEntry::eRelation rel;
    };
    static const SRel RELS[] = {
        {"right-of", SMonitorGroupLayoutEntry::REL_RIGHT_OF},
        {"left-of", SMonitorGroupLayoutEntry::REL_LEFT_OF},
        {"above", SMonitorGroupLayoutEntry::REL_ABOVE},
        {"below", SMonitorGroupLayoutEntry::REL_BELOW},
    };
    for (const auto& r : RELS) {
        const auto kwlen = std::strlen(r.keyword);
        if (rhs.size() > kwlen && rhs.compare(0, kwlen, r.keyword) == 0 && (rhs[kwlen] == ' ' || rhs[kwlen] == '\t')) {
            e.relation = r.rel;
            e.anchor   = std::string{trim(rhs.substr(kwlen))};
            if (e.anchor.empty()) {
                m_error = "monitor_group: layout entry `" + entry + "` missing anchor after `" + r.keyword + "`";
                return false;
            }
            m_rule.m_layout.push_back(std::move(e));
            return true;
        }
    }

    // Absolute `x,y`.
    auto commaPos = rhs.find(',');
    if (commaPos == std::string::npos) {
        m_error = "monitor_group: layout entry `" + entry + "` is not `x,y` or `<relation> <anchor>`";
        return false;
    }
    auto xs = std::string{trim(rhs.substr(0, commaPos))};
    auto ys = std::string{trim(rhs.substr(commaPos + 1))};
    auto x  = strToNumber<double>(xs);
    auto y  = strToNumber<double>(ys);
    if (!x || !y) {
        m_error = "monitor_group: layout entry `" + entry + "` has non-numeric coordinates";
        return false;
    }
    e.relation = SMonitorGroupLayoutEntry::REL_ABSOLUTE;
    e.offset   = Vector2D{x.value(), y.value()};
    m_rule.m_layout.push_back(std::move(e));
    return true;
}

bool CMonitorGroupParser::parseDefaultWorkspace(const std::string& value) {
    if (value.empty()) {
        m_error = "monitor_group: empty default_workspace";
        return false;
    }
    m_rule.m_defaultWorkspace = value;
    return true;
}
