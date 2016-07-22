/**
 * @file httpresolver_test.cpp UT for HTTPResolver class.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "utils.h"
#include "dnscachedresolver.h"
#include "httpresolver.h"
#include "test_utils.hpp"
#include "resolver_utils.h"
#include "resolver_test.h"
#include "test_interposer.hpp"

using namespace std;

/// Fixture for HttpResolverTest.
class HttpResolverTest : public ResolverTest
{
  HttpResolver _httpresolver;

  HttpResolverTest() :
    _httpresolver(&_dnsresolver, AF_INET, 30, 30)
  {
  }

  virtual ~HttpResolverTest()
  {
    cwtest_reset_time();
  }

  /// Implements the resolve method using a HttpResolver
  std::vector<AddrInfo> resolve(int max_targets, std::string host, int port)
  {
    std::vector<AddrInfo> targets;
    _httpresolver.resolve(host, port, max_targets, targets, 0);
    return targets;
  }

  std::vector<AddrInfo> resolve(int max_targets)
  {
    return resolve(max_targets, TEST_HOST, TEST_PORT);
  }
};

TEST_F(HttpResolverTest, IPv4AddressResolution)
{
  // Test defaulting of port and transport when target is just an IPv4 address
  std::vector<AddrInfo> targets = resolve(1, "3.0.0.1", 0);

  ASSERT_GT(targets.size(), 0);
  EXPECT_EQ("3.0.0.1:80;transport=TCP",
            ResolverUtils::addrinfo_to_string(targets[0]));
}

TEST_F(HttpResolverTest, IPv6AddressResolution)
{
  // Test defaulting of port and transport when target is just an IPv6 address
  std::vector<AddrInfo> targets = resolve(1, "3::1", 0);

  ASSERT_GT(targets.size(), 0);
  EXPECT_EQ("[3::1]:80;transport=TCP",
            ResolverUtils::addrinfo_to_string(targets[0]));
}

TEST_F(HttpResolverTest, ARecordResolution)
{
  // Resolve an A record. Set the port (the transport is always the default)
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);

  std::vector<AddrInfo> targets = resolve(1, "cpp-common-test.cw-ngv.com", 0);

  ASSERT_GT(targets.size(), 0);
  EXPECT_EQ("3.0.0.1:80;transport=TCP",
            ResolverUtils::addrinfo_to_string(targets[0]));
}

TEST_F(HttpResolverTest, AAAARecordResolution)
{
  // Resolve an AAAA record. Set the port (the transport is always the default)
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::aaaa("cpp-common-test.cw-ngv.com", 3600, "3::1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);

  std::vector<AddrInfo> targets = resolve(1, "cpp-common-test.cw-ngv.com", 8888);

  ASSERT_GT(targets.size(), 0);
  EXPECT_EQ("[3::1]:8888;transport=TCP",
            ResolverUtils::addrinfo_to_string(targets[0]));
}

/// Tests that the default time to remain on the blacklist is greater than
/// 29999ms. Combined with the following test, effectively checks that the
/// default time to remain on the blacklist is 30s.
TEST_F(HttpResolverTest, BlacklistTimeLowerBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(29999);

  EXPECT_TRUE(is_black("3.0.0.0", 11, 15));
}

/// Tests that the default time to remain on the blacklist is less than 30001ms.
/// Combined with the previous test, effectively checks that the default time to
/// remain on the blacklist is 30s.
TEST_F(HttpResolverTest, BlackListTimeUpperBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(30001);

  EXPECT_TRUE(is_gray("3.0.0.0", 11, 15));
}

/// Tests that the default time to remain on the graylist is greater than
/// 29999ms. Combined with the following test, effectively checks that the
/// default time to remain on the graylist is 30s.
TEST_F(HttpResolverTest, GrayListTimeLowerBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(59999);

  EXPECT_TRUE(is_gray("3.0.0.0", 11, 15));
}

/// Tests that the default time to remain on the graylist is less than 30001ms.
/// Combined with the previous test, effectively checks that the default time to
/// remain on the graylist is 30s.
TEST_F(HttpResolverTest, GrayListTimeUpperBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(60001);

  EXPECT_TRUE(is_white("3.0.0.0", 11, 15));
}

TEST_F(HttpResolverTest, ResolutionFailure)
{
  // Fail to resolve
  EXPECT_EQ(0, resolve(1, "cpp-common-test.cw-ngv.com", 0).size());
}
