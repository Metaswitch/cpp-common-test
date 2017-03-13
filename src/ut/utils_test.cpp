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
#include "utils.h"

using namespace Utils;

/// Fixture for UtilsTest.
class UtilsTest : public BaseTest
{
  UtilsTest()
  {
  }

  virtual ~UtilsTest()
  {
  }
};

TEST_F(UtilsTest, ValidIPv4Address)
{
  EXPECT_EQ(IPAddressType::IPV4_ADDRESS, parse_ip_address("127.0.0.1")); 
}

TEST_F(UtilsTest, ValidIPv4AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV4_ADDRESS_WITH_PORT, parse_ip_address("127.0.0.1:80"));
}

TEST_F(UtilsTest, ValidIPv6Address)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS, parse_ip_address("1234:1234:1234:1234:1234:1234:1234:1234"));
}

TEST_F(UtilsTest, ValidIPv6AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_WITH_PORT, parse_ip_address("[1234:1234:1234:1234:1234:1234:1234:1234]:80"));
}

TEST_F(UtilsTest, ValidIPv6AddressWithBracketsNoPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_BRACKETED, parse_ip_address("[1234:1234:1234:1234:1234:1234:1234:1234]"));
}

TEST_F(UtilsTest, CompressedIPv6Address)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS, parse_ip_address("1::1"));
}

TEST_F(UtilsTest, CompressedIPv6AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_WITH_PORT, parse_ip_address("[1::1]:80"));
}

TEST_F(UtilsTest, CompressedIPv6AddressWithBracketsNoPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_BRACKETED, parse_ip_address("[1::1]"));
}

TEST_F(UtilsTest, LocalIPv6Address)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS, parse_ip_address("::1"));
}

TEST_F(UtilsTest, LocalIPv6AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_WITH_PORT, parse_ip_address("[::1]:80"));
}

TEST_F(UtilsTest, LocalIPv6AddressWithBracketsNoPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_BRACKETED, parse_ip_address("[::1]"));
}

TEST_F(UtilsTest, InvalidAddress)
{
  EXPECT_EQ(IPAddressType::INVALID, parse_ip_address("327.0.0.1"));
}

TEST_F(UtilsTest, IsBracketedAddressNo)
{
  EXPECT_FALSE(is_bracketed_address("::1"));
}

TEST_F(UtilsTest, IsBracketedAddressYes)
{
  EXPECT_TRUE(is_bracketed_address("[::1]"));
}

TEST_F(UtilsTest, UriServerForIpV4WithDefaultPort)
{
  EXPECT_EQ("127.0.0.1:80", uri_address("127.0.0.1:80", 80));
}

TEST_F(UtilsTest, UriServerForIpV4WithSpecifiedPort)
{
  EXPECT_EQ("127.0.0.1:81", uri_address("127.0.0.1:81", 80));
}

TEST_F(UtilsTest, UriServerForIpV6WithDefaultPort)
{
  EXPECT_EQ("[::1]:80", uri_address("::1", 80));
}

TEST_F(UtilsTest, UriServerForIpV6BracketedWithDefaultPort)
{
  EXPECT_EQ("[::1]:80", uri_address("[::1]", 80));
}

TEST_F(UtilsTest, UriServerForIpV6WithSpecifiedPort)
{
  EXPECT_EQ("[::1]:81", uri_address("[::1]:81", 80));
}

TEST_F(UtilsTest, UriServerForHost)
{
  EXPECT_EQ("example.com:80", uri_address("example.com", 80));
}

TEST_F(UtilsTest, UriServerForHostWithPort)
{
  EXPECT_EQ("example.com:81", uri_address("example.com:81", 80));
}

TEST_F(UtilsTest, RemoveBracketsFromIPv4Address)
{
  EXPECT_EQ("127.0.0.1", remove_brackets_from_ip("127.0.0.1"));
}

TEST_F(UtilsTest, RemoveBracketsFromBracketedIPv6Address)
{
  EXPECT_EQ("::1", remove_brackets_from_ip("[::1]"));
}

TEST_F(UtilsTest, RemoveBracketsFromBareIPv6Address)
{
  EXPECT_EQ("::1", remove_brackets_from_ip("::1"));
}

