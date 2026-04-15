#include <helpers/MonitorGroup.hpp>
#include <config/shared/monitor/MonitorGroupParser.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <tuple>
#include <vector>

namespace {
    Config::CMonitorGroupRule makeRule(const std::string& line) {
        Config::CMonitorGroupParser p;
        EXPECT_TRUE(p.parse(line)) << (p.getError().value_or("<no error>"));
        return p.rule();
    }

    CMonitorGroup::FnMemberInfo reader(std::initializer_list<std::tuple<std::string, Vector2D, Vector2D>> init) {
        std::vector<std::tuple<std::string, Vector2D, Vector2D>> data{init};
        return [data = std::move(data)](const std::string& name) -> std::optional<std::pair<Vector2D, Vector2D>> {
            for (const auto& [n, pos, sz] : data) {
                if (n == name)
                    return std::make_pair(pos, sz);
            }
            return std::nullopt;
        };
    }
}

TEST(MonitorGroup, namePreserved) {
    CMonitorGroup g(makeRule("rdp_span, members:[a;b]"));
    EXPECT_EQ(g.name(), "rdp_span");
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);
}

TEST(MonitorGroup, boundingBoxAbsoluteLayout) {
    auto          rule = makeRule("g, members:[a;b], layout:[a@0,0;b@1920,0]");
    CMonitorGroup g(rule);
    g.recomputeBoundingBoxFrom(reader({
        {"a", Vector2D{0, 0}, Vector2D{1920, 1080}},
        {"b", Vector2D{1920, 0}, Vector2D{1920, 1080}},
    }));
    auto bbox = g.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.x, 0.0);
    EXPECT_DOUBLE_EQ(bbox.y, 0.0);
    EXPECT_DOUBLE_EQ(bbox.w, 3840.0);
    EXPECT_DOUBLE_EQ(bbox.h, 1080.0);
}

TEST(MonitorGroup, boundingBoxRelativeLayout) {
    auto          rule = makeRule("g, members:[a;b], layout:[a@0,0;b@right-of a]");
    CMonitorGroup g(rule);
    g.recomputeBoundingBoxFrom(reader({
        {"a", Vector2D{0, 0}, Vector2D{1920, 1080}},
        {"b", Vector2D{0, 0}, Vector2D{2560, 1440}},
    }));
    auto bbox = g.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.w, 1920.0 + 2560.0);
    EXPECT_DOUBLE_EQ(bbox.h, 1440.0);
}

TEST(MonitorGroup, boundingBoxMismatchedSizes) {
    auto          rule = makeRule("g, members:[a;b], layout:[a@0,0;b@right-of a]");
    CMonitorGroup g(rule);
    g.recomputeBoundingBoxFrom(reader({
        {"a", Vector2D{0, 0}, Vector2D{1920, 1080}},
        {"b", Vector2D{0, 0}, Vector2D{2560, 1440}},
    }));
    auto bbox = g.boundingBox();
    // The 1080-tall slice leaves a 360-px tall dead zone; the bounding box
    // itself takes the taller member.
    EXPECT_DOUBLE_EQ(bbox.h, 1440.0);
}

TEST(MonitorGroup, boundingBoxImplicitLayoutUsesMonitorPositions) {
    auto          rule = makeRule("g, members:[a;b]");
    CMonitorGroup g(rule);
    g.recomputeBoundingBoxFrom(reader({
        {"a", Vector2D{0, 0}, Vector2D{1920, 1080}},
        {"b", Vector2D{1920, 0}, Vector2D{1920, 1080}},
    }));
    auto bbox = g.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.w, 3840.0);
    EXPECT_DOUBLE_EQ(bbox.h, 1080.0);
}

TEST(MonitorGroup, memberByName) {
    CMonitorGroup g(makeRule("g, members:[a;b]"));
    EXPECT_TRUE(g.memberByName("a"));
    EXPECT_TRUE(g.memberByName("b"));
    EXPECT_FALSE(g.memberByName("c"));
}

TEST(MonitorGroup, transitionsToAvailableWhenAllMembersConnect) {
    CMonitorGroup g(makeRule("g, members:[a;b;c]"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);

    EXPECT_TRUE(g.simulateConnectForTest("a"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);

    EXPECT_TRUE(g.simulateConnectForTest("b"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);

    EXPECT_TRUE(g.simulateConnectForTest("c"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_AVAILABLE);
}

TEST(MonitorGroup, transitionsToUnavailableOnMemberDisconnect) {
    CMonitorGroup g(makeRule("g, members:[a;b]"));
    EXPECT_TRUE(g.simulateConnectForTest("a"));
    EXPECT_TRUE(g.simulateConnectForTest("b"));
    ASSERT_EQ(g.state(), CMonitorGroup::STATE_AVAILABLE);

    EXPECT_TRUE(g.simulateDisconnectForTest("a"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_UNAVAILABLE);
}

TEST(MonitorGroup, boundingBoxClearsOnDisconnect) {
    auto          rule = makeRule("g, members:[a;b], layout:[a@0,0;b@1920,0]");
    CMonitorGroup g(rule);

    // Simulate all members being present, then populate the cached bbox via the
    // injectable reader hook. This mirrors what onPhysicalConnect does in production
    // (connect → recomputeBoundingBox) without requiring real CMonitor instances.
    EXPECT_TRUE(g.simulateConnectForTest("a"));
    EXPECT_TRUE(g.simulateConnectForTest("b"));
    g.recomputeBoundingBoxFrom(reader({
        {"a", Vector2D{0, 0}, Vector2D{1920, 1080}},
        {"b", Vector2D{1920, 0}, Vector2D{1920, 1080}},
    }));
    EXPECT_DOUBLE_EQ(g.boundingBox().w, 3840.0);

    EXPECT_TRUE(g.simulateDisconnectForTest("a"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_UNAVAILABLE);
    const auto bbox = g.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.w, 0.0);
    EXPECT_DOUBLE_EQ(bbox.h, 0.0);
}

TEST(MonitorGroup, stateStaysDefinedAfterPartialDisconnect) {
    // If a group was never fully AVAILABLE, a disconnect of a never-present member
    // should not push it to UNAVAILABLE.
    CMonitorGroup g(makeRule("g, members:[a;b]"));
    EXPECT_TRUE(g.simulateConnectForTest("a"));
    ASSERT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);

    // Disconnecting 'a' (which was briefly present) should not transition to
    // UNAVAILABLE because the group never became AVAILABLE in the first place.
    EXPECT_TRUE(g.simulateDisconnectForTest("a"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);
}

TEST(MonitorGroup, simulateForUnknownMemberReturnsFalse) {
    CMonitorGroup g(makeRule("g, members:[a;b]"));
    EXPECT_FALSE(g.simulateConnectForTest("nonexistent"));
    EXPECT_EQ(g.state(), CMonitorGroup::STATE_DEFINED);
}
