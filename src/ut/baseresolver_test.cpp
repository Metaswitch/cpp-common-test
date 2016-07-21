/**
 * @file baseresolver_test.cpp UT for BaseResolver class.
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
#include <sstream>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "utils.h"
#include "baseresolver.h"
#include "test_utils.hpp"
#include "test_interposer.hpp"
#include "resolver_utils.h"
#include "resolver_test.h"

using namespace std;
using ::testing::MatchesRegex;

class BaseResolverTest : public ResolverTest
{
  BaseResolver _baseresolver;

  BaseResolverTest() :
    _baseresolver(&_dnsresolver)
  {
    // Create the NAPTR cache.
    std::map<std::string, int> naptr_services;
    naptr_services["AAA+D2T"] = IPPROTO_TCP;
    naptr_services["AAA+D2S"] = IPPROTO_SCTP;
    naptr_services["aaa:diameter.tcp"] = IPPROTO_TCP;
    naptr_services["aaa:diameter.sctp"] = IPPROTO_SCTP;
    _baseresolver.create_naptr_cache(naptr_services);

    // Create the SRV cache.
    _baseresolver.create_srv_cache();

    // Create the blacklist.
    _baseresolver.create_blacklist(30, 30);
  }

  ~BaseResolverTest()
  {
    _baseresolver.destroy_blacklist();
    _baseresolver.destroy_srv_cache();
    _baseresolver.destroy_naptr_cache();
    cwtest_reset_time();
  }

  /// Implements the resolve method using a BaseResolver
  std::vector<AddrInfo> resolve(int max_targets) override
  {
    std::vector<AddrInfo> targets;
    int ttl;
    _baseresolver.a_resolve(TEST_HOST, AF_INET, TEST_PORT, TEST_TRANSPORT,
                            max_targets, targets, ttl, 1);
    return targets;
  }

};

/// A single resolver operation.
class RT
{
public:
  RT(BaseResolver& resolver, const std::string& realm) :
    _resolver(resolver),
    _realm(realm),
    _host(""),
    _max_targets(2)
  {
  }

  RT& set_host(std::string host)
  {
    _host = host;
    return *this;
  }

  /// Does an SRV record resolution for the specified SRV name, selecting
  // appropriate targets.
  std::string srv_resolve()
  {
    std::vector<AddrInfo> targets;
    int ttl;
    std::string output;

    _resolver.srv_resolve(_realm, AF_INET, IPPROTO_SCTP, _max_targets, targets, ttl, 1);
    if (!targets.empty())
    {
      // Successful, so render AddrInfo as a string.
      output = ResolverUtils::addrinfo_to_string(targets[0]);
    }
    return output;
  }

private:
  /// Reference to the BaseResolver.
  BaseResolver& _resolver;

  /// Input parameters to request.
  std::string _realm;
  std::string _host;
  int _max_targets;
};

// Test that basic IPv4 resolution works
TEST_F(BaseResolverTest, IPv4AddressResolution)
{
  add_white_records(1);
  std::string result = ResolverUtils::addrinfo_to_string(resolve(1)[0]);
  EXPECT_EQ(result, "3.0.0.0:80;transport=TCP");
}

// Test that IPv4 resolution works when there's multiple correct answers
TEST_F(BaseResolverTest, IPv4AddressResolutionManyTargets)
{
  add_white_records(7);
  std::string result = ResolverUtils::addrinfo_to_string(resolve(1)[0]);
  EXPECT_THAT(result, MatchesRegex("3.0.0.[0-6]:80;transport=TCP"));
}

// Test that whitelisted records are moved to the blackist on calling blacklist.
TEST_F(BaseResolverTest, ARecordWhiteToBlackBlacklist)
{
  add_white_records(11);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  EXPECT_TRUE(is_black("3.0.0.0", 11, 15));
}

// Test that blacklisted records are moved to the graylist after the specified
// time.
TEST_F(BaseResolverTest, ARecordBlackToGrayTime)
{
  add_white_records(11);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(31000);
  EXPECT_TRUE(is_gray("3.0.0.0", 11, 15));
}

// Test that graylisted records are moved to the blacklist on calling blacklist.
TEST_F(BaseResolverTest, ARecordGrayToBlackBlacklist)
{
  add_white_records(11);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(61000);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  EXPECT_TRUE(is_black("3.0.0.0", 11, 15));
}

// Test that the untested method correctly makes graylisted records available
// for testing once more.
TEST_F(BaseResolverTest, ARecordGrayToGrayUntested)
{
  add_white_records(11);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(31000);
  resolve(1);
  _baseresolver.untested(ip_to_addr_info("3.0.0.0"));
  EXPECT_TRUE(is_gray("3.0.0.0", 11, 15));
}

// Test that graylisted records are moved to the whitelist after the specified
// time.
TEST_F(BaseResolverTest, ARecordGrayToWhiteTime)
{
  add_white_records(11);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(61000);
  EXPECT_TRUE(is_white("3.0.0.0", 11, 15));
}

// Test that graylisted records are moved to the whitelist after calling
// success.
TEST_F(BaseResolverTest, ARecordGrayToWhiteSuccess)
{
  add_white_records(11);
  _baseresolver.blacklist(ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(31000);
  _baseresolver.success(ip_to_addr_info("3.0.0.0"));
  EXPECT_TRUE(is_white("3.0.0.0", 11, 15));
}

// Test that blacklisted SRV records aren't chosen
TEST_F(BaseResolverTest, SRVRecordResolutionWithBlacklist)
{
  // Clear the blacklist to start
  _baseresolver.clear_blacklist();

  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 0, 0, 3868, "cpp-common-test-1.cw-ngv.com"));
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 0, 1, 3868, "cpp-common-test-1.cw-ngv.com"));
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 0, 0, 3868, "cpp-common-test-2.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.cpp-common-test.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("cpp-common-test-1.cw-ngv.com", ns_t_a, records);
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.2"));
  _dnsresolver.add_to_cache("cpp-common-test-2.cw-ngv.com", ns_t_a, records);

  AddrInfo ai;
  ai.transport = IPPROTO_SCTP;
  ai.port = 3868;
  DnsRRecord* bl = ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.1");
  ai.address = _baseresolver.to_ip46(bl);
  _baseresolver.blacklist((const AddrInfo)ai);

  EXPECT_EQ("3.0.0.2:3868;transport=SCTP",
            RT(_baseresolver, "_diameter._sctp.cpp-common-test.cw-ngv.com").srv_resolve());

  delete bl;
  _baseresolver.clear_blacklist();
}

// Test that SRV resolution works when there's multiple correct answers
TEST_F(BaseResolverTest, SRVRecordResolutionManyTargets)
{
  // Clear the blacklist to start
  _baseresolver.clear_blacklist();

  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 0, 0, 3868, "cpp-common-test-1.cw-ngv.com"));
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 0, 0, 3868, "cpp-common-test-2.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.cpp-common-test.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.10"));
  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.11"));
  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.12"));
  _dnsresolver.add_to_cache("cpp-common-test-1.cw-ngv.com", ns_t_a, records);
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.20"));
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.21"));
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.22"));
  _dnsresolver.add_to_cache("cpp-common-test-2.cw-ngv.com", ns_t_a, records);
  std::string resolve = RT(_baseresolver, "_diameter._sctp.cpp-common-test.cw-ngv.com").srv_resolve();
  EXPECT_THAT(resolve, MatchesRegex("3.0.0.[0-9]{2}:3868;transport=SCTP"));
}

// Test that a failed SRV lookup returns empty
TEST_F(BaseResolverTest, SRVRecordFailedResolution)
{
  EXPECT_EQ("",
            RT(_baseresolver, "_diameter._sctp.cpp-common-test.cw-ngv.com").srv_resolve());
}
