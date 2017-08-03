/**
 * @file baseresolver_test.cpp UT for BaseResolver class.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
using ::testing::Contains;
using ::testing::Not;
using ::testing::AllOf;

const int DEFAULT_COUNT = 11;
const int DEFAULT_REPETITIONS = 15;

class BaseResolverTest : public ResolverTest
{
  BaseResolver _baseresolver;
  BaseAddrIterator* it;

  BaseResolverTest() :
    _baseresolver(&_dnsresolver),
    it(nullptr)
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
    delete it; it = nullptr;
    cwtest_reset_time();
  }

  /// Helper function calling the a_resolve method of BaseResolver.
  std::vector<AddrInfo> resolve(int max_targets) override
  {
    std::vector<AddrInfo> targets;
    int ttl;
    _baseresolver.a_resolve(TEST_HOST, AF_INET, TEST_PORT, TEST_TRANSPORT,
                            max_targets, targets, ttl, 1);
    return targets;
  }

  // Overloading 'resolve' with option for allowed host state
  std::vector<AddrInfo> resolve(int max_targets, int allowed_host_state)
  {
    std::vector<AddrInfo> targets;
    int ttl;
    _baseresolver.a_resolve(TEST_HOST, AF_INET, TEST_PORT, TEST_TRANSPORT,
                            max_targets, targets, ttl, 1, allowed_host_state);
    return targets;
  }

  /// Helper function calling the a_resolve_iter method of BaseResolver.
  BaseAddrIterator* resolve_iter(int allowed_host_state=BaseResolver::ALL_LISTS)
  {
    int ttl;
    return _baseresolver.a_resolve_iter(TEST_HOST, AF_INET, TEST_PORT,
                                        TEST_TRANSPORT, ttl, 1, allowed_host_state);
  }

  /// Calls srv resolve and renders the result as a string
  std::string srv_resolve(std::string realm)
  {
    std::vector<AddrInfo> targets;
    int ttl;
    std::string output;

    _baseresolver.srv_resolve(
      realm, AF_INET, IPPROTO_SCTP, 2, targets, ttl, 1, BaseResolver::ALL_LISTS);
    if (!targets.empty())
    {
      output = ResolverUtils::addrinfo_to_string(targets[0]);
    }
    return output;
  }

  // Calls srv resolve and renders the result as a vector
  std::vector<AddrInfo> srv_resolve(std::string realm,
                                    int retries,
                                    int allowed_host_state)
  {
    std::vector<AddrInfo> targets;
    int ttl;

    _baseresolver.srv_resolve(
      realm, AF_INET, IPPROTO_SCTP, retries, targets, ttl, 1, allowed_host_state);

    return targets;
  }
};

// Test that allowed host states function returns the correct bools.
TEST_F(BaseResolverTest, GetAllowedHostStates)
{
  int allowed_host_state = BaseResolver::ALL_LISTS;
  bool whitelisted_allowed, blacklisted_allowed;

  _baseresolver.get_allowed_host_states(
                allowed_host_state, whitelisted_allowed, blacklisted_allowed);
  EXPECT_TRUE(whitelisted_allowed);
  EXPECT_TRUE(blacklisted_allowed);

  allowed_host_state = BaseResolver::WHITELISTED;
  _baseresolver.get_allowed_host_states(
                allowed_host_state, whitelisted_allowed, blacklisted_allowed);
  EXPECT_TRUE(whitelisted_allowed);
  EXPECT_FALSE(blacklisted_allowed);

  allowed_host_state = BaseResolver::BLACKLISTED;
  _baseresolver.get_allowed_host_states(
                allowed_host_state, whitelisted_allowed, blacklisted_allowed);
  EXPECT_FALSE(whitelisted_allowed);
  EXPECT_TRUE(blacklisted_allowed);
}

// Test that basic parsing of IP addresses works
TEST_F(BaseResolverTest, ParseIPAddresses)
{
  AddrInfo ai;
  ai.port = 80;
  ai.transport = IPPROTO_TCP;

  EXPECT_TRUE(_baseresolver.parse_ip_target("1.2.3.4", ai.address));
  EXPECT_EQ("1.2.3.4:80;transport=TCP", ResolverUtils::addrinfo_to_string(ai));

  EXPECT_TRUE(_baseresolver.parse_ip_target("1:2::2", ai.address));
  EXPECT_EQ("[1:2::2]:80;transport=TCP", ResolverUtils::addrinfo_to_string(ai));

  EXPECT_TRUE(_baseresolver.parse_ip_target("[1:2::2]", ai.address));
  EXPECT_EQ("[1:2::2]:80;transport=TCP", ResolverUtils::addrinfo_to_string(ai));

  EXPECT_FALSE(_baseresolver.parse_ip_target("1.2.3.4:8888", ai.address));
}

// Test that basic IPv4 resolution works
TEST_F(BaseResolverTest, IPv4AddressResolution)
{
  add_white_records(1);

  std::vector<AddrInfo> targets = resolve(1);
  ASSERT_GT(targets.size(), 0);

  std::string result = ResolverUtils::addrinfo_to_string(resolve(1)[0]);
  EXPECT_EQ(result, "3.0.0.0:80;transport=TCP");
}

// Test that IPv4 resolution works when there's multiple correct answers
TEST_F(BaseResolverTest, IPv4AddressResolutionManyTargets)
{
  add_white_records(7);

  std::vector<AddrInfo> targets = resolve(1);
  ASSERT_GT(targets.size(), 0);

  std::string result = ResolverUtils::addrinfo_to_string(resolve(1)[0]);
  EXPECT_THAT(result, MatchesRegex("3.0.0.[0-6]:80;transport=TCP"));
}

// Test that at least one graylisted record is given out each call, if
// available.
TEST_F(BaseResolverTest, ARecordAtLeastOneGray)
{
  add_white_records(DEFAULT_COUNT);
  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0");

  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);
  std::vector<AddrInfo> targets = resolve(DEFAULT_COUNT - 1);

  EXPECT_THAT(targets, Contains(gray_record));
}

// Test that just one graylisted record is given out each call, provided there
// are sufficient valid records.
TEST_F(BaseResolverTest, ARecordJustOneGray)
{
  add_white_records(DEFAULT_COUNT + 1);
  AddrInfo gray_record_0 = ResolverTest::ip_to_addr_info("3.0.0.0");
  AddrInfo gray_record_1 = ResolverTest::ip_to_addr_info("3.0.0.1");

  _baseresolver.blacklist(gray_record_0);
  _baseresolver.blacklist(gray_record_1);
  cwtest_advance_time_ms(31000);
  std::vector<AddrInfo> targets = resolve(DEFAULT_COUNT);

  // targets should contain at most one of the two gray records
  EXPECT_THAT(targets, Not(AllOf(Contains(gray_record_0), Contains(gray_record_1))));
}

// Test that graylisted records are given out only once.
TEST_F(BaseResolverTest, ARecordGrayReturnedOnce)
{
  add_white_records(DEFAULT_COUNT);
  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0");

  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);
  std::vector<AddrInfo> targets = resolve(1);

  // targets should contain the gray record
  EXPECT_THAT(targets, Contains(gray_record));

  // Further calls to resolve should not return the gray record
  for (int i = 0; i < DEFAULT_REPETITIONS; i++)
  {
    targets = resolve(DEFAULT_COUNT - 1);
    EXPECT_THAT(targets, Not(Contains(gray_record)));
  }
}

// Test that whitelisted records are moved to the blackist on calling blacklist.
TEST_F(BaseResolverTest, ARecordWhiteToBlackBlacklist)
{
  add_white_records(DEFAULT_COUNT);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));

  EXPECT_TRUE(is_black("3.0.0.0", DEFAULT_COUNT, DEFAULT_REPETITIONS));
}

// Test that blacklisted records are moved to the graylist after the specified
// time.
TEST_F(BaseResolverTest, ARecordBlackToGrayTime)
{
  add_white_records(DEFAULT_COUNT);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(31000);

  EXPECT_TRUE(is_gray("3.0.0.0", DEFAULT_COUNT, DEFAULT_REPETITIONS));
}

// Test that graylisted records are moved to the blacklist on calling blacklist.
TEST_F(BaseResolverTest, ARecordGrayToBlackBlacklist)
{
  add_white_records(DEFAULT_COUNT);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));

  cwtest_advance_time_ms(31000);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));

  EXPECT_TRUE(is_black("3.0.0.0", DEFAULT_COUNT, DEFAULT_REPETITIONS));
}

// Test that the untested method correctly makes graylisted records available
// for testing once more.
TEST_F(BaseResolverTest, ARecordGrayToGrayUntested)
{
  add_white_records(DEFAULT_COUNT);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));
  cwtest_advance_time_ms(31000);

  resolve(1);
  _baseresolver.untested(ResolverTest::ip_to_addr_info("3.0.0.0"));

  EXPECT_TRUE(is_gray("3.0.0.0", DEFAULT_COUNT, DEFAULT_REPETITIONS));
}

// Test that graylisted records are moved to the whitelist after the specified
// time.
TEST_F(BaseResolverTest, ARecordGrayToWhiteTime)
{
  add_white_records(DEFAULT_COUNT);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));

  cwtest_advance_time_ms(61000);

  EXPECT_TRUE(is_white("3.0.0.0", DEFAULT_COUNT, DEFAULT_REPETITIONS));
}

// Test that graylisted records are moved to the whitelist after calling
// success.
TEST_F(BaseResolverTest, ARecordGrayToWhiteSuccess)
{
  add_white_records(DEFAULT_COUNT);
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.0"));

  cwtest_advance_time_ms(31000);
  _baseresolver.success(ResolverTest::ip_to_addr_info("3.0.0.0"));

  EXPECT_TRUE(is_white("3.0.0.0", DEFAULT_COUNT, DEFAULT_REPETITIONS));
}

// Test that blacklisted records are returned in the case that there are
// insufficient valid records.
TEST_F(BaseResolverTest, ARecordMakeUpBlack)
{
  add_white_records(2);
  AddrInfo black_record = ResolverTest::ip_to_addr_info("3.0.0.0");
  _baseresolver.blacklist(black_record);

  std::vector<AddrInfo> targets = resolve(2);
  EXPECT_THAT(targets, Contains(black_record));
}

/// Test that multiple gray records may be returned in the case that there are
/// insufficient valid records.
TEST_F(BaseResolverTest, ARecordMakeUpMultipleGray)
{
  add_white_records(3);
  AddrInfo gray_record_0 = ResolverTest::ip_to_addr_info("3.0.0.0");
  AddrInfo gray_record_1 = ResolverTest::ip_to_addr_info("3.0.0.1");
  _baseresolver.blacklist(gray_record_0);
  _baseresolver.blacklist(gray_record_1);
  cwtest_advance_time_ms(31000);

  std::vector<AddrInfo> targets = resolve(3);

  // Both gray records should be returned.
  EXPECT_THAT(targets, AllOf(Contains(gray_record_0), Contains(gray_record_1)));
}

/// Test that gray records that have already been given out once may be returned
/// in the case that there are insufficient valid records.
TEST_F(BaseResolverTest, ARecordMakeUpUsedGray)
{
  add_white_records(2);
  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0");
  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);

  // This call should return the gray record
  resolve(1);

  std::vector<AddrInfo> targets = resolve(2);

  // The gray record should be returned.
  EXPECT_THAT(targets, Contains(gray_record));
}

/// Test that the lazy target selection iterator returns true when the target
/// has been set, and false otherwise
TEST_F(BaseResolverTest, ARecordIteratorNextReturnValue)
{
  AddrInfo record;
  AddrInfo expected_record = ResolverTest::ip_to_addr_info("3.0.0.0");

  add_white_records(1);
  BaseAddrIterator* it = resolve_iter();

  // The value of record should be set by the iterator.
  EXPECT_TRUE(it->next(record));
  EXPECT_EQ(record, expected_record);

  // The value of record should be left unchanged by the iterator.
  EXPECT_FALSE(it->next(record));
  EXPECT_EQ(record, expected_record);

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator functions correctly when there
/// are no targets
TEST_F(BaseResolverTest, ARecordEmptyIteratorNext)
{
  AddrInfo record = ResolverTest::ip_to_addr_info("3.0.0.0");
  AddrInfo expected_record = record;

  BaseAddrIterator* it = resolve_iter();

  // The value of record should be left unchanged by the iterator.
  EXPECT_FALSE(it->next(record));
  EXPECT_EQ(record, expected_record);

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator functions correctly when there
/// are no targets
TEST_F(BaseResolverTest, ARecordEmptyIteratorTake)
{
  BaseAddrIterator* it = resolve_iter();
  std::vector<AddrInfo> results = it->take(1);
  EXPECT_EQ(results.size(), 0);

  delete it; it = nullptr;
}

TEST_F(BaseResolverTest, ARecordIteratorTakeAll)
{
  add_white_records(5);
  BaseAddrIterator* it = resolve_iter();

  std::vector<AddrInfo> results = it->take(5);
  EXPECT_EQ(results.size(), 5);

  results = it->take(5);
  EXPECT_EQ(results.size(), 0);

  delete it; it = nullptr;
}

TEST_F(BaseResolverTest, ARecordIteratorTakeSome)
{
  add_white_records(5);
  BaseAddrIterator* it = resolve_iter();

  std::vector<AddrInfo> results_1 = it->take(4);
  EXPECT_EQ(results_1.size(), 4);

  // The second call should return the remaining result
  std::vector<AddrInfo> results_2 = it->take(1);
  ASSERT_EQ(results_2.size(), 1);
  EXPECT_THAT(results_1, Not(Contains(results_2[0])));

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator functions correctly when too
/// many targets are requested
TEST_F(BaseResolverTest, ARecordIteratorTakeTooMany)
{
  add_white_records(3);
  BaseAddrIterator* it = resolve_iter();

  std::vector<AddrInfo> results = it->take(5);
  EXPECT_EQ(results.size(), 3);

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator functions correctly when
/// calling a mixture of the take and next methods
TEST_F(BaseResolverTest, ARecordIteratorMixTakeAndNext)
{
  add_white_records(5);
  BaseAddrIterator* it = resolve_iter();

  AddrInfo result_1;
  EXPECT_TRUE(it->next(result_1));

  std::vector<AddrInfo> results = it->take(3);
  EXPECT_EQ(results.size(), 3);
  EXPECT_THAT(results, Not(Contains(result_1)));

  AddrInfo result_2;
  EXPECT_TRUE(it->next(result_2));
  EXPECT_THAT(results, Not(Contains(result_2)));

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator uses the state of each Host at
/// the time that next is called each time, i.e. acts lazily
TEST_F(BaseResolverTest, ARecordLazyIteratorIsLazy)
{
  AddrInfo record;
  add_white_records(2);

  // Blacklist a record
  AddrInfo black_to_gray_record = ResolverTest::ip_to_addr_info("3.0.0.0");
  _baseresolver.blacklist(black_to_gray_record);

  // Get two iterators
  BaseAddrIterator* it_1 = resolve_iter();
  BaseAddrIterator* it_2 = resolve_iter();

  // The blacklisted record should not be returned
  EXPECT_TRUE(it_1->next(record));
  EXPECT_NE(record, black_to_gray_record);

  // Move the record to the graylist
  cwtest_advance_time_ms(31000);

  // The graylisted record should be returned
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, black_to_gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

// Test the allowed list behaviour of A-record resolution
TEST_F(BaseResolverTest, ARecordAllowedHostStates)
{
  add_white_records(3);
  AddrInfo black_record = ResolverTest::ip_to_addr_info("3.0.0.0");
  _baseresolver.blacklist(black_record);
  std::vector<AddrInfo> results;

  // Test all lists allowed - should return all 3 records
  results = resolve(3);
  EXPECT_EQ(3, results.size());
  EXPECT_EQ(black_record, results.back());
  results.pop_back();
  for (const AddrInfo& result : results)
  {
    std::string result_str = ResolverUtils::addrinfo_to_string(result);
    EXPECT_THAT(result_str, MatchesRegex("3.0.0.[1-2]:80;transport=TCP"));
  }

  // Now allow the whitelist only, and ensure we only receive whitelisted
  // results
  results = resolve(3, BaseResolver::WHITELISTED);
  EXPECT_EQ(2, results.size());
  for (const AddrInfo& result : results)
  {
    std::string result_str = ResolverUtils::addrinfo_to_string(result);
    EXPECT_THAT(result_str, MatchesRegex("3.0.0.[1-2]:80;transport=TCP"));
  }

  // Now allow the blacklist only, and ensure we only receive the blacklisted
  // result
  results = resolve(3, BaseResolver::BLACKLISTED);
  EXPECT_EQ(1, results.size());
  EXPECT_EQ(black_record, results.back());

  // Now advance time to graylist the record. It should still be returned
  // as the only non-whitelisted result.
  cwtest_advance_time_ms(31000);
  results = resolve(3, BaseResolver::BLACKLISTED);
  EXPECT_EQ(1, results.size());
  EXPECT_EQ(black_record, results.back());

  // It should be returned as the only result if we ask for a single
  // whitelisted result.
  results = resolve(1, BaseResolver::WHITELISTED);
  EXPECT_EQ(1, results.size());
  EXPECT_EQ(black_record, results.back());
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

  EXPECT_EQ("3.0.0.2:3868;transport=SCTP", srv_resolve("_diameter._sctp.cpp-common-test.cw-ngv.com"));

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
  std::string resolve = srv_resolve("_diameter._sctp.cpp-common-test.cw-ngv.com");
  EXPECT_THAT(resolve, MatchesRegex("3.0.0.[0-9]{2}:3868;transport=SCTP"));
}

// Test that a failed SRV lookup returns empty
TEST_F(BaseResolverTest, SRVRecordFailedResolution)
{
  EXPECT_EQ("", srv_resolve("_diameter._sctp.cpp-common-test.cw-ngv.com"));
}

// Test that allowed host state processing works correctly for SRV resolution
TEST_F(BaseResolverTest, SRVRecordAllowedHostStates)
{
  // Clear the blacklist to start
  _baseresolver.clear_blacklist();

  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 1, 0, 3868, "cpp-common-test-1.cw-ngv.com"));
  records.push_back(ResolverUtils::srv("_diameter._sctp.cpp-common-test.cw-ngv.com", 3600, 2, 0, 3868, "cpp-common-test-2.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.cpp-common-test.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.10"));
  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.11"));
  records.push_back(ResolverUtils::a("cpp-common-test-1.cw-ngv.com", 3600, "3.0.0.12"));
  _dnsresolver.add_to_cache("cpp-common-test-1.cw-ngv.com", ns_t_a, records);
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.20"));
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.21"));
  records.push_back(ResolverUtils::a("cpp-common-test-2.cw-ngv.com", 3600, "3.0.0.22"));
  _dnsresolver.add_to_cache("cpp-common-test-2.cw-ngv.com", ns_t_a, records);

  // Blacklist some entries
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.12", 3868, IPPROTO_SCTP));
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.21", 3868, IPPROTO_SCTP));
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.22", 3868, IPPROTO_SCTP));

  std::string whitelist_regex = "3.0.0.(1[0-1]|20):3868;transport=SCTP";
  std::string blacklist_regex = "3.0.0.(12|2[1-2]):3868;transport=SCTP";

  std::vector<AddrInfo> results;

  // Resolve with 3 'retries' and specifying all_lists - we should skip all
  // the blacklisted entries
  results = srv_resolve(
    "_diameter._sctp.cpp-common-test.cw-ngv.com", 3, BaseResolver::ALL_LISTS);
  EXPECT_EQ(3, results.size());
  for (AddrInfo& result : results)
  {
    std::string result_str = ResolverUtils::addrinfo_to_string(result);
    EXPECT_THAT(result_str, MatchesRegex(whitelist_regex));
  }

  // Now ask for 4 results; the last result should be the highest priority
  // blacklisted address.
  results = srv_resolve(
    "_diameter._sctp.cpp-common-test.cw-ngv.com", 4, BaseResolver::ALL_LISTS);
  EXPECT_EQ(4, results.size());
  EXPECT_EQ(ResolverTest::ip_to_addr_info("3.0.0.12", 3868, IPPROTO_SCTP),
            results.back());

  // Ask for 4 results again, but specify whitelisted results only. We should
  // only get the 3 whitelisted results.
  results = srv_resolve(
    "_diameter._sctp.cpp-common-test.cw-ngv.com", 4, BaseResolver::WHITELISTED);
  EXPECT_EQ(3, results.size());
  for (AddrInfo& result : results)
  {
    std::string result_str = ResolverUtils::addrinfo_to_string(result);
    EXPECT_THAT(result_str, MatchesRegex(whitelist_regex));
  }

  // Ask for 4 results again, but specify blacklisted results only. We should
  // only get the 3 blacklisted results.
  results = srv_resolve(
    "_diameter._sctp.cpp-common-test.cw-ngv.com", 4, BaseResolver::BLACKLISTED);
  EXPECT_EQ(3, results.size());
  for (AddrInfo& result : results)
  {
    std::string result_str = ResolverUtils::addrinfo_to_string(result);
    EXPECT_THAT(result_str, MatchesRegex(blacklist_regex));
  }
}

// Test that SimpleAddrIterator's next method works correctly
TEST(SimpleAddrIterator, NextMethod)
{
  std::vector<AddrInfo> targets;
  AddrInfo ai = ResolverTest::ip_to_addr_info("3.0.0.1");
  targets.push_back(ai);

  SimpleAddrIterator addr_it(targets);
  AddrInfo target;

  // Check that target is set and true is returned when a target is available.
  EXPECT_TRUE(addr_it.next(target));
  EXPECT_EQ(target, ai);

  // Check that target is left unchanged and false is returned when a target is
  // not available.
  EXPECT_FALSE(addr_it.next(target));
  EXPECT_EQ(target, ai);
}

// Test that SimpleAddrIterator returns elements of its target vector in order
TEST(SimpleAddrIterator, ReturnsInOrder)
{
  std::vector<AddrInfo> targets;
  AddrInfo ai_1 = ResolverTest::ip_to_addr_info("3.0.0.1");
  targets.push_back(ai_1);
  AddrInfo ai_2 = ResolverTest::ip_to_addr_info("3.0.0.2");
  targets.push_back(ai_2);

  SimpleAddrIterator addr_it(targets);
  AddrInfo target;

  addr_it.next(target);
  EXPECT_EQ(target, ai_1);

  addr_it.next(target);
  EXPECT_EQ(target, ai_2);
}

// Test that SimpleAddrIterator functions correctly when too many targets are
// requested
TEST(SimpleAddrIterator, TooManyTargetsRequested)
{
  std::vector<AddrInfo> targets_in;
  AddrInfo ai = ResolverTest::ip_to_addr_info("3.0.0.1");
  targets_in.push_back(ai);

  SimpleAddrIterator addr_it(targets_in);
  std::vector<AddrInfo> targets_out = addr_it.take(2);

  ASSERT_EQ(targets_out.size(), 1);
  EXPECT_EQ(targets_out[0], ai);
}

