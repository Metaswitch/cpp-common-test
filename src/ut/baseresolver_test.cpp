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
const std::string DEFAULT_REALM = "_diameter._sctp.cpp-common-test.cw-ngv.com";

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
  std::vector<AddrInfo> resolve(
      int max_targets, int allowed_host_state=BaseResolver::ALL_LISTS) override
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

  /// Help function calling the srv_resolve_iter method of BaseResolver.
  BaseAddrIterator* srv_resolve_iter(std::string realm,
                                    int allowed_host_state=BaseResolver::ALL_LISTS)
  {
    return _baseresolver.srv_resolve_iter(
      realm, AF_INET, IPPROTO_SCTP, 1, allowed_host_state);

  }

  /// Calls srv resolve and renders the result as a vector.
  std::vector<AddrInfo> srv_resolve(std::string realm,
                                    int retries=2,
                                    int allowed_host_state=BaseResolver::ALL_LISTS)
  {
    std::vector<AddrInfo> targets;
    int ttl = 0;

    _baseresolver.srv_resolve(
      realm, AF_INET, IPPROTO_SCTP, retries, targets, ttl, 1, allowed_host_state);

    return targets;
  }

  /// Calls srv resolve, returning the first result as a string (if there is
  /// a result)
  std::string first_result_from_srv(std::string realm)
  {
    std::vector<AddrInfo> targets = srv_resolve(realm);
    std::string output;

    if (! targets.empty())
    {
      output = targets[0].to_string();
    }

    return output;
  }

  AddrInfo ip_to_addrinfo(const std::string ip)
  {
    AddrInfo ai;
    ai.port = 80;
    ai.transport = IPPROTO_TCP;
    EXPECT_TRUE(Utils::parse_ip_target(ip, ai.address));

    return ai;
  }

  void add_ip_to_blacklist(const std::string ip)
  {
    _baseresolver.blacklist(ip_to_addrinfo(ip));
  }

  bool ip_allowed(const std::string ip, int allowed_host_state)
  {
    return _baseresolver.select_address(ip_to_addrinfo(ip),
                                        SAS::TrailId(1),
                                        allowed_host_state);
  }
};


// Test that basic IPv4 resolution works
TEST_F(BaseResolverTest, IPv4AddressResolution)
{
  add_white_records(1);

  std::vector<AddrInfo> targets = resolve(1);
  ASSERT_GT(targets.size(), 0);

  std::string result = resolve(1)[0].to_string();
  EXPECT_EQ(result, "3.0.0.0:80;transport=TCP");
}

// Test that IPv4 resolution works when there's multiple correct answers.
TEST_F(BaseResolverTest, IPv4AddressResolutionManyTargets)
{
  add_white_records(7);

  std::vector<AddrInfo> targets = resolve(1);
  ASSERT_GT(targets.size(), 0);

  std::string result = resolve(1)[0].to_string();
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

  // targets should contain at most one of the two gray records.
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

  // targets should contain the gray record.
  EXPECT_THAT(targets, Contains(gray_record));

  // Further calls to resolve should not return the gray record.
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

  // This call should return the gray record.
  resolve(1);

  std::vector<AddrInfo> targets = resolve(2);

  // The gray record should be returned.
  EXPECT_THAT(targets, Contains(gray_record));
}

/// Test that the lazy target selection iterator returns true when the target
/// has been set, and false otherwise.
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
/// are no targets.
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
/// are no targets.
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

  // The second call should return the remaining result.
  std::vector<AddrInfo> results_2 = it->take(1);
  ASSERT_EQ(results_2.size(), 1);
  EXPECT_THAT(results_1, Not(Contains(results_2[0])));

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator functions correctly when too
/// many targets are requested.
TEST_F(BaseResolverTest, ARecordIteratorTakeTooMany)
{
  add_white_records(3);
  BaseAddrIterator* it = resolve_iter();

  std::vector<AddrInfo> results = it->take(5);
  EXPECT_EQ(results.size(), 3);

  delete it; it = nullptr;
}

