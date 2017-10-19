/**
 * @file httpresolver_test.cpp UT for HTTPResolver class.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
  std::vector<AddrInfo> resolve(int max_targets,
                                std::string host,
                                int port,
                                int allowed_host_state=BaseResolver::ALL_LISTS)
  {
    std::vector<AddrInfo> targets;
    _httpresolver.resolve(host, port, max_targets, targets, 0, allowed_host_state);
    return targets;
  }

  std::vector<AddrInfo> resolve(
              int max_targets, int allowed_host_state=BaseResolver::ALL_LISTS)
  {
    return resolve(max_targets, TEST_HOST, TEST_PORT);
  }
};

TEST_F(HttpResolverTest, IPv4AddressResolution)
{
  // Test defaulting of port and transport when target is just an IPv4 address
  std::vector<AddrInfo> targets = resolve(1, "3.0.0.1", 0);

  ASSERT_EQ(targets.size(), 1);
  EXPECT_EQ(targets[0], ip_to_addr_info("3.0.0.1", 80, IPPROTO_TCP));
}

TEST_F(HttpResolverTest, IPv6AddressResolution)
{
  // Test defaulting of port and transport when target is just an IPv6 address
  std::vector<AddrInfo> targets = resolve(1, "3::1", 0);

  ASSERT_EQ(targets.size(), 1);
  EXPECT_EQ(targets[0], ip_to_addr_info("3::1", 80, IPPROTO_TCP));
}

TEST_F(HttpResolverTest, IPv4AddressResolutionWithAllowedHostState)
{
  // Test that a IP address is rejected if the host is not in an acceptable
  // state.
  std::vector<AddrInfo> targets = resolve(1, "3.0.0.1", 0, BaseResolver::BLACKLISTED);
  ASSERT_EQ(targets.size(), 0);
}

TEST_F(HttpResolverTest, ARecordResolution)
{
  // Resolve an A record. Set the port (the transport is always the default)
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);

  std::vector<AddrInfo> targets = resolve(1, "cpp-common-test.cw-ngv.com", 0);

  ASSERT_GT(targets.size(), 0);
  EXPECT_EQ(targets[0], ip_to_addr_info("3.0.0.1", 80, IPPROTO_TCP));
}

TEST_F(HttpResolverTest, AAAARecordResolution)
{
  // Resolve an AAAA record. Set the port (the transport is always the default)
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::aaaa("cpp-common-test.cw-ngv.com", 3600, "3::1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);

  std::vector<AddrInfo> targets = resolve(1, "cpp-common-test.cw-ngv.com", 8888);

  ASSERT_GT(targets.size(), 0);
  EXPECT_EQ(targets[0], ip_to_addr_info("3::1", 8888, IPPROTO_TCP));
}

// The following four tests determine whether or not the blacklist_ttl and
// graylist_ttl member variables have been set correctly to 30s each. This done
// by bounding the time a record spends on the blacklist, and then the graylist,
// above and below.

TEST_F(HttpResolverTest, BlacklistTimeLowerBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(29999);

  EXPECT_TRUE(is_black("3.0.0.0", 11, 15));
}

TEST_F(HttpResolverTest, BlackListTimeUpperBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(30001);

  EXPECT_TRUE(is_gray("3.0.0.0", 11, 15));
}

TEST_F(HttpResolverTest, GrayListTimeLowerBound)
{
  add_white_records(11);
  _httpresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(59999);

  EXPECT_TRUE(is_gray("3.0.0.0", 11, 15));
}

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
