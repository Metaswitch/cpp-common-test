/**
 * @file utils_test.cpp
 *
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "gtest/gtest.h"
#include "basetest.hpp"
#include "wildcard_utils.h"

using namespace WildcardUtils;

/// Fixture for WildcardUtilsTest.
class WildcardUtilsTest : public BaseTest
{
  WildcardUtilsTest()
  {
  }

  virtual ~WildcardUtilsTest()
  {
  }
};

TEST_F(WildcardUtilsTest, IsAWildcard)
{
  EXPECT_TRUE(is_wildcard_uri("sip:!.*!@domain"));
  EXPECT_TRUE(is_wildcard_uri("tel:!.*!"));
  EXPECT_TRUE(is_wildcard_uri("sip:!.!*!@domain"));
  EXPECT_TRUE(is_wildcard_uri("tel:!.!*!"));
  EXPECT_TRUE(is_wildcard_uri("sip:test!.!*!@domain"));
  EXPECT_TRUE(is_wildcard_uri("sip:!.!*!test@domain"));
  EXPECT_TRUE(is_wildcard_uri("sip:test!.!*!test@domain"));
}

TEST_F(WildcardUtilsTest, IsNotAWildcard)
{
  EXPECT_FALSE(is_wildcard_uri("sip:!.*@domain"));
  EXPECT_FALSE(is_wildcard_uri("tel:!.*"));
  EXPECT_FALSE(is_wildcard_uri("sip:test!.*@!domain"));
  EXPECT_FALSE(is_wildcard_uri("sip:test!.*test@domain"));
  EXPECT_FALSE(is_wildcard_uri("tel:1234"));
  EXPECT_FALSE(is_wildcard_uri("sip:test@domain"));
}

TEST_F(WildcardUtilsTest, WildcardMatchSipURI)
{
  EXPECT_TRUE(check_users_equivalent("sip:!.*!@domain", "sip:scscf1@domain"));
}

TEST_F(WildcardUtilsTest, WildcardMatchTelURI)
{
  EXPECT_TRUE(check_users_equivalent("tel:!.*!", "tel:1234567890"));
}

TEST_F(WildcardUtilsTest, WildcardMatchTelURIWithParams)
{
  EXPECT_TRUE(check_users_equivalent("tel:!.*!", "tel:1234567890;param1;param2"));
}

TEST_F(WildcardUtilsTest, BracesWildcardMatch)
{
  EXPECT_TRUE(check_users_equivalent("tel:![4]{4}!", "tel:4444"));
}

TEST_F(WildcardUtilsTest, BracesWildcardMatchExtra)
{
  EXPECT_FALSE(check_users_equivalent("tel:![4]{4}!", "tel:44444"));
  EXPECT_FALSE(check_users_equivalent("tel:![4]{4}$!", "tel:44444"));
  EXPECT_FALSE(check_users_equivalent("tel:!^[4]{4}!", "tel:44444"));
  EXPECT_FALSE(check_users_equivalent("tel:!^[4]{4}$!", "tel:44444"));
}

TEST_F(WildcardUtilsTest, BracketsWildcardMatch)
{
  EXPECT_TRUE(check_users_equivalent("tel:![0-9]+!", "tel:1234567890"));
}

TEST_F(WildcardUtilsTest, TypeWildcardMatch)
{
  EXPECT_TRUE(check_users_equivalent("sip:!^\\w+$!@domain", "sip:scscf@domain"));
}

TEST_F(WildcardUtilsTest, BraceWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("tel:![4]{4}!", "tel:444"));
}

TEST_F(WildcardUtilsTest, BracketsWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("tel:!^[^0]+$!", "tel:1000"));
}

TEST_F(WildcardUtilsTest, TypeWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("tel:!^\\d+$!", "tel:12345notdigit"));
}

TEST_F(WildcardUtilsTest, NotAWildcard)
{
  EXPECT_TRUE(check_users_equivalent("sip:scscf@domain", "sip:scscf@domain"));
}

TEST_F(WildcardUtilsTest, NotAWildcardWithParams)
{
  EXPECT_TRUE(check_users_equivalent("sip:scscf@domain", "sip:scscf@domain;param1;param2"));
}

TEST_F(WildcardUtilsTest, NotAWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf@domain", "sip:scscf123@domain"));
}

TEST_F(WildcardUtilsTest, WildcardNoMatchAtStart)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:icscfscscf@domain"));
}

TEST_F(WildcardUtilsTest, WildcardNoMatchAtEnd)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:scscf@newdomain"));
}

TEST_F(WildcardUtilsTest, MatchNoWildcardString)
{
  EXPECT_FALSE(check_users_equivalent("", "sip:scscf@newdomain"));
}

TEST_F(WildcardUtilsTest, WildcardNoMatchString)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", ""));
}

TEST_F(WildcardUtilsTest, WildcardNoMatchStartTooShort)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:"));
}

TEST_F(WildcardUtilsTest, WildcardNoMatchEndTooShort)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:scscf"));
}