TEST_F(UtilsTest, IsAWildcard)
{
  EXPECT_TRUE(is_wildcard_uri("sip:!.*!@domain"));
  EXPECT_TRUE(is_wildcard_uri("tel:!.*!"));
  EXPECT_TRUE(is_wildcard_uri("sip:!.!*!@domain"));
  EXPECT_TRUE(is_wildcard_uri("tel:!.!*!"));
  EXPECT_TRUE(is_wildcard_uri("sip:test!.!*!@domain"));
  EXPECT_TRUE(is_wildcard_uri("sip:!.!*!test@domain"));
  EXPECT_TRUE(is_wildcard_uri("sip:test!.!*!test@domain"));
}

TEST_F(UtilsTest, IsNotAWildcard)
{
  EXPECT_FALSE(is_wildcard_uri("sip:!.*@domain"));
  EXPECT_FALSE(is_wildcard_uri("tel:!.*"));
  EXPECT_FALSE(is_wildcard_uri("sip:test!.*@!domain"));
  EXPECT_FALSE(is_wildcard_uri("sip:test!.*test@domain"));
  EXPECT_FALSE(is_wildcard_uri("tel:1234"));
  EXPECT_FALSE(is_wildcard_uri("sip:test@domain"));
}

TEST_F(UtilsTest, WildcardMatchSipURI)
{
  EXPECT_TRUE(check_users_equivalent("sip:!.*!@domain", "sip:scscf1@domain"));
}

TEST_F(UtilsTest, WildcardMatchTelURI)
{
  EXPECT_TRUE(check_users_equivalent("tel:!.*!", "tel:1234567890"));
}

TEST_F(UtilsTest, WildcardMatchTelURIWithParams)
{
  EXPECT_TRUE(check_users_equivalent("tel:!.*!", "tel:1234567890;param1;param2"));
}

TEST_F(UtilsTest, BracesWildcardMatch)
{
  EXPECT_TRUE(check_users_equivalent("tel:![4]{4}!", "tel:4444"));
}

TEST_F(UtilsTest, BracesWildcardMatchExtra)
{
  EXPECT_FALSE(check_users_equivalent("tel:![4]{4}!", "tel:44444"));
  EXPECT_FALSE(check_users_equivalent("tel:![4]{4}$!", "tel:44444"));
  EXPECT_FALSE(check_users_equivalent("tel:!^[4]{4}!", "tel:44444"));
  EXPECT_FALSE(check_users_equivalent("tel:!^[4]{4}$!", "tel:44444"));
}

TEST_F(UtilsTest, BracketsWildcardMatch)
{
  EXPECT_TRUE(check_users_equivalent("tel:![0-9]+!", "tel:1234567890"));
}

TEST_F(UtilsTest, TypeWildcardMatch)
{
  EXPECT_TRUE(check_users_equivalent("sip:!^\\w+$!@domain", "sip:scscf@domain"));
}

TEST_F(UtilsTest, BraceWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("tel:![4]{4}!", "tel:444"));
}

TEST_F(UtilsTest, BracketsWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("tel:!^[^0]+$!", "tel:1000"));
}

TEST_F(UtilsTest, TypeWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("tel:!^\\d+$!", "tel:12345notdigit"));
}

TEST_F(UtilsTest, NotAWildcard)
{
  EXPECT_TRUE(check_users_equivalent("sip:scscf@domain", "sip:scscf@domain"));
}

TEST_F(UtilsTest, NotAWildcardWithParams)
{
  EXPECT_TRUE(check_users_equivalent("sip:scscf@domain", "sip:scscf@domain;param1;param2"));
}

TEST_F(UtilsTest, NotAWildcardNoMatch)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf@domain", "sip:scscf123@domain"));
}

TEST_F(UtilsTest, WildcardNoMatchAtStart)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:icscfscscf@domain"));
}

TEST_F(UtilsTest, WildcardNoMatchAtEnd)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:scscf@newdomain"));
}

TEST_F(UtilsTest, MatchNoWildcardString)
{
  EXPECT_FALSE(check_users_equivalent("", "sip:scscf@newdomain"));
}

TEST_F(UtilsTest, WildcardNoMatchString)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", ""));
}

TEST_F(UtilsTest, WildcardNoMatchStartTooShort)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:"));
}

TEST_F(UtilsTest, WildcardNoMatchEndTooShort)
{
  EXPECT_FALSE(check_users_equivalent("sip:scscf!.*!@domain", "sip:scscf"));
}
