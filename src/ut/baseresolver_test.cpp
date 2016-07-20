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
#include "dnscachedresolver.h"
#include "baseresolver.h"
#include "test_utils.hpp"
#include "test_interposer.hpp"
#include "resolver_utils.h"

using namespace std;
using ::testing::MatchesRegex;

const std::string TEST_HOST = "cpp-common-test.cw-ngv.com";
const int TEST_PORT = 80;
const int TEST_TRANSPORT = IPPROTO_TCP;
const int TEST_TTL = 3600;

/// Fixture for BaseResolverTest.
class BaseResolverTest : public ::testing::Test
{
  DnsCachedResolver _dnsresolver;
  BaseResolver _baseresolver;

  // DNS Resolver is created with server address 0.0.0.0 to disable server
  // queries.
  BaseResolverTest() :
    _dnsresolver("0.0.0.0"),
    _baseresolver(&_dnsresolver)
  {
    cwtest_completely_control_time();

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
    _baseresolver.create_blacklist(15,15);
  }

  virtual ~BaseResolverTest()
  {
    _baseresolver.destroy_blacklist();
    _baseresolver.destroy_srv_cache();
    _baseresolver.destroy_naptr_cache();
    cwtest_reset_time();
  }

  /// Modified a_resolve method more suited for testing
  std::vector<AddrInfo> a_resolve(int max_targets, std::string host = TEST_HOST)
  {
    std::vector<AddrInfo> targets;
    int ttl;
    _baseresolver.a_resolve(host, AF_INET, TEST_PORT, TEST_TRANSPORT, max_targets, targets, ttl, 1);
    return targets;
  }

  /// Adds white_number + 1 new white records to the resolver's cache, beginning at 3.0.0.0 and
  /// incrementing by one each time.
  void setup_records(int white_number, std::string host = TEST_HOST)
  {
    std::vector<DnsRRecord*> records;
    std::stringstream os;
    for (int i = 0; i <= white_number; ++i)
    {
      os << "3.0.0." << i;
      records.push_back(ResolverUtils::a(host, 3600, os.str()));
      os.str(std::string());
    }
    _dnsresolver.add_to_cache(host, ns_t_a, records);
  }

  /// Calls a_resolve with the given parameters, and returns true if the result
  /// contains test_record_str, and false otherwise.
  bool a_resolve_contains(std::string address_str, int max_targets,
                          std::string host = TEST_HOST)
  {
    AddrInfo ai;
    ai.port = TEST_PORT;
    ai.transport = TEST_TRANSPORT;
    if (!_baseresolver.parse_ip_target(address_str, ai.address))
    {
      return false;
    }
    std::vector<AddrInfo> targets = a_resolve(max_targets, host);
    std::vector<AddrInfo>::const_iterator found_record_it;
    found_record_it = find(targets.begin(), targets.end(), ai);
    return (found_record_it != targets.end());
  }

  // The following three methods assume that the resolver's cache contains
  // white_number + 1 records, one of which, given by address_str, is under
  // consideration, and the rest are untouched white records. The methods may
  // modify the state of the resolver's records, so should be called at most
  // once per test.

  /// Returns true if the record is blacklisted. Has a chance of giving a false
  /// positive, which can be decreased by increasing white_number or repetitions
  bool is_black(std::string address_str, int white_number, int repetitions)
  {
    // The black record should not be returned on any call.
    for (int i = 0; i < repetitions; ++i)
    {
      if (a_resolve_contains(address_str, white_number))
      {
        return false;
      }
    }
    return true;
  }

  /// Returns true if the record is graylisted. Has a chance of giving a false
  /// positive, which can be decreased by increasing white_number or repetitions
  bool is_gray(std::string address_str, int white_number, int repetitions)
  {
    // The gray record should be returned on the first call to a_resolve
    if (!a_resolve_contains(address_str, 1))
    {
      return false;
    }
    // The gray record should not be returned on any further call
    for (int i = 0; i < repetitions - 1; ++i)
    {
      if (a_resolve_contains(address_str, white_number))
      {
        return false;
      }
    }
    return true;
  }

  /// Returns true if the record is whitelisted. Has a chance of giving a false
  /// negative, which can be decreased by increasing white_number or repetitions
  bool is_white(std::string address_str, int white_number, int repetitions)
  {
    // If the record is gray, it will be removed from the pool of valid records
    // here.
    a_resolve(1);

    // If the record is white, it is highly likely it is returned here.
    for (int i = 0; i < repetitions; ++i)
    {
      if (a_resolve_contains(address_str, white_number))
      {
        return true;
      }
    }
    return false;
  }

  /// Modified blacklist method more suitable for testing.
  void blacklist(std::string address_str)
  {
    AddrInfo ai;
    _baseresolver.parse_ip_target(address_str, ai.address);
    ai.port = TEST_PORT;
    ai.transport = TEST_TRANSPORT;
    _baseresolver.blacklist(ai);
  }

  /// Modified untested method more suitable for testing.
  void untested(std::string address_str)
  {
    AddrInfo ai;
    _baseresolver.parse_ip_target(address_str, ai.address);
    ai.port = TEST_PORT;
    ai.transport = TEST_TRANSPORT;
    _baseresolver.untested(ai);
  }

