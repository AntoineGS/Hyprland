#include <config/shared/monitor/MonitorGroupParser.hpp>

#include <gtest/gtest.h>

using namespace Config;

TEST(MonitorGroupParser, basicMembers) {
    CMonitorGroupParser p;
    ASSERT_TRUE(p.parse("rdp_span, members:[rdp0;rdp1]"));
    EXPECT_EQ(p.rule().m_name, "rdp_span");
    ASSERT_EQ(p.rule().m_members.size(), 2u);
    EXPECT_EQ(p.rule().m_members[0], "rdp0");
    EXPECT_EQ(p.rule().m_members[1], "rdp1");
    EXPECT_TRUE(p.rule().m_layout.empty());
    EXPECT_FALSE(p.rule().m_defaultWorkspace.has_value());
}

TEST(MonitorGroupParser, missingName) {
    CMonitorGroupParser p;
    EXPECT_FALSE(p.parse(""));
    EXPECT_TRUE(p.getError().has_value());
}

TEST(MonitorGroupParser, missingMembers) {
    CMonitorGroupParser p;
    EXPECT_FALSE(p.parse("rdp_span"));
    EXPECT_TRUE(p.getError().has_value());
}

TEST(MonitorGroupParser, singleMemberRejected) {
    CMonitorGroupParser p;
    EXPECT_FALSE(p.parse("rdp_span, members:[rdp0]"));
    EXPECT_TRUE(p.getError().has_value());
}

TEST(MonitorGroupParser, duplicateMemberRejected) {
    CMonitorGroupParser p;
    EXPECT_FALSE(p.parse("rdp_span, members:[rdp0;rdp0]"));
    EXPECT_TRUE(p.getError().has_value());
}

TEST(MonitorGroupParser, layoutAbsolute) {
    CMonitorGroupParser p;
    ASSERT_TRUE(p.parse("rdp_span, members:[rdp0;rdp1], layout:[rdp0@0,0;rdp1@1920,0]"));
    ASSERT_EQ(p.rule().m_layout.size(), 2u);
    EXPECT_EQ(p.rule().m_layout[0].member, "rdp0");
    EXPECT_EQ(p.rule().m_layout[0].relation, SMonitorGroupLayoutEntry::REL_ABSOLUTE);
    EXPECT_DOUBLE_EQ(p.rule().m_layout[0].offset.x, 0.0);
    EXPECT_DOUBLE_EQ(p.rule().m_layout[0].offset.y, 0.0);
    EXPECT_EQ(p.rule().m_layout[1].member, "rdp1");
    EXPECT_DOUBLE_EQ(p.rule().m_layout[1].offset.x, 1920.0);
}

TEST(MonitorGroupParser, layoutRelative) {
    CMonitorGroupParser p;
    ASSERT_TRUE(p.parse("g, members:[a;b], layout:[a@0,0;b@right-of a]"));
    ASSERT_EQ(p.rule().m_layout.size(), 2u);
    EXPECT_EQ(p.rule().m_layout[1].relation, SMonitorGroupLayoutEntry::REL_RIGHT_OF);
    EXPECT_EQ(p.rule().m_layout[1].anchor, "a");
}

TEST(MonitorGroupParser, layoutBadEntry) {
    CMonitorGroupParser p;
    EXPECT_FALSE(p.parse("g, members:[a;b], layout:[a@wat]"));
    EXPECT_TRUE(p.getError().has_value());
}

TEST(MonitorGroupParser, defaultWorkspace) {
    CMonitorGroupParser p;
    ASSERT_TRUE(p.parse("g, members:[a;b], default_workspace:name:rdp"));
    ASSERT_TRUE(p.rule().m_defaultWorkspace.has_value());
    EXPECT_EQ(p.rule().m_defaultWorkspace.value(), "name:rdp");
}

TEST(MonitorGroupParser, unknownKeyRejected) {
    CMonitorGroupParser p;
    EXPECT_FALSE(p.parse("g, members:[a;b], bogus:1"));
    EXPECT_TRUE(p.getError().has_value());
}
