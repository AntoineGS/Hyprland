#include "MonitorGroup.hpp"
#include "Monitor.hpp"

#include <algorithm>
#include <limits>

CMonitorGroup::CMonitorGroup(const Config::CMonitorGroupRule& rule) : m_rule(rule) {
    m_members.resize(m_rule.m_members.size());
}

const std::string& CMonitorGroup::name() const {
    return m_rule.m_name;
}

const Config::CMonitorGroupRule& CMonitorGroup::rule() const {
    return m_rule;
}

CMonitorGroup::eState CMonitorGroup::state() const {
    return m_state;
}

const std::vector<PHLMONITORREF>& CMonitorGroup::members() const {
    return m_members;
}

CBox CMonitorGroup::boundingBox() const {
    return m_bbox;
}

bool CMonitorGroup::memberByName(const std::string& physicalName) const {
    return std::ranges::find(m_rule.m_members, physicalName) != m_rule.m_members.end();
}

bool CMonitorGroup::onPhysicalConnect(const PHLMONITOR& physical) {
    if (!physical)
        return false;

    auto it = std::ranges::find(m_rule.m_members, physical->m_name);
    if (it == m_rule.m_members.end())
        return false;

    const auto idx = static_cast<size_t>(std::distance(m_rule.m_members.begin(), it));
    m_members[idx] = physical;

    const bool allPresent = std::ranges::all_of(m_members, [](const PHLMONITORREF& r) { return !r.expired(); });
    if (allPresent) {
        m_state = STATE_AVAILABLE;
        recomputeBoundingBox();
    }
    return true;
}

bool CMonitorGroup::onPhysicalDisconnect(const PHLMONITORREF& physical) {
    if (physical.expired())
        return false;

    auto locked = physical.lock();
    auto it     = std::ranges::find(m_rule.m_members, locked->m_name);
    if (it == m_rule.m_members.end())
        return false;

    const auto idx = static_cast<size_t>(std::distance(m_rule.m_members.begin(), it));
    m_members[idx].reset();

    if (m_state == STATE_AVAILABLE) {
        m_state = STATE_UNAVAILABLE;
        m_bbox  = CBox{};
    }
    return true;
}

bool CMonitorGroup::resolveLayoutLive(std::vector<Vector2D>& outOffsets) const {
    outOffsets.assign(m_rule.m_members.size(), Vector2D{});

    auto indexOf = [&](const std::string& n) -> std::optional<size_t> {
        auto it = std::ranges::find(m_rule.m_members, n);
        if (it == m_rule.m_members.end())
            return std::nullopt;
        return static_cast<size_t>(std::distance(m_rule.m_members.begin(), it));
    };

    if (m_rule.m_layout.empty()) {
        for (size_t i = 0; i < m_members.size(); ++i) {
            if (auto m = m_members[i].lock())
                outOffsets[i] = m->m_position;
        }
        return true;
    }

    std::vector<bool> placed(m_rule.m_members.size(), false);
    for (const auto& e : m_rule.m_layout) {
        auto memberIdx = indexOf(e.member);
        if (!memberIdx)
            return false;
        if (e.relation == Config::SMonitorGroupLayoutEntry::REL_ABSOLUTE) {
            outOffsets[*memberIdx] = e.offset;
            placed[*memberIdx]     = true;
            continue;
        }
        auto anchorIdx = indexOf(e.anchor);
        if (!anchorIdx || !placed[*anchorIdx])
            return false;
        auto anchorMon = m_members[*anchorIdx].lock();
        auto thisMon   = m_members[*memberIdx].lock();
        if (!anchorMon || !thisMon)
            return false;
        const auto& ap = outOffsets[*anchorIdx];
        switch (e.relation) {
            case Config::SMonitorGroupLayoutEntry::REL_RIGHT_OF: outOffsets[*memberIdx] = Vector2D{ap.x + anchorMon->m_size.x, ap.y}; break;
            case Config::SMonitorGroupLayoutEntry::REL_LEFT_OF: outOffsets[*memberIdx] = Vector2D{ap.x - thisMon->m_size.x, ap.y}; break;
            case Config::SMonitorGroupLayoutEntry::REL_ABOVE: outOffsets[*memberIdx] = Vector2D{ap.x, ap.y - thisMon->m_size.y}; break;
            case Config::SMonitorGroupLayoutEntry::REL_BELOW: outOffsets[*memberIdx] = Vector2D{ap.x, ap.y + anchorMon->m_size.y}; break;
            default: break;
        }
        placed[*memberIdx] = true;
    }

    for (size_t i = 0; i < m_members.size(); ++i) {
        if (placed[i])
            continue;
        if (auto m = m_members[i].lock())
            outOffsets[i] = m->m_position;
    }
    return true;
}