/// Test that the lazy target selection iterator functions correctly when
/// calling a mixture of the take and next methods.
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
/// the time that next is called each time, i.e. acts lazily.
TEST_F(BaseResolverTest, ARecordLazyIteratorIsLazy)
{
  AddrInfo record;
  add_white_records(2);

  // Blacklist a record.
  AddrInfo black_to_gray_record = ResolverTest::ip_to_addr_info("3.0.0.0");
  _baseresolver.blacklist(black_to_gray_record);

  // Get two iterators.
  BaseAddrIterator* it_1 = resolve_iter();
  BaseAddrIterator* it_2 = resolve_iter();

  // The blacklisted record should not be returned.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_NE(record, black_to_gray_record);

  // Move the record to the graylist.
  cwtest_advance_time_ms(31000);

  // The graylisted record should be returned.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, black_to_gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

// Test the allowed list behaviour of A-record resolution.
TEST_F(BaseResolverTest, ARecordAllowedHostStates)
{
  add_white_records(3);
  AddrInfo black_record = ResolverTest::ip_to_addr_info("3.0.0.0");
  _baseresolver.blacklist(black_record);
  std::vector<AddrInfo> results;

  // Test all lists allowed - should return all 3 records.
  results = resolve(3);
  EXPECT_EQ(3, results.size());
  EXPECT_EQ(black_record, results.back());
  results.pop_back();
  for (const AddrInfo& result : results)
  {
    std::string result_str = result.to_string();
    EXPECT_THAT(result_str, MatchesRegex("3.0.0.[1-2]:80;transport=TCP"));
  }

  // Now allow the whitelist only, and ensure we only receive whitelisted
  // results.
  results = resolve(3, BaseResolver::WHITELISTED);
  EXPECT_EQ(2, results.size());
  for (const AddrInfo& result : results)
  {
    std::string result_str = result.to_string();
    EXPECT_THAT(result_str, MatchesRegex("3.0.0.[1-2]:80;transport=TCP"));
  }

  // Now allow the blacklist only, and ensure we only receive the blacklisted
  // result.
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

// Test that blacklisted SRV records aren't chosen.
TEST_F(BaseResolverTest, SRVRecordResolutionWithBlacklist)
{
  // Clear the blacklist to start.
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
            first_result_from_srv("_diameter._sctp.cpp-common-test.cw-ngv.com"));

  delete bl;
  _baseresolver.clear_blacklist();
}

// Test that SRV resolution works when there's multiple correct answers.
TEST_F(BaseResolverTest, SRVRecordResolutionManyTargets)
{
  // Clear the blacklist to start.
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

  std::string resolve = first_result_from_srv("_diameter._sctp.cpp-common-test.cw-ngv.com");
  EXPECT_THAT(resolve, MatchesRegex("3.0.0.[0-9]{2}:3868;transport=SCTP"));
}

// Test that a failed SRV lookup returns empty.
TEST_F(BaseResolverTest, SRVRecordFailedResolution)
{
  EXPECT_EQ("", first_result_from_srv("_diameter._sctp.cpp-common-test.cw-ngv.com"));
}

// Test that allowed host state processing works correctly for SRV resolution.
TEST_F(BaseResolverTest, SRVRecordAllowedHostStates)
{
  // Clear the blacklist to start.
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

  // Blacklist some entries.
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.12", 3868, IPPROTO_SCTP));
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.21", 3868, IPPROTO_SCTP));
  _baseresolver.blacklist(ResolverTest::ip_to_addr_info("3.0.0.22", 3868, IPPROTO_SCTP));

  std::string whitelist_regex = "3.0.0.(1[0-1]|20):3868;transport=SCTP";
  std::string blacklist_regex = "3.0.0.(12|2[1-2]):3868;transport=SCTP";

  std::vector<AddrInfo> results;

  // Resolve with 3 'retries' and specifying all_lists - we should skip all
  // the blacklisted entries.
  results = srv_resolve(
    "_diameter._sctp.cpp-common-test.cw-ngv.com", 3, BaseResolver::ALL_LISTS);
  EXPECT_EQ(3, results.size());
  for (AddrInfo& result : results)
  {
    std::string result_str = result.to_string();
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
    std::string result_str = result.to_string();
    EXPECT_THAT(result_str, MatchesRegex(whitelist_regex));
  }

  // Ask for 4 results again, but specify blacklisted results only. We should
  // only get the 3 blacklisted results.
  results = srv_resolve(
    "_diameter._sctp.cpp-common-test.cw-ngv.com", 4, BaseResolver::BLACKLISTED);
  EXPECT_EQ(3, results.size());
  for (AddrInfo& result : results)
  {
    std::string result_str = result.to_string();
    EXPECT_THAT(result_str, MatchesRegex(blacklist_regex));
  }
}

// Test that SimpleAddrIterator's next method works correctly.
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

// Test that SimpleAddrIterator returns elements of its target vector in order.
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
// requested.
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

/// Test that the SRV resolution iterator functions correctly when there are no
/// targets.
TEST_F(BaseResolverTest, SRVResolutionTakeEmpty)
{
  BaseAddrIterator* it = srv_resolve_iter(DEFAULT_REALM);
  std::vector<AddrInfo> results = it->take(1);
  EXPECT_EQ(results.size(), 0);

  delete it; it = nullptr;
}

/// Test that the SRV resolution iterator functions correctly when too many
/// targets are requested.
TEST_F(BaseResolverTest, SRVResolutionTakeTooMany)
{
  add_white_srv_records(1, 3, 1);
  BaseAddrIterator* it = srv_resolve_iter(DEFAULT_REALM);

  std::vector<AddrInfo> results = it->take(5);
  EXPECT_EQ(results.size(), 3);

  delete it; it = nullptr;
}

/// Test that when all records are whitelisted and only blacklisted records are
/// requested this is handled correctly and an empty iterator is returned.
TEST_F(BaseResolverTest, SRVResolutionTakeBlackWhenAllWhite)
{
  add_white_srv_records(1, 3, 1);
  BaseAddrIterator* it = srv_resolve_iter(DEFAULT_REALM,
                                          BaseResolver::BLACKLISTED);

  std::vector<AddrInfo> results = it->take(3);
  EXPECT_EQ(results.size(), 0);

  delete it; it = nullptr;
}

/// Test that the SRV resolution will only probe each graylisted target with a
/// single call.
TEST_F(BaseResolverTest, SRVResolutionOnlyProbesGrayTargetOnce)
{
  AddrInfo record;
  add_white_srv_records(1, 2, 1);

  // Blacklist a record.
  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);
  _baseresolver.blacklist(gray_record);

  // Move the record to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted record should be returned, as it should be being probed by
  // this call.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // Get a new iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted target is now being probed and so the target should be the
  // whitelisted one.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

/// Test that the SRV resolution will not probe blacklisted targets, but once
/// the target is graylisted SRV resolution will prioritise probing it over
/// going to a whitelisted target at the same priority level.
TEST_F(BaseResolverTest, SRVResolutionProbesGrayOverWhite)
{
  AddrInfo record;
  add_white_srv_records(1, 2, 1);

  // Blacklist a record.
  AddrInfo black_to_gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);
  _baseresolver.blacklist(black_to_gray_record);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // The blacklisted record should not be returned.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, white_record);

  // Move the record to the graylist.
  cwtest_advance_time_ms(31000);

  // Get a new iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted record should be returned.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, black_to_gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

/// Test that the SRV resolution will only probe a single graylisted target per
/// call, even if the only whitelisted targets are of lower priority levels.
TEST_F(BaseResolverTest, SRVResolutionOnlyProbesGrayOncePerCall)
{
  // Creates 4 records, 2 at priority 0 and 2 at priority 1
  add_white_srv_records(2, 2, 1);

  AddrInfo gray_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo gray_record_2 = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);
  AddrInfo gray_record_3 = ResolverTest::ip_to_addr_info("3.1.1.0", 3868, IPPROTO_SCTP);

  // Blacklists 3 records.
  _baseresolver.blacklist(gray_record_1);
  _baseresolver.blacklist(gray_record_2);
  _baseresolver.blacklist(gray_record_3);

  // Move the records to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator to 2 targets.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);
  std::vector<AddrInfo> results = it_1->take(2);
  EXPECT_EQ(results.size(), 2);

  // One of the 2 priority 0 graylisted targets is returned first.
  std::string graylist_regex = "3.0.[0-1].0:3868;transport=SCTP";
  std::string result_str = results[0].to_string();
  EXPECT_THAT(result_str, MatchesRegex(graylist_regex));

  // The second target should be the white target, despite being a lower
  // priority.
  EXPECT_EQ(results[1], white_record);

  delete it_1; it_1 = nullptr;
}

