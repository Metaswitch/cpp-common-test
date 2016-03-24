/**
 * @file httpresolver_test.cpp
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

using namespace std;

/// Fixture for HttpResolverTest.
class HttpResolverTest : public ::testing::Test
{
  DnsCachedResolver _dnsresolver;
  HttpResolver _httpresolver;

  // DNS Resolver is created with server address 0.0.0.0 to disable server
  // queries.
  HttpResolverTest() :
    _dnsresolver("0.0.0.0"),
    _httpresolver(&_dnsresolver, AF_INET)
  {
  }

  virtual ~HttpResolverTest()
  {
  }
};

/// A single resolver operation
class HttpRT
{
public:
  HttpRT(HttpResolver& resolver) :
    _resolver(resolver),
    _host(""),
    _port(0),
    _max_targets(2)
  {
  }

  HttpRT& set_host(std::string host)
  {
    _host = host;
    return *this;
  }

  HttpRT& set_port(int port)
  {
    _port = port;
    return *this;
  }

  HttpRT& set_max_targets(int max_targets)
  {
    _max_targets = max_targets;
    return *this;
  }

  std::string resolve()
  {
    std::vector<AddrInfo> targets;
    std::string output;

    _resolver.resolve(_host, _port, _max_targets, targets, 0);

    if (!targets.empty())
    {
      // Successful, so render AddrInfo as a string.
      output = ResolverUtils::addrinfo_to_string(targets[0]);
    }
    return output;
  }

private:
  /// Reference to the HttpResolver.
  HttpResolver& _resolver;

  /// Input parameters to request.
  std::string _host;
  int _port;
  int _max_targets;
};

TEST_F(HttpResolverTest, IPv4AddressResolution)
{
  // Test defaulting of port and transport when target is just an IPv4 address
  EXPECT_EQ("3.0.0.1:80;transport=TCP",
            HttpRT(_httpresolver).set_host("3.0.0.1").resolve());
}

TEST_F(HttpResolverTest, IPv6AddressResolution)
{
  // Test defaulting of port and transport when target is just an IPv6 address
  EXPECT_EQ("[3::1]:80;transport=TCP",
            HttpRT(_httpresolver).set_host("3::1").resolve());
}

TEST_F(HttpResolverTest, ARecordResolution)
{
  // Resolve an A record. Set the port (the transport is always the default)
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("cpp-common-test.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);
  EXPECT_EQ("3.0.0.1:80;transport=TCP",
            HttpRT(_httpresolver).set_host("cpp-common-test.cw-ngv.com").resolve());
}

TEST_F(HttpResolverTest, AAAARecordResolution)
{
  // Resolve an AAAA record. Set the port (the transport is always the default)
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::aaaa("cpp-common-test.cw-ngv.com", 3600, "3::1"));
  _dnsresolver.add_to_cache("cpp-common-test.cw-ngv.com", ns_t_a, records);
  EXPECT_EQ("[3::1]:8888;transport=TCP",
            HttpRT(_httpresolver).set_host("cpp-common-test.cw-ngv.com").set_port(8888).resolve());
}

TEST_F(HttpResolverTest, ResolutionFailure)
{
  // Fail to resolve
  EXPECT_EQ("",
            HttpRT(_httpresolver).set_host("cpp-common-test.cw-ngv.com").resolve());
}
