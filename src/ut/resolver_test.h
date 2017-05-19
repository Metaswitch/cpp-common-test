/**
 * @file resolver_test.h Parent class for Resolver test fixtures.
 *
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef RESOLVER_TEST_H__
#define RESOLVER_TEST_H__

#include <string>
#include <sstream>
#include "gtest/gtest.h"

#include "dnscachedresolver.h"
#include "baseresolver.h"

const std::string TEST_HOST = "cpp-common-test.cw-ngv.com";
const int TEST_PORT = 80;
const int TEST_TRANSPORT = IPPROTO_TCP;
const int TEST_TTL = 3600;

/// Parent class for BaseResolver and HttpResolver test fixtures. The blacklist
/// tests for both classes are similar, and this class contains common variables
/// and methods.
class ResolverTest : public ::testing::Test
{
public:
  ResolverTest();
  virtual ~ResolverTest();

  /// Pure virtual method - subclasses define how they interact with the
  /// resolver under test.
  virtual std::vector<AddrInfo> resolve(int max_targets) = 0;

  /// Creates and returns an AddrInfo object with the given data.
  static AddrInfo ip_to_addr_info(std::string address_str,
                           int port = TEST_PORT,
                           int transport = TEST_TRANSPORT);

  /// Adds 'count' new white records to the resolver's cache under hostname
  /// 'host', beginning at 3.0.0.0 and incrementing by one each time.
  void add_white_records(int count, std::string host = TEST_HOST);

  /// Calls resolve with the given parameters, and returns true if the result
  /// contains ai, and false otherwise.
  bool resolution_contains(AddrInfo ai, int max_targets);

  // The following three methods assume that the resolver's cache contains
  // count records, one of which, given by address_str, is under
  // consideration, and the rest are untouched white records. The methods may
  // modify the state of the resolver's records, so should be called at most
  // once per test.
  // Each evaluates a number of calls to resolve, determined by 'repetitions',
  // which reduces the probability that a record is reported as one color when
  // it is actually another.

  /// Returns true if the record is blacklisted. Has a chance of giving a false
  /// positive, which can be decreased by increasing count or repetitions
  bool is_black(std::string address_str, int count, int repetitions);

  /// Returns true if the record is graylisted. Has a chance of giving a false
  /// positive, which can be decreased by increasing count or repetitions
  bool is_gray(std::string address_str, int count, int repetitions);

  /// Returns true if the record is whitelisted. Has a chance of giving a false
  /// negative, which can be decreased by increasing count or repetitions
  bool is_white(std::string address_str, int count, int repetitions);

protected:
  DnsCachedResolver _dnsresolver;
};

#endif
