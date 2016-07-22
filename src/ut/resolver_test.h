/**
 * @file resolver_test.h Parent class for Resolver test fixtures.
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
  AddrInfo ip_to_addr_info(std::string address_str,
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
  // for consistency with the hypothesis that the address_str record has the
  // selected color.

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
