/**
 * @file diameterresolver_test.cpp UT for DiameterResolver class.
 *
 * Copyright (C) Metaswitch Networks 2017
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
#include "diameterresolver.h"
#include "test_utils.hpp"
#include "resolver_utils.h"
#include "test_interposer.hpp"

using namespace std;

/// Fixture for DiamterResolverTest.
class DiameterResolverTest : public ::testing::Test
{
  DnsCachedResolver _dnsresolver;
  DiameterResolver _diameterresolver;

  // DNS Resolver is created with server address 0.0.0.0 to disable server
  // queries.
  DiameterResolverTest() :
    _dnsresolver("0.0.0.0"),
    _diameterresolver(&_dnsresolver, AF_INET)
  {
    cwtest_completely_control_time();
  }

  virtual ~DiameterResolverTest()
  {
    cwtest_reset_time();
  }
};

/// A single resolver operation.
class RT
{
public:
  RT(DiameterResolver& resolver, const std::string& realm) :
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

  RT& set_max_targets(int max_targets)
  {
    _max_targets = max_targets;
    return *this;
  }

  void resolve(std::string expected_output, int expected_ttl)
  {
    SCOPED_TRACE(_realm);
    std::vector<AddrInfo> targets;
    int ttl;
    std::string output;

    _resolver.resolve(_realm, _host, _max_targets, targets, ttl);
    if (!targets.empty())
    {
      // Successful, so render AddrInfo as a string.
      output = ResolverUtils::addrinfo_to_string(targets[0]);
    }

    EXPECT_EQ(expected_output, output);
    EXPECT_EQ(expected_ttl, ttl);
  }

private:
  /// Reference to the DiameterResolver.
  DiameterResolver& _resolver;

  /// Input parameters to request.
  std::string _realm;
  std::string _host;
  int _max_targets;
};

TEST_F(DiameterResolverTest, IPv4AddressResolution)
{
  // Test defaulting of port and transport when target is IP address.
  // IP doesn't have ttl - confirm that ttl is not set.
  RT(_diameterresolver, "").set_host("3.0.0.1").resolve("3.0.0.1:3868;transport=SCTP", 0);
}

TEST_F(DiameterResolverTest, SimpleNAPTRSRVTCPResolution)
{
  // Test selection of TCP transport and port using NAPTR and SRV records.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 3600, 0, 0, "S", "AAA+D2T", "", "_diameter._tcp.sprout.cw-ngv.com"));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_naptr, records);

  records.push_back(ResolverUtils::srv("_diameter._tcp.sprout.cw-ngv.com", 2400, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._tcp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=TCP", 2400);
}

TEST_F(DiameterResolverTest, NAPTRSRVResolutionWithRegex)
{
  // Set up NAPTR records with regexes
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 3600, 0, 0, "", "AAA+D2S", "/", ""));
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 2400, 0, 0, "", "AAA+D2S", "/(.*)/a$1/", ""));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_naptr, records);
  records.push_back(ResolverUtils::naptr("asprout.cw-ngv.com", 3600, 0, 0, "s", "AAA+D2S", "", "_diameter._sctp.sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("asprout.cw-ngv.com", ns_t_naptr, records);

  records.push_back(ResolverUtils::srv("_diameter._sctp.sprout-1.cw-ngv.com", 3600, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.sprout-1.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 2400);
}

TEST_F(DiameterResolverTest, SimpleNAPTRSRVSCTPResolution)
{
  // Test selection of SCTP transport and port using NAPTR and SRV records (and lowercase S).
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 3600, 0, 0, "s", "AAA+D2S", "", "_diameter._sctp.sprout.cw-ngv.com"));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_naptr, records);

  records.push_back(ResolverUtils::srv("_diameter._sctp.sprout.cw-ngv.com", 2400, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 1200, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 1200);
}

TEST_F(DiameterResolverTest, SimpleNAPTRATCPResolution)
{
  // Test selection of TCP transport and port using NAPTR and SRV records.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 3600, 0, 0, "A", "AAA+D2T", "", "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_naptr, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 2400, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=TCP", 2400);
}

TEST_F(DiameterResolverTest, SimpleNAPTRASCTPResolution)
{
  // Test selection of TCP transport and port using NAPTR and SRV records.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 2400, 0, 0, "A", "AAA+D2S", "", "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_naptr, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 2400);
}

TEST_F(DiameterResolverTest, SimpleSRVTCPResolution)
{
  // Test selection of TCP transport and port using SRV records only
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._tcp.sprout.cw-ngv.com", 3600, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._tcp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=TCP", 3600);
}

TEST_F(DiameterResolverTest, SimpleSRVSCTPResolution)
{
  // Test selection of SCTP transport and port using SRV records only
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._sctp.sprout.cw-ngv.com", 3600, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 2400, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 2400);
}

TEST_F(DiameterResolverTest, SimpleSRVTCPPreference)
{
  // Test preference for TCP transport over SCTP transport if both configure in SRV.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._tcp.sprout.cw-ngv.com", 3600, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._tcp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::srv("_diameter._sctp.sprout.cw-ngv.com", 1200, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 2400, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=TCP", 2400);
}

TEST_F(DiameterResolverTest, SimpleAResolution)
{
  // Test resolution using A records only.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::a("sprout.cw-ngv.com", 3600, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  // Test default port/transport.
  RT(_diameterresolver, "").set_host("sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 3600);
}

TEST_F(DiameterResolverTest, MinTTLEmptySRV)
{
  // Test that the correct minimum ttl is returned when multiple SRVs, one of
  // which is empty, are present.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::srv("_diameter._tcp.sprout.cw-ngv.com", 3600, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  records.push_back(ResolverUtils::srv("_diameter._tcp.sprout.cw-ngv.com", 1200, 0, 0, 3868, "sprout-2.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._tcp.sprout.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 2400, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=TCP", 1200);
}

TEST_F(DiameterResolverTest, MinTTLEmptyNAPTR)
 {
  // Test that the correct minimum ttl is returned when multiple NAPTRs, one of
  // which is empty, are present.
  std::vector<DnsRRecord*> records;
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 600, 0, 0, "", "AAA+D2S", "/", ""));
  records.push_back(ResolverUtils::naptr("sprout.cw-ngv.com", 3600, 0, 0, "s", "AAA+D2S", "", "_diameter._sctp.sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("sprout.cw-ngv.com", ns_t_naptr, records);

  records.push_back(ResolverUtils::srv("_diameter._sctp.sprout-1.cw-ngv.com", 2400, 0, 0, 3868, "sprout-1.cw-ngv.com"));
  _dnsresolver.add_to_cache("_diameter._sctp.sprout-1.cw-ngv.com", ns_t_srv, records);

  records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 1200, "3.0.0.1"));
  _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

  TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

  RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 600);
 }

TEST_F(DiameterResolverTest, ZeroTTLReturned)
{
  // Test that if the ttl is set to 0, it will be returned correctly.
  std::vector<DnsRRecord*> records;
   records.push_back(ResolverUtils::srv("_diameter._sctp.sprout.cw-ngv.com", 3600, 0, 0, 3868, "sprout-1.cw-ngv.com"));
   _dnsresolver.add_to_cache("_diameter._sctp.sprout.cw-ngv.com", ns_t_srv, records);

   records.push_back(ResolverUtils::a("sprout-1.cw-ngv.com", 0, "3.0.0.1"));
   _dnsresolver.add_to_cache("sprout-1.cw-ngv.com", ns_t_a, records);

   TRC_DEBUG("Cache status\n%s", _dnsresolver.display_cache().c_str());

   RT(_diameterresolver, "sprout.cw-ngv.com").resolve("3.0.0.1:3868;transport=SCTP", 0);
}

