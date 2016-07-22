/**
 * @file resolver_test.cpp Parent class for Resolver test fixtures.
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

#include<resolver_test.h>
#include "resolver_utils.h"
#include "test_interposer.hpp"

/// DNS Resolver is created with server address 0.0.0.0 to disable server
/// queries.
ResolverTest::ResolverTest() :
  _dnsresolver("0.0.0.0")
{
  cwtest_completely_control_time();
}

ResolverTest::~ResolverTest()
{
  cwtest_reset_time();
}

AddrInfo ResolverTest::ip_to_addr_info(std::string address_str, int port, int transport)
{
  AddrInfo ai;
  BaseResolver::parse_ip_target(address_str, ai.address);
  ai.port = TEST_PORT;
  ai.transport = TEST_TRANSPORT;
  return ai;
}

void ResolverTest::add_white_records(int count, std::string host)
{
  std::vector<DnsRRecord*> records;
  for (int i = 0; i < count; ++i)
  {
    std::stringstream os;
    os << "3.0.0." << i;
    records.push_back(ResolverUtils::a(host, 3600, os.str()));
  }
  _dnsresolver.add_to_cache(host, ns_t_a, records);
}

bool ResolverTest::resolution_contains(AddrInfo ai, int max_targets)
{
  std::vector<AddrInfo> targets = resolve(max_targets);
  return (find(targets.begin(), targets.end(), ai) != targets.end());
}

bool ResolverTest::is_black(std::string address_str, int count, int repetitions)
{
  AddrInfo ai = ip_to_addr_info(address_str);
  // We request one fewer than the number of records contained in the resolver's
  // cache. If one record is black, and the remaining white, the black record
  // should never be returned.
  for (int i = 0; i < repetitions; ++i)
  {
    if (resolution_contains(ai, count - 1))
    {
      return false;
    }
  }
  return true;
}

bool ResolverTest::is_gray(std::string address_str, int count, int repetitions)
{
  AddrInfo ai = ip_to_addr_info(address_str);
  // The gray record should be returned on the first call to resolve, as the
  // remaining records are all white.
  if (!resolution_contains(ai, 1))
  {
    return false;
  }
  // The gray record should not be returned on any further call, so is
  // effectively black
  return is_black(address_str, count, repetitions - 1);
}

bool ResolverTest::is_white(std::string address_str, int count, int repetitions)
{
  AddrInfo ai = ip_to_addr_info(address_str);
  // If the record is gray, it will be removed from the pool of valid records
  // here.
  resolve(1);

  // If the record is white, it is highly likely it is returned here. We request
  // one fewer than the number of records so that only valid records will be
  // returned, that is, blacklisted records are not used to make up the numbers.
  for (int i = 0; i < repetitions; ++i)
  {
    if (resolution_contains(ai, count - 1))
    {
      return true;
    }
  }
  return false;
}