/// Test that the SRV resolution does not probe a blacklisted target without
/// waiting the full 30 seconds for it to be graylisted.
TEST_F(BaseResolverTest, SRVResolutionOnlyProbesBlackAfterWaiting)
{
  AddrInfo record;
  add_white_srv_records(1, 2, 1);

  // Blacklist 2 records.
  AddrInfo black_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);
  _baseresolver.blacklist(black_record);

  // Wait just under the time taken for the record to move to the blacklist.
  cwtest_advance_time_ms(29000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // The first record should still be on the blacklist, so the whitelisted
  // record should be returned.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, white_record);

  delete it_1; it_1 = nullptr;
}

/// Tests that if the highest priority level is all blacklisted, a graylisted
/// target on level 2 is probed before targeting a whitelisted target on level 2
TEST_F(BaseResolverTest, SRVResolutionPrioritisesGrayProbingIfHighestLevelUnavailable)
{
  AddrInfo record;
  add_white_srv_records(2, 2, 1);

  // Blacklist a record.
  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.1.1.0", 3868, IPPROTO_SCTP);
  _baseresolver.blacklist(gray_record);

  // Move the record to the graylist.
  cwtest_advance_time_ms(31000);

  // Blacklist the records on the highest priority level.
  AddrInfo black_record_1 = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);
  AddrInfo black_record_2 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  _baseresolver.blacklist(black_record_1);
  _baseresolver.blacklist(black_record_2);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted record should be returned, as it should be probed first.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  delete it_1; it_1 = nullptr;
}

/// Tests that a graylisted target will not be probed if whitelisted targets
/// exist of a higher priority.
TEST_F(BaseResolverTest, SRVResolutionOnlyProbesGrayAtHighestAvailablePriority)
{
  // Creates 4 records, 2 at priority 0 and 2 at priority 1
  add_white_srv_records(2, 2, 1);

  AddrInfo gray_record_1 = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);
  AddrInfo gray_record_2 = ResolverTest::ip_to_addr_info("3.1.1.0", 3868, IPPROTO_SCTP);

  // Blacklists 2 records.
  _baseresolver.blacklist(gray_record_1);
  _baseresolver.blacklist(gray_record_2);

  // Move the records to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);
  std::vector<AddrInfo> results = it_1->take(2);
  EXPECT_EQ(results.size(), 2);

  // The two priority 0 whitelisted targets will be returned in some order.
  std::string whitelist_regex = "3.0.[0-1].0:3868;transport=SCTP";
  std::string result_str_1 = results[0].to_string();
  EXPECT_THAT(result_str_1, MatchesRegex(whitelist_regex));

  std::string result_str_2 = results[1].to_string();
  EXPECT_THAT(result_str_2, MatchesRegex(whitelist_regex));

  // Verifies that the same target wasn't returned twice.
  EXPECT_NE(result_str_1, result_str_2);

  delete it_1; it_1 = nullptr;
}

