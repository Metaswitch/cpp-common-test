/**
 * @file resolver_test.cpp Parent class for Resolver test fixtures.
 *
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "resolver_test.h"
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
  ai.port = port;
  ai.transport = transport;
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