void CMonitorGroup::recomputeBoundingBox() {
    std::vector<Vector2D> offsets;
    if (!resolveLayoutLive(offsets)) {
        m_bbox = CBox{};
        return;
    }

    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    bool   any  = false;

    for (size_t i = 0; i < m_members.size(); ++i) {
        auto m = m_members[i].lock();
        if (!m)
            continue;
        any  = true;
        minX = std::min(minX, offsets[i].x);
        minY = std::min(minY, offsets[i].y);
        maxX = std::max(maxX, offsets[i].x + m->m_size.x);
        maxY = std::max(maxY, offsets[i].y + m->m_size.y);
    }

    if (!any) {
        m_bbox = CBox{};
        return;
    }

    m_bbox = CBox{minX, minY, maxX - minX, maxY - minY};
}

void CMonitorGroup::recomputeBoundingBoxFrom(const FnMemberInfo& reader) {
    struct SMemInfo {
        Vector2D pos;
        Vector2D size;
    };
    std::vector<std::optional<SMemInfo>> infos(m_rule.m_members.size());
    for (size_t i = 0; i < m_rule.m_members.size(); ++i) {
        auto r = reader(m_rule.m_members[i]);
        if (r)
            infos[i] = SMemInfo{r->first, r->second};
    }

    std::vector<Vector2D> offsets(m_rule.m_members.size(), Vector2D{});
    std::vector<bool>     placed(m_rule.m_members.size(), false);

    auto indexOf = [&](const std::string& n) -> std::optional<size_t> {
        auto it = std::ranges::find(m_rule.m_members, n);
        if (it == m_rule.m_members.end())
            return std::nullopt;
        return static_cast<size_t>(std::distance(m_rule.m_members.begin(), it));
    };

    if (m_rule.m_layout.empty()) {
        for (size_t i = 0; i < infos.size(); ++i) {
            if (infos[i])
                offsets[i] = infos[i]->pos;
        }
    } else {
        for (const auto& e : m_rule.m_layout) {
            auto mi = indexOf(e.member);
            if (!mi || !infos[*mi]) {
                m_bbox = CBox{};
                return;
            }
            if (e.relation == Config::SMonitorGroupLayoutEntry::REL_ABSOLUTE) {
                offsets[*mi] = e.offset;
                placed[*mi]  = true;
                continue;
            }
            auto ai = indexOf(e.anchor);
            if (!ai || !placed[*ai] || !infos[*ai]) {
                m_bbox = CBox{};
                return;
            }
            const auto& ap = offsets[*ai];
            const auto  as = infos[*ai]->size;
            const auto  ts = infos[*mi]->size;
            switch (e.relation) {
                case Config::SMonitorGroupLayoutEntry::REL_RIGHT_OF: offsets[*mi] = Vector2D{ap.x + as.x, ap.y}; break;
                case Config::SMonitorGroupLayoutEntry::REL_LEFT_OF: offsets[*mi] = Vector2D{ap.x - ts.x, ap.y}; break;
                case Config::SMonitorGroupLayoutEntry::REL_ABOVE: offsets[*mi] = Vector2D{ap.x, ap.y - ts.y}; break;
                case Config::SMonitorGroupLayoutEntry::REL_BELOW: offsets[*mi] = Vector2D{ap.x, ap.y + as.y}; break;
                default: break;
            }
            placed[*mi] = true;
        }
        for (size_t i = 0; i < infos.size(); ++i) {
            if (!placed[i] && infos[i])
                offsets[i] = infos[i]->pos;
        }
    }

    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    bool   any  = false;
    for (size_t i = 0; i < infos.size(); ++i) {
        if (!infos[i])
            continue;
        any  = true;
        minX = std::min(minX, offsets[i].x);
        minY = std::min(minY, offsets[i].y);
        maxX = std::max(maxX, offsets[i].x + infos[i]->size.x);
        maxY = std::max(maxY, offsets[i].y + infos[i]->size.y);
    }
    if (!any) {
        m_bbox = CBox{};
        return;
    }
    m_bbox = CBox{minX, minY, maxX - minX, maxY - minY};
}