  /// Modified success method more suitable for testing.
  void success(std::string address_str)
  {
    AddrInfo ai;
    _baseresolver.parse_ip_target(address_str, ai.address);
    ai.port = TEST_PORT;
    ai.transport = TEST_TRANSPORT;
    _baseresolver.success(ai);
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

  /// Does an A/AAAA record resolution for the specified name, selecting
  /// appropriate targets.
  std::string a_resolve()
  {
    std::vector<AddrInfo> targets;
    int ttl;
    std::string output;

    _resolver.a_resolve(_host, AF_INET, 80, IPPROTO_TCP, _max_targets, targets, ttl, 1);
    if (!targets.empty())
    {
      // Successful, so render AddrInfo as a string.
      output = ResolverUtils::addrinfo_to_string(targets[0]);
    }
    return output;
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
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);
  EXPECT_EQ("3.0.0.1:80;transport=TCP",
            RT(_baseresolver, "").set_host("cpp-common-test.cw-ngv.com").a_resolve());
}

// Test that IPv4 resolution works when there's multiple correct answers
TEST_F(BaseResolverTest, IPv4AddressResolutionManyTargets)
{
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.2"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.3"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.4"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.5"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.6"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);
  std::string resolve = RT(_baseresolver, "").set_host("cpp-common-test.cw-ngv.com").a_resolve();
  EXPECT_THAT(resolve, MatchesRegex("3.0.0.[1-6]:80;transport=TCP"));
}

// Test that whitelisted records are moved to the blackist on calling blacklist.
TEST_F(BaseResolverTest, ARecordWhiteToBlackBlacklist)
{
  setup_records(10);
  blacklist("3.0.0.0");
  EXPECT_TRUE(is_black("3.0.0.0",10,15));
}

// Test that blacklisted records are moved to the graylist after the specified
// time.
TEST_F(BaseResolverTest, ARecordBlackToGrayTime)
{
  setup_records(10);
  blacklist("3.0.0.0");
  cwtest_advance_time_ms(20000);
  EXPECT_TRUE(is_gray("3.0.0.0",10,15));
}

// Test that graylisted records are moved to the blacklist on calling blacklist.
TEST_F(BaseResolverTest, ARecordGrayToBlackBlacklist)
{
  setup_records(10);
  blacklist("3.0.0.0");
  cwtest_advance_time_ms(40000);
  blacklist("3.0.0.0");
  EXPECT_TRUE(is_black("3.0.0.0",10,15));
}

// Test that the untested method correctly makes graylisted records available
// for testing once more.
TEST_F(BaseResolverTest, ARecordGrayToGrayUntested)
{
  setup_records(10);
  blacklist("3.0.0.0");
  cwtest_advance_time_ms(20000);
  a_resolve(1);
  untested("3.0.0.0");
  EXPECT_TRUE(is_gray("3.0.0.0",10,15));
}

// Test that graylisted records are moved to the whitelist after the specified
// time.
TEST_F(BaseResolverTest, ARecordGrayToWhiteTime)
{
  setup_records(10);
  blacklist("3.0.0.0");
  cwtest_advance_time_ms(40000);
  EXPECT_TRUE(is_white("3.0.0.0",10,15));
}

// Test that graylisted records are moved to the whitelist after calling
// success.
TEST_F(BaseResolverTest, ARecordGrayToWhiteSuccess)
{
  setup_records(10);
  blacklist("3.0.0.0");
  cwtest_advance_time_ms(20000);
  success("3.0.0.0");
  EXPECT_TRUE(is_white("3.0.0.0",10,15));
}

// Test that blacklisted A records aren't chosen
TEST_F(BaseResolverTest, ARecordResolutionWithBlacklist)
{
  // Clear the blacklist to start
  _baseresolver.clear_blacklist();

  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.2"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);
  AddrInfo ai;
  ai.transport = IPPROTO_TCP;
  ai.port = 80;
  DnsRRecord* bl = ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1");
  ai.address = _baseresolver.to_ip46(bl);
  _baseresolver.blacklist((const AddrInfo)ai);

  EXPECT_EQ("3.0.0.2:80;transport=TCP",
            RT(_baseresolver, "").set_host("cpp-common-test.cw-ngv.com").a_resolve());
  delete bl;
  _baseresolver.clear_blacklist();
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

// Test that blacklist entries time out correctly
TEST_F(BaseResolverTest, ARecordResolutionWithOutOfDateBlacklist)
{
  // Clear the blacklist to start
  _baseresolver.clear_blacklist();

  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.2"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);

  // Now add a record to the blacklist
  AddrInfo ai;
  ai.transport = IPPROTO_TCP;
  ai.port = 80;
  DnsRRecord* bl1 = ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1");
  ai.address = _baseresolver.to_ip46(bl1);
  _baseresolver.blacklist((const AddrInfo)ai);

  // Check that we get the non-blacklisted entry
  EXPECT_EQ("3.0.0.2:80;transport=TCP",
            RT(_baseresolver, "").set_host("cpp-common-test.cw-ngv.com").a_resolve());

  // Advance time so the blacklist becomes out of date. Then add the other entry
  // to the blacklist
  cwtest_advance_time_ms(31000);
  AddrInfo ai2;
  ai2.transport = IPPROTO_TCP;
  ai2.port = 80;
  DnsRRecord* bl2 = ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.2");
  ai2.address = _baseresolver.to_ip46(bl2);
  _baseresolver.blacklist((const AddrInfo)ai2);

  // Check that we get the other entry
  EXPECT_EQ("3.0.0.1:80;transport=TCP",
            RT(_baseresolver, "").set_host("cpp-common-test.cw-ngv.com").a_resolve());

  delete bl1;
  delete bl2;
}

// Test that a failed SRV lookup returns empty
TEST_F(BaseResolverTest, SRVRecordFailedResolution)
{
  EXPECT_EQ("",
            RT(_baseresolver, "_diameter._sctp.cpp-common-test.cw-ngv.com").srv_resolve());
}
