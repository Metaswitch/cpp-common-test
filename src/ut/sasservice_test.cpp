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


class SasServiceTest : public ::testing::Test
{
  SasServiceTest()
  {
  }

  virtual ~SasServiceTest()
  {
  }
};

TEST_F(SasServiceTest, ValidSasJsonFile)
{
  SasService SasService(string(SAS_JSON_DIR).append("valid_sas.json"));

  EXPECT_EQ(SasService.get_single_sas_server(), "1.1.1.1");
  EXPECT_EQ(SasService.get_sas_servers(), "[{\"ip\":\"1.1.1.1\"}]");
}


// In the following tests we have various invalid/unexpected sas.json
// files.
// These tests check that the correct logs are made in each case; this isn't
// ideal as it means the tests are quite fragile, but it's the best we can do.

TEST_F(SasServiceTest, MissingFile)
{
  CapturingTestLogger _log;
  std::string config = string(SAS_JSON_DIR).append("missing_sas.json");
  SasService SasService(config);
  EXPECT_TRUE(_log.contains("No SAS configuration (file"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(SasService.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(SasService.get_sas_servers(), "[]");
}

TEST_F(SasServiceTest, EmptyFile)
{
  CapturingTestLogger _log;
  SasService SasService(string(SAS_JSON_DIR).append("empty_sas.json"));
  EXPECT_TRUE(_log.contains("Failed to read SAS configuration data from"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(SasService.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(SasService.get_sas_servers(), "[]");
}

TEST_F(SasServiceTest, InvalidJson)
{
  CapturingTestLogger _log;
  SasService SasService(string(UT_DIR).append("/invalid_json.json"));
  EXPECT_TRUE(_log.contains("Failed to read SAS configuration data: "));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(SasService.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(SasService.get_sas_servers(), "[]");
}

TEST_F(SasServiceTest, WrongJsonFormat)
{
  CapturingTestLogger _log;
  SasService SasService(string(SAS_JSON_DIR).append("bad_format_sas.json"));
  EXPECT_TRUE(_log.contains("Badly formed SAS configuration file"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(SasService.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(SasService.get_sas_servers(), "[]");
}

TEST_F(SasServiceTest, MistypedKey)
{
  CapturingTestLogger _log;
  SasService SasService(string(SAS_JSON_DIR).append("mistyped_sas.json"));
  EXPECT_TRUE(_log.contains("Badly formed SAS configuration file"));

  // Ensure we don't crash if we attempt to extract the sas_servers after
  // loading an invald JSON
  EXPECT_EQ(SasService.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(SasService.get_sas_servers(), "[]");
}

// Test that we cope with the case that the file is valid but empty.
// (Use case - customer wishes to 'turn off' SAS.)
TEST_F(SasServiceTest, EmptyValidFile)
{
  CapturingTestLogger _log;
  SasService SasService(string(SAS_JSON_DIR).append("valid_empty_sas.json"));
  EXPECT_EQ(SasService.get_single_sas_server(), "0.0.0.0");
  EXPECT_EQ(SasService.get_sas_servers(), "[]");
}
