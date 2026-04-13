#pragma once

#include "../defines.hpp"
#include "../config/shared/monitor/MonitorGroupRule.hpp"
#include "math/Math.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class CMonitor;

class CMonitorGroup {
  public:
    explicit CMonitorGroup(const Config::CMonitorGroupRule& rule);
    ~CMonitorGroup() = default;

    enum eState : uint8_t {
        STATE_DEFINED     = 0, // known from config; not all members are present
        STATE_AVAILABLE   = 1, // all members connected; bounding box computed
        STATE_UNAVAILABLE = 2, // at least one member has disconnected since we were Available
    };

    const std::string&                name() const;
    const Config::CMonitorGroupRule&  rule() const;
    eState                            state() const;
    const std::vector<PHLMONITORREF>& members() const;
    CBox                              boundingBox() const;

    // True if `physicalName` is one of this group's configured members.
    bool                              memberByName(const std::string& physicalName) const;

    // Bind a newly-connected physical monitor. Returns true if the physical is referenced
    // by this group. On becoming fully populated, state transitions to Available and the
    // bounding box is recomputed.
    bool                              onPhysicalConnect(const PHLMONITOR& physical);

    // Unbind a disconnecting physical. Returns true if it was part of this group. If the
    // group was Available, it transitions to Unavailable and the bounding box is cleared.
    bool                              onPhysicalDisconnect(const PHLMONITORREF& physical);

    // Recompute the bounding box from the currently-bound members' logical rectangles.
    void                              recomputeBoundingBox();

    // Test hook: recompute the bounding box from an injectable data source instead of
    // reading live CMonitor refs. Used by unit tests that cannot construct real monitors.
    using FnMemberInfo = std::function<std::optional<std::pair<Vector2D /*position*/, Vector2D /*size*/>>(const std::string& /*name*/)>;
    void                              recomputeBoundingBoxFrom(const FnMemberInfo& reader);

    // Self-reference, populated by the Compositor immediately after construction.
    PHLMONITORGROUPREF m_self;

  private:
    // Resolve explicit-layout offsets against live members. Returns false if a layout
    // entry references an unknown member or uses a relative anchor that has not been
    // placed yet.
    bool                      resolveLayoutLive(std::vector<Vector2D>& outOffsets) const;

    Config::CMonitorGroupRule  m_rule;
    eState                     m_state = STATE_DEFINED;
    std::vector<PHLMONITORREF> m_members; // indexed 1:1 with m_rule.m_members
    CBox                       m_bbox{};
};
