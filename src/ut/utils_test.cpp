/**
 * @file utils_test.cpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
