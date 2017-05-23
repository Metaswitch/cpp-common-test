/**
 * @file chronosconnection_test.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "gtest/gtest.h"

#include "basetest.hpp"
#include "json_alarms.h"
#include "test_utils.hpp"
#include "fakelogger.h"

using ::testing::MatchesRegex;

/// Fixture for JSONAlarmsTest.
class JSONAlarmsTest : public BaseTest
{
  JSONAlarmsTest() {}
  virtual ~JSONAlarmsTest() {}
};

TEST_F(JSONAlarmsTest, ValidAlarms)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_TRUE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/valid_alarms.json"), error, alarm_definitions));
  EXPECT_EQ(alarm_definitions[0]._index, 1000);
  EXPECT_EQ(alarm_definitions[0]._cause, AlarmDef::Cause::SOFTWARE_ERROR);
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._severity, AlarmDef::Severity::CLEARED);
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._details, "The process has been restored to normal operation.");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._description, "Process failure cleared");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._cause, "Cause");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._effect, "Effect");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._action, "Action");
  // Here we check that if there are no extended details or extended
  // description that we use the regular details and description files.
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._extended_details, "The process has been restored to normal operation.");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._extended_description, "Process failure cleared");
}

TEST_F(JSONAlarmsTest, ExtendedAlarmDetails)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_TRUE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/extended_fields.json"), error, alarm_definitions));
  EXPECT_EQ(alarm_definitions[0]._index, 1000);
  EXPECT_EQ(alarm_definitions[0]._cause, AlarmDef::Cause::SOFTWARE_ERROR);
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._severity, AlarmDef::Severity::CLEARED);
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._details, "The process has been restored to normal operation.");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._description, "Process failure cleared");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._cause, "Cause");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._effect, "Effect");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._action, "Action");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._extended_details, "These are some extended details");
  EXPECT_EQ(alarm_definitions[0]._severity_details[0]._extended_description, "This is an extended description");
}

TEST_F(JSONAlarmsTest, ClearMissing)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/clear_missing.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*define a CLEARED.*"));
}

TEST_F(JSONAlarmsTest, NonClearMissing)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/non_clear_missing.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*define at least one non-CLEARED.*"));
}

TEST_F(JSONAlarmsTest, DescriptionTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/desc_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'description' exceeds.*"));
}

TEST_F(JSONAlarmsTest, DetailsTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/details_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'details' exceeds.*"));
}

TEST_F(JSONAlarmsTest, CauseTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/cause_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'cause' exceeds.*"));
}

TEST_F(JSONAlarmsTest, EffectTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/effect_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'effect' exceeds.*"));
}

TEST_F(JSONAlarmsTest, ActionTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/action_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'action' exceeds.*"));
}

TEST_F(JSONAlarmsTest, ExtendedDetailsTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/extended_details_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'extended_details' exceeds.*"));
}

TEST_F(JSONAlarmsTest, ExtendedDescriptionTooLong)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/extended_description_too_long.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*'extended_description' exceeds.*"));
}

TEST_F(JSONAlarmsTest, InvalidJSON)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/invalid_json.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*Invalid JSON file.*"));
}

TEST_F(JSONAlarmsTest, InvalidJSONFormat)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/invalid_json_format.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*Invalid JSON file.*"));
}

TEST_F(JSONAlarmsTest, InvalidSeverity)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/invalid_severity.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*Invalid severity.*"));
}

TEST_F(JSONAlarmsTest, InvalidCause)
{
  std::vector<AlarmDef::AlarmDefinition> alarm_definitions;
  std::string error;
  EXPECT_FALSE(JSONAlarms::validate_alarms_from_json(std::string(UT_DIR).append("/invalid_cause.json"), error, alarm_definitions));
  EXPECT_THAT(error, testing::MatchesRegex(".*Invalid cause.*"));
}
