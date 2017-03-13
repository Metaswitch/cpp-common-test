/**
 * @file utils_test.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
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