/// Tests that after 60s a blacklisted target returns to the whitelist.
TEST_F(BaseResolverTest, SRVResolutionReturnsBlackToWhiteAfterTimeOut)
{
  // Creates 2 records, 1 at priority 0 and 1 at priority 1
  add_white_srv_records(2, 1, 1);

  AddrInfo black_to_white_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the higher priority record.
  _baseresolver.blacklist(black_to_white_record);

  // Get an iterator.
  AddrInfo record;
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // The blacklisted record should be skipped and the whitelisted record
  // returned.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, white_record);

  // Move the records to the graylist and then to the whitelist (30s time out
  // for each)
  cwtest_advance_time_ms(61000);

  // Get an iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The highest priority record should have returned to the whitelist and
  // should now be returned.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, black_to_white_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

/// Tests that if a graylisted record is being probed it still leaves the
/// graylist after 30s.
TEST_F(BaseResolverTest, SRVResolutionGrayTimesOutIfProbing)
{
  AddrInfo record;

  // Creates 2 records, 1 at priority 0 and 1 at priority 1
  add_white_srv_records(2, 1, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the higher priority record.
  _baseresolver.blacklist(gray_record);

  // Move it to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // This request is probing the graylisted record.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // Get an iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted record is being probed so the whitelisted record is targeted
  // instead.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  // Despite nothing being returned by the first request, the target should still
  // time out of the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_3 = srv_resolve_iter(DEFAULT_REALM);

  // The highest priority record should have returned to the whitelist and
  // should now be returned.
  EXPECT_TRUE(it_3->next(record));
  EXPECT_EQ(record, gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
  delete it_3; it_3 = nullptr;
}

/// Tests that if a graylisted target times out while being probed and is added
/// back to the graylist, it is not still thought to be being probed.
TEST_F(BaseResolverTest, SRVResolutionGrayProbingResetAfterTimeOut)
{
  AddrInfo record;

  // Creates 2 records, 1 at priority 0 and 1 at priority 1
  add_white_srv_records(2, 1, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the higher priority record.
  _baseresolver.blacklist(gray_record);

  // Move it to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // This request is probing the graylisted record.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // Get an iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted record is being probed so the whitelisted record is targeted
  // instead.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  // Despite nothing being returned by the first request, the target should still
  // time out of the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_3 = srv_resolve_iter(DEFAULT_REALM);

  // The highest priority record should have returned to the whitelist and
  // should now be returned.
  EXPECT_TRUE(it_3->next(record));
  EXPECT_EQ(record, gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
  delete it_3; it_3 = nullptr;
}

/// Tests that if a graylisted target is successfully probed it returns to the
/// whitelist, so it is probed ahead of a lower priority whitelisted target.
TEST_F(BaseResolverTest, SRVResolutionGrayProbingSuccessMakesWhite)
{
  AddrInfo record;

  // Creates 2 records, 1 at priority 0 and 1 at priority 1
  add_white_srv_records(2, 1, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the higher priority record.
  _baseresolver.blacklist(gray_record);

  // Move it to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // This request is probing the graylisted record.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // Get an iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The graylisted record is being probed so the whitelisted record is targeted
  // instead.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  // Return success on the graylisted target.
  _baseresolver.success(gray_record);

  // Get an iterator.
  BaseAddrIterator* it_3 = srv_resolve_iter(DEFAULT_REALM);

  // The highest priority record should have returned to the whitelist and
  // should now be returned.
  EXPECT_TRUE(it_3->next(record));
  EXPECT_EQ(record, gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
  delete it_3; it_3 = nullptr;
}

/// Tests that if a graylisted target is unsuccessfully probed it returns to the
/// blacklist, so a low priority whitelisted target is targetted before it.
TEST_F(BaseResolverTest, SRVResolutionGrayProbingFailureMakesBlack)
{
  AddrInfo record;

  // Creates 2 records, 1 at priority 0 and 1 at priority 1
  add_white_srv_records(2, 1, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the higher priority record.
  _baseresolver.blacklist(gray_record);

  // Move it to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // This request is probing the graylisted record.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // Return failure on the probe.
  _baseresolver.blacklist(gray_record);

  // Get an iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The gray record is now blacklisted so the lower priority whitelisted record
  // is targeted instead.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

/// Tests that if a graylisted target is unsuccessfully probed it takes 30s
/// before it can be probed again.
TEST_F(BaseResolverTest, SRVResolutionGrayProbingFailurePreventsMoreProbes)
{
  AddrInfo record;

  // Creates 2 records, 1 at priority 0 and 1 at priority 1
  add_white_srv_records(2, 1, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the higher priority record.
  _baseresolver.blacklist(gray_record);

  // Move it to the graylist.
  cwtest_advance_time_ms(31000);

  // Get an iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // This request is probing the graylisted record.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // Return failure on the probe.
  _baseresolver.blacklist(gray_record);

  // Get an iterator.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // The gray record is now blacklisted so the lower priority whitelisted record
  // is targeted instead.
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  // Move it to the graylist again.
  cwtest_advance_time_ms(31000);

  // This request probes the graylist record again.
  BaseAddrIterator* it_3 = srv_resolve_iter(DEFAULT_REALM);
  EXPECT_TRUE(it_3->next(record));
  EXPECT_EQ(record, gray_record);

  // This request will target the whitelisted record since the graylisted record
  // is being probed.
  BaseAddrIterator* it_4 = srv_resolve_iter(DEFAULT_REALM);
  EXPECT_TRUE(it_4->next(record));
  EXPECT_EQ(record, white_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
  delete it_3; it_3 = nullptr;
  delete it_4; it_4 = nullptr;
}

/// Tests that if a graylisted target is already being probed, it is only
/// targeted when blacklisted targets are allowed.
TEST_F(BaseResolverTest, SRVResolutionGrayAlreadyProbingIsBlack)
{
  AddrInfo record;

  // Creates 2 record at priority 0
  add_white_srv_records(1, 2, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);

  // Blacklists the first record and wait for it to move to the graylist.
  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);

  // First request probes the graylisted target.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // When only whitelisted targets are allowed, the whitelisted target is
  // targeted, instead of the graylisted one currently being probed.
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM,
                                            BaseResolver::WHITELISTED);
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, white_record);

  // When only blacklisted targets are allowed, the graylisted target is allowed
  // to be targeted.
  BaseAddrIterator* it_3 = srv_resolve_iter(DEFAULT_REALM,
                                            BaseResolver::BLACKLISTED);
  EXPECT_TRUE(it_3->next(record));
  EXPECT_EQ(record, gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
  delete it_3; it_3 = nullptr;
}

/// Tests that if a graylisted address is not being probed, it will be selected
/// by a request that only allows blacklisted targets and that this will not
/// change its state to probing. Also tests that the same address can be
/// targeted by a whitelisted only request and that this will change its state
/// to probing
TEST_F(BaseResolverTest, SRVResolutionGrayNotProbingIsBlackOrWhite)
{
  AddrInfo record;

  // Creates 2 record at priority 0
  add_white_srv_records(1, 2, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);

  // Blacklists the first record and wait for it to move to the graylist.
  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);

  // First request probes the graylisted target, since it can only probe black
  // or graylisted targets. Note that this does not change the state of the
  // graylisted target to probing
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM,
                                            BaseResolver::BLACKLISTED);
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, gray_record);

  // When only whitelisted targets are allowed, the graylisted target is probed
  // and so treated as whitelisted, taking priority over the white record
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM,
                                            BaseResolver::WHITELISTED);
  EXPECT_TRUE(it_2->next(record));
  EXPECT_EQ(record, gray_record);

  // The whitelisting only call should have changed the state of the gray record
  // to probing, so a call that requests both address types will prioritise the
  // white record
  BaseAddrIterator* it_3 = srv_resolve_iter(DEFAULT_REALM);
  EXPECT_TRUE(it_3->next(record));
  EXPECT_EQ(record, white_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
  delete it_3; it_3 = nullptr;
}

/// Tests that if an address on a lower priority level moves from the whitelist
/// to the blacklist after a request is created, that address will not be
/// returned if there are any other whitelisted targets left.
TEST_F(BaseResolverTest, SRVResolutionLazyNoticeIfBlackLowPriority)
{
  AddrInfo record;

  // Creates 3 records, each at different priorities. This ensures they are
  // returned in a deterministic order.
  add_white_srv_records(3, 1, 1);

  AddrInfo white_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record_2 = ResolverTest::ip_to_addr_info("3.2.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_to_black_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // First record returned is at the highest priority.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, white_record_1);

  // Blacklists the second-highest priority record
  _baseresolver.blacklist(white_to_black_record);

  // Next record returned is the third highest priority, since the one at the
  // second highest priority is no longer whitelisted.
  EXPECT_TRUE(it_1->next(record));
  EXPECT_EQ(record, white_record_2);

  delete it_1; it_1 = nullptr;
}

/// Tests that if an address on the highest priority level moves from the
/// whitelist to the blacklist after a request is created but before that
/// address is returned, that address will not be returned if there are any
/// other whitelisted targets left.
TEST_F(BaseResolverTest, SRVResolutionLazyNoticeIfBlackHighPriority)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 1, 2);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  AddrInfo white_to_black_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_to_black_record_2 = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records at the top priority level.
  std::string result_str_1 = record.to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.0.[0-1]:3868;transport=SCTP"));

  // Blacklists both of the top priority records.
  _baseresolver.blacklist(white_to_black_record_1);
  _baseresolver.blacklist(white_to_black_record_2);

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records from the second priority level, since
  // now the entire top priority level is blacklisted.
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.1.0.[0-1]:3868;transport=SCTP"));

  delete it_1; it_1 = nullptr;
}

/// Tests that if only blacklisted targets are requested, a blacklisted target
/// that turns white will not be returned.
TEST_F(BaseResolverTest, SRVResolutionLazyNoticeIfWhiteBlackOnly)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 1, 2);

  AddrInfo black_to_white_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo black_to_white_record_2 = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);
  AddrInfo black_record_1 = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);
  AddrInfo black_record_2 = ResolverTest::ip_to_addr_info("3.1.0.1", 3868, IPPROTO_SCTP);

  // Initially blacklists all records.
  _baseresolver.blacklist(black_to_white_record_1);
  _baseresolver.blacklist(black_to_white_record_2);
  _baseresolver.blacklist(black_record_1);
  _baseresolver.blacklist(black_record_2);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM, BaseResolver::BLACKLISTED);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records at the top priority level.
  std::string result_str_1 = record.to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.0.[0-1]:3868;transport=SCTP"));

  // Waits for all records to leave the blacklist and then leave the graylist.
  cwtest_advance_time_ms(61000);

  // Returns the 2 low priority records to the blacklist.
  _baseresolver.blacklist(black_record_1);
  _baseresolver.blacklist(black_record_2);

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records from the second priority level, since
  // now the entire top priority level is whitelisted.
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.1.0.[0-1]:3868;transport=SCTP"));

  delete it_1; it_1 = nullptr;
}

