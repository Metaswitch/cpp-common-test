/**
 * @file sasservice_test.cpp Tests support of MMF configuration
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gmock/gmock.h"
#include "fakelogger.h"

#include "test_utils.hpp"
#include "sasservice.h"

using ::testing::UnorderedElementsAreArray;
using ::testing::AtLeast;

using namespace std;

static std::string SAS_JSON_DIR = string(UT_DIR).append("/sas_json/");


TEST(SasServiceTest, ValidSasJsonFile)
{
  SasService test_service("test", "test", true, string(SAS_JSON_DIR).append("valid_sas.json"));

  EXPECT_EQ(test_service.get_single_sas_server(), "1.1.1.1");
  EXPECT_EQ(test_service.get_sas_servers(), "[{\"ip\":\"1.1.1.1\"}]");
}

TEST(SasServiceTest, MissingFile)
{
  std::string config = string(SAS_JSON_DIR).append("missing_sas.json");
  SasService test_service("test", "test", true, config);

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(test_service.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(test_service.get_sas_servers(), "[]");
}

TEST(SasServiceTest, EmptyFile)
{
  SasService test_service("test", "test", true, string(SAS_JSON_DIR).append("empty_sas.json"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(test_service.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(test_service.get_sas_servers(), "[]");
}

TEST(SasServiceTest, InvalidJson)
{
  SasService test_service("test", "test", true, string(UT_DIR).append("/invalid_json.json"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(test_service.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(test_service.get_sas_servers(), "[]");
}

TEST(SasServiceTest, WrongJsonFormat)
{
  SasService test_service("test", "test", true, string(SAS_JSON_DIR).append("bad_format_sas.json"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(test_service.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(test_service.get_sas_servers(), "[]");
}

TEST(SasServiceTest, MistypedKey)
{
  SasService test_service("test", "test", true, string(SAS_JSON_DIR).append("mistyped_sas.json"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(test_service.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(test_service.get_sas_servers(), "[]");
}

// Test that we cope with the case that the file is valid but empty.
// (Use case - customer wishes to 'turn off' SAS.)
TEST(SasServiceTest, EmptyValidFile)
{
  SasService test_service("test", "test", true, string(SAS_JSON_DIR).append("valid_empty_sas.json"));

  EXPECT_EQ(test_service.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(test_service.get_sas_servers(), "[]");
}
