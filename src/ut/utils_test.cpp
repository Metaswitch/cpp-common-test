/**
 * @file utils_test.cpp UT for LoadMonitor classes.
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