/// Tests that if only whitelisted targets are requested, a whitelisted target
/// that is blacklisted after the request is created will not be returned.
TEST_F(BaseResolverTest, SRVResolutionLazyNoticeIfBlackWhiteOnly)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 1, 2);

  AddrInfo white_to_black_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_to_black_record_2 = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM, BaseResolver::WHITELISTED);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records at the top priority level.
  std::string result_str_1 = record.to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.0.[0-1]:3868;transport=SCTP"));

  // Blacklists the two highest priority records.
  _baseresolver.blacklist(white_to_black_record_1);
  _baseresolver.blacklist(white_to_black_record_2);

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records from the second priority level, since
  // now the entire top priority level is blacklisted.
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.1.0.[0-1]:3868;transport=SCTP"));

  delete it_1; it_1 = nullptr;
}

/// Tests that if the request's first target is whitelisted, and then an
/// unprobed graylisted target becomes available on the top priority level, that
/// the request will not probe it.
TEST_F(BaseResolverTest, SRVResolutionLazyWillNotProbeIfFirstTargetWhite)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 2, 1);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  AddrInfo white_to_gray_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_to_gray_record_2 = ResolverTest::ip_to_addr_info("3.0.1.0", 3868, IPPROTO_SCTP);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records at the top priority level.
  std::string result_str_1 = record.to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.[0-1].0:3868;transport=SCTP"));

  // Blacklists both of the top priority records and waits for them to move to
  // the graylist.
  _baseresolver.blacklist(white_to_gray_record_1);
  _baseresolver.blacklist(white_to_gray_record_2);
  cwtest_advance_time_ms(31000);

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records from the second priority level, since
  // now the entire top priority level is graylisted and unprobed.
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.1.[0-1].0:3868;transport=SCTP"));

  delete it_1; it_1 = nullptr;
}

/// Tests that if an address is moved from the blacklist to the whitelist after
/// the request was created, the code will not crash. The exact behaviour
/// will not be specified, since there are several reasonable implementations.
TEST_F(BaseResolverTest, SRVResolutionLazyNotCrashIfTurnsWhite)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 1, 2);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  AddrInfo black_to_white_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);

  // Initially blacklists the record.
  _baseresolver.blacklist(black_to_white_record);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is the whitelisted record at the top priority level.
  EXPECT_EQ(record, white_record);

  // Wait for the blacklisted record to move to the graylist and then the
  // whitelist.
  cwtest_advance_time_ms(61000);

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that the second record is different from the first record, beyond
  // that any behaviour is acceptable.
  EXPECT_NE(record, white_record);

  delete it_1; it_1 = nullptr;
}

/// Tests that if the lazy iterator probes a graylisted target the first time
/// it's called, it will not probe a graylisted target the second time take is
/// called.
TEST_F(BaseResolverTest, SRVResolutionLazyNotProbeSubsequentCalls)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 1, 2);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  AddrInfo gray_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo gray_record_2 = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);

  // Blacklists the top two records and waits for them to move to the graylist.
  _baseresolver.blacklist(gray_record_1);
  _baseresolver.blacklist(gray_record_2);
  cwtest_advance_time_ms(31000);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the graylisted records at the top priority level,
  // since the first time the iterator is called it will search for a top
  // priority graylisted address to probe.
  std::string result_str_1 = record.to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.0.[0-1]:3868;transport=SCTP"));

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that the second record is on the second priority level, the request
  // does not probe a graylisted target twice.
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.1.0.[0-1]:3868;transport=SCTP"));

  delete it_1; it_1 = nullptr;
}

TEST_F(BaseResolverTest, SRVResolutionLazyNoticeIfMarkedAsProbed)
{
  AddrInfo record_1;
  AddrInfo record_2;

  // Creates 2 records.
  add_white_srv_records(2, 1, 1);

  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.1.0.0", 3868, IPPROTO_SCTP);

  // Blacklists the top priority record and waits for it to move to the
  // graylist.
  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);

  // Gets 2 lazy iterators.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);
  BaseAddrIterator* it_2 = srv_resolve_iter(DEFAULT_REALM);

  // Get the first record for the second iterator and verify that it is probing
  // the graylisted record.
  EXPECT_TRUE(it_2->next(record_1));
  EXPECT_EQ(record_1, gray_record);

  // Get the first record for the first iterator, and verify that it is the
  // whitelisted record, since the graylisted record was just marked as probed.
  EXPECT_TRUE(it_1->next(record_2));
  EXPECT_EQ(record_2, white_record);

  // Verify that each iterator will return the other record on the next call.
  EXPECT_TRUE(it_2->next(record_1));
  EXPECT_EQ(record_1, white_record);
  EXPECT_TRUE(it_1->next(record_2));
  EXPECT_EQ(record_2, gray_record);

  delete it_1; it_1 = nullptr;
  delete it_2; it_2 = nullptr;
}

/// Checks that records will not be returned twice, even the only remaining
/// records are blacklisted, or no records remain at all. Also verifies that on
/// subsequent calls to take when the iterator has run out whitelisted
/// addresses, all blacklisted addresses will be returned before the iterator
/// runs out of targets.
TEST_F(BaseResolverTest, SRVResolutionLazyNotReturnTwice)
{
  std::vector<AddrInfo> results;
  AddrInfo record;

  // Creates 3 records.
  add_white_srv_records(1, 1, 3);

  AddrInfo white_record = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo black_record_1 = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);
  AddrInfo black_record_2 = ResolverTest::ip_to_addr_info("3.0.0.2", 3868, IPPROTO_SCTP);

  // Blacklists the record.
  _baseresolver.blacklist(black_record_1);
  _baseresolver.blacklist(black_record_2);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  // Get the first record and check that it is the white record.
  results = it_1->take(1);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], white_record);

  // Get the second record and check that it is a black record.
  results = it_1->take(1);
  EXPECT_EQ(results.size(), 1);
  std::string result_str_1 = results[0].to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.0.[1-2]:3868;transport=SCTP"));

  // Get the third record and check that it is the other black record.
  EXPECT_TRUE(it_1->next(record));
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.0.0.[1-2]:3868;transport=SCTP"));
  EXPECT_NE(results[0], record);

  // Try to get a fourth record and verify that nothing is returned.
  results = it_1->take(1);
  EXPECT_EQ(results.size(), 0);

  delete it_1; it_1 = nullptr;
}

/// Tests that if an address on the highest priority levels turns black after
/// the iterator prepares that priority level, that that address is returned,
/// but only after all whitelisted records have been returned.
TEST_F(BaseResolverTest, SRVResolutionLazyIfTurnsBlackStillReturnedEventually)
{
  AddrInfo record;

  // Creates 4 records.
  add_white_srv_records(2, 1, 2);

  // Gets a lazy iterator.
  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  AddrInfo white_to_black_record_1 = ResolverTest::ip_to_addr_info("3.0.0.0", 3868, IPPROTO_SCTP);
  AddrInfo white_to_black_record_2 = ResolverTest::ip_to_addr_info("3.0.0.1", 3868, IPPROTO_SCTP);

  // Get the first record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records at the top priority level.
  std::string result_str_1 = record.to_string();
  EXPECT_THAT(result_str_1, MatchesRegex("3.0.0.[0-1]:3868;transport=SCTP"));

  // Blacklists both of the top priority records.
  _baseresolver.blacklist(white_to_black_record_1);
  _baseresolver.blacklist(white_to_black_record_2);

  // Get the second record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is one of the records from the second priority level, since
  // now the entire top priority level is blacklisted.
  std::string result_str_2 = record.to_string();
  EXPECT_THAT(result_str_2, MatchesRegex("3.1.0.[0-1]:3868;transport=SCTP"));

  // Get the third record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is the other record from the second priority level.
  std::string result_str_3 = record.to_string();
  EXPECT_THAT(result_str_3, MatchesRegex("3.1.0.[0-1]:3868;transport=SCTP"));
  EXPECT_NE(result_str_3, result_str_2);

  // Get the fourth record.
  EXPECT_TRUE(it_1->next(record));

  // Check that it is the other record from the top priority level, despite
  // being blacklisted it is returned now as there are no whitelisted records
  // left.
  std::string result_str_4 = record.to_string();
  EXPECT_THAT(result_str_4, MatchesRegex("3.0.0.[0-1]:3868;transport=SCTP"));
  EXPECT_NE(result_str_4, result_str_1);


  delete it_1; it_1 = nullptr;
}

/// Tests that the lazy target selection iterator functions correctly when
/// calling a mixture of the take and next methods. A fairly arbitrary
/// convoluted example to check that nothing goes wrong in a more complicated
/// scenario.
TEST_F(BaseResolverTest, SRVResolutionLazyMixTakeAndNext)
{
  add_white_srv_records(2, 3, 3);

  AddrInfo black_record = ResolverTest::ip_to_addr_info("3.0.2.0", 3868, IPPROTO_SCTP);
  AddrInfo gray_record = ResolverTest::ip_to_addr_info("3.1.1.1", 3868, IPPROTO_SCTP);

  // Blacklist a low priority record and wait for it to move to the graylist. It
  // is a low priority record, so it will not be probed, and so be the final
  // record returned.
  _baseresolver.blacklist(gray_record);
  cwtest_advance_time_ms(31000);

  // Blacklist a high priority record. It is higher priority than the graylisted
  // record so it will be the penultimate record returned.
  _baseresolver.blacklist(black_record);

  BaseAddrIterator* it_1 = srv_resolve_iter(DEFAULT_REALM);

  AddrInfo record_1;
  EXPECT_TRUE(it_1->next(record_1));

  AddrInfo record_2;
  EXPECT_TRUE(it_1->next(record_2));
  EXPECT_NE(record_1, record_2);

  std::vector<AddrInfo> results_1 = it_1->take(10);
  EXPECT_EQ(results_1.size(), 10);
  EXPECT_THAT(results_1, Not(Contains(record_1)));
  EXPECT_THAT(results_1, Not(Contains(record_2)));

  AddrInfo record_3;
  EXPECT_TRUE(it_1->next(record_3));
  EXPECT_THAT(results_1, Not(Contains(record_3)));

  std::vector<AddrInfo> results_2 = it_1->take(8);
  // There are not enough targets left, so a vector of length 5 is returned.
  EXPECT_EQ(results_2.size(), 5);
  EXPECT_THAT(results_2, Not(Contains(record_1)));
  EXPECT_THAT(results_2, Not(Contains(record_2)));
  EXPECT_THAT(results_2, Not(Contains(record_3)));

  // Verify that the blacklisted and graylisted addresses are the second last
  // and last records returned respectively.
  EXPECT_EQ(results_2[3], black_record);
  EXPECT_EQ(results_2[4], gray_record);

  delete it_1; it_1 = nullptr;
}

// Test the behaviour of the BaseResolver's IP address allowed host state
// verification
TEST_F(BaseResolverTest, AllowedHostStateForIPAddr)
{
  add_ip_to_blacklist("192.0.2.11");
  add_ip_to_blacklist("192.0.2.12");
  add_ip_to_blacklist("[2001:db8::1]");

  // Test that ALL_LISTS behaves correctly.
  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_TRUE(ip_allowed("192.0.2.11", BaseResolver::ALL_LISTS));

  // Allow whitelisted addresses only, and ensure that blacklisted addresses
  // are not returned.
  EXPECT_TRUE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_FALSE(ip_allowed("192.0.2.12", BaseResolver::WHITELISTED));

  // Allow blacklisted addresses only, and ensure that whitelisted addresses
  // are not returned. Throw in IPv6 for flavour.
  EXPECT_FALSE(ip_allowed("[2001:db8::]", BaseResolver::BLACKLISTED));
  EXPECT_TRUE(ip_allowed("[2001:db8::1]", BaseResolver::BLACKLISTED));
}

// Test the behaviour of the BaseResolver's IP address allowed host state
// verification for graylisted addresses.
TEST_F(BaseResolverTest, AllowedHostStateForGraylistedIPAddr)
{
  // Create 3 blacklisted addresses.
  add_ip_to_blacklist("192.0.2.1"); // Only resolve this using ALL_LISTS
  add_ip_to_blacklist("192.0.2.2"); // Only resolve this using WHITELISTED
  add_ip_to_blacklist("192.0.2.3"); // Only resolve this using BLACKLISTED

  // Resolving the address with different allowed host states does what you
  // would expect, and is also repeatable (gives the same answer each time).
  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_FALSE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_TRUE(ip_allowed("192.0.2.3", BaseResolver::BLACKLISTED));

  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_FALSE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_TRUE(ip_allowed("192.0.2.3", BaseResolver::BLACKLISTED));

  // Advance time so the addresses become graylisted.
  cwtest_advance_time_ms(32000);

  // Now, an address is treated as whitelisted until it is selected, at which
  // point is starts to behave as though it's been blacklisted.
  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_TRUE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_FALSE(ip_allowed("192.0.2.3", BaseResolver::BLACKLISTED));

  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_FALSE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_FALSE(ip_allowed("192.0.2.3", BaseResolver::BLACKLISTED));

  // Make the addresses whitelisted again.
  _baseresolver.success(ip_to_addrinfo("192.0.2.1"));
  _baseresolver.success(ip_to_addrinfo("192.0.2.2"));
  _baseresolver.success(ip_to_addrinfo("192.0.2.3"));

  // Resolving the address with different allowed host states does what you
  // would expect, and is also repeatable.
  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_TRUE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_FALSE(ip_allowed("192.0.2.3", BaseResolver::BLACKLISTED));

  EXPECT_TRUE(ip_allowed("192.0.2.1", BaseResolver::ALL_LISTS));
  EXPECT_TRUE(ip_allowed("192.0.2.2", BaseResolver::WHITELISTED));
  EXPECT_FALSE(ip_allowed("192.0.2.3", BaseResolver::BLACKLISTED));
}
