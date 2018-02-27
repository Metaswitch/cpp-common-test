/**
 * @file dnscachedresolver_test.cpp UT for DnsCachedResolver class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_interposer.hpp"
#include "test_utils.hpp"
#include "dnscachedresolver.h"
#include "static_dns_cache.h"

using namespace std;
static std::string DNS_JSON_DIR = string(UT_DIR).append("/dns_json/");

std::vector<std::string> dns_servers = {"0.0.0.0"};
// We don't test the DnsCachedResolver directly as we want to be able to
// manually add entries to the DnsCache
class TestDnsCachedResolver : public  DnsCachedResolver
{
  TestDnsCachedResolver(string filename) :
    DnsCachedResolver(dns_servers, DnsCachedResolver::DEFAULT_TIMEOUT, filename)
  {
    reload_static_records();
    add_fake_entries_to_cache();
  }

  // Adds some A records to the cache
  void add_fake_entries_to_cache()
  {
    vector<string> domains = {"one.made.up.domain", "two.made.up.domain"};

    time_t expiry = time(NULL) + 1;
    SAS::TrailId no_trail = 0;

    for (vector<string>::const_iterator domain = domains.begin();
         domain != domains.end();
         ++domain)
    {
      DnsCacheEntryPtr ce = create_cache_entry(*domain, ns_t_a, no_trail);
      ce->expires = expiry;

      struct in_addr address;
      DnsARecord* record = new DnsARecord(*domain, 1000, address);
      add_record_to_cache(ce, record, no_trail);
    }
  }
};

class DnsCachedResolverTest : public ::testing::Test
{
public:
  DnsCachedResolverTest() :
    _resolver("")
  {
    cwtest_completely_control_time();
  }

  ~DnsCachedResolverTest()
  {
    cwtest_reset_time();
  }

  TestDnsCachedResolver _resolver;
};

TEST_F(DnsCachedResolverTest, SingleRecordLookup)
{
  TestDnsCachedResolver resolver("");

  string domain = "one.made.up.domain";
  DnsResult result = resolver.dns_query(domain, ns_t_a, 0);

  EXPECT_EQ(result.domain(), domain);
  EXPECT_EQ(result.records().size(), 1);
}

TEST_F(DnsCachedResolverTest, NoRecordLookup)
{
  TestDnsCachedResolver resolver("");

  string domain = "nonexistent.made.up.domain";
  DnsResult result = resolver.dns_query(domain, ns_t_a, 0);

  EXPECT_EQ(result.domain(), domain);
  EXPECT_EQ(result.records().size(), 0);
}

// When querying multiple records, DnsCachedResolver should return the results
// in the same order as the requested domains.
TEST_F(DnsCachedResolverTest, MultipleDomainOrdering)
{
  TestDnsCachedResolver resolver("");

  vector<string> domains = {"nonexistent.made.up.domain", "other.made.up.domain"};
  vector<DnsResult> results;
  resolver.dns_query(domains, ns_t_a, results, 0);

  EXPECT_EQ(results[0].domain(), domains[0]);
  EXPECT_EQ(results[0].records().size(), 0);

  EXPECT_EQ(results[1].domain(), domains[1]);
  EXPECT_EQ(results[1].records().size(), 0);
}

// When querying multiple records, DnsCachedResolver should return the results
// in the same order as the requested domains. This should be true even if some
// results come from the pre-provisioned dns.json file.
TEST_F(DnsCachedResolverTest, MultipleDomainOrderingJson)
{
  TestDnsCachedResolver resolver(string(DNS_JSON_DIR).append("/a_records.json"));

  vector<string> domains = {"nonexistent.made.up.domain", "a.records.domain", "other.made.up.domain"};
  vector<DnsResult> results;
  resolver.dns_query(domains, ns_t_a, results, 0);

  EXPECT_EQ(results[0].domain(), domains[0]);
  EXPECT_EQ(results[0].records().size(), 0);

  EXPECT_EQ(results[1].domain(), domains[1]);
  EXPECT_EQ(results[1].records().size(), 2);

  EXPECT_EQ(results[2].domain(), domains[2]);
  EXPECT_EQ(results[2].records().size(), 0);
}



TEST_F(DnsCachedResolverTest, ValidJsonRedirectedLookup)
{
  TestDnsCachedResolver resolver(string(DNS_JSON_DIR).append("/valid_dns_config.json"));

  DnsResult result = resolver.dns_query("one.extra.domain", ns_t_a, 0);

  // The lookup should have been redirected by the static CNAME record in the json
  // and retrieved one result record.
  EXPECT_EQ(result.domain(), "one.made.up.domain");
  EXPECT_EQ(result.records().size(), 1);
}

TEST_F(DnsCachedResolverTest, ValidJsonRedirectedLookupNoResult)
{
  TestDnsCachedResolver resolver(string(DNS_JSON_DIR).append("/valid_dns_config.json"));

  DnsResult result = resolver.dns_query("three.extra.domain", ns_t_a, 0);

  // The lookup should have been redirected by the static CNAME record in the json
  // but there should be no results
  EXPECT_EQ(result.domain(), "three.made.up.domain");
  EXPECT_EQ(result.records().size(), 0);
}

TEST_F(DnsCachedResolverTest, DuplicateJson)
{
  TestDnsCachedResolver resolver(string(DNS_JSON_DIR).append("/duplicate_dns_config.json"));

  DnsResult result = resolver.dns_query("one.duplicated.domain", ns_t_a, 0);

  // Only the first of the two duplicates should have been read in, and that
  // should be used for the redirection
  EXPECT_EQ(result.domain(), "one.made.up.domain");
  EXPECT_EQ(result.records().size(), 1);
}

TEST_F(DnsCachedResolverTest, JsonBadRrtype)
{
  TestDnsCachedResolver resolver(string(DNS_JSON_DIR).append("/bad_rrtype_dns_config.json"));

  DnsResult result = resolver.dns_query("one.redirected.domain", ns_t_a, 0);

  // The first entry with a missing "rrtype" member and the A record should have
  // been skipped, but the valid CNAME record should have been read in
  EXPECT_EQ(result.domain(), "one.made.up.domain");
  EXPECT_EQ(result.records().size(), 1);
}

TEST_F(DnsCachedResolverTest, NXDomainTTL)
{
  // Tests that if we get NXDOMAIN and the SOA has a TTL, we'll cache that

  // The domain we'll query
  std::string domain = "abc-abc.abc.cw-ngv.com";

  // Hex representation of a DNS NXDOMAIN response for abc-abc.abc.cw-ngv.com
  // with 60s TTL
  unsigned char dns_response[] = {
    0xf2, 0x6a, // Transction ID
    0x81, 0x83, // Flags (Standard Query Response, No such name)
    0x00, 0x01, // One Question
    0x00, 0x00, // Zero Answer RRs
    0x00, 0x01, // One Authority RR
    0x00, 0x00, // Zero Additional RRs

    // Query: for abc-abc.abc.cw-ngv.com
    0x07, 0x61, 0x62, 0x63, 0x2d, 0x61, 0x62, 0x63, 0x03, 0x61, 0x62, 0x63,
    0x06, 0x63, 0x77, 0x2d, 0x6e, 0x67, 0x76, 0x03, 0x63, 0x6f, 0x6d, 0x00,
    0x00, 0x01, // Type A
    0x00, 0x01, // Class IN

    // Authority RR
    0xc0, 0x18,
    0x00, 0x06, // Type SOA
    0x00, 0x01, // Class IN
    0x00, 0x00, 0x00, 0x3c, // TTL: 60
    0x00, 0x46, 0x07, 0x6e, 0x73, 0x2d, 0x31, 0x32, 0x37, 0x35, 0x09, 0x61,
    0x77, 0x73, 0x64, 0x6e, 0x73, 0x2d, 0x33, 0x31, 0x03, 0x6f, 0x72, 0x67,
    0x00, 0x11, 0x61, 0x77, 0x73, 0x64, 0x6e, 0x73, 0x2d, 0x68, 0x6f, 0x73,
    0x74, 0x6d, 0x61, 0x73, 0x74, 0x65, 0x72, 0x06, 0x61, 0x6d, 0x61, 0x7a,
    0x6f, 0x6e, 0xc0, 0x1f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x1c, 0x20,
    0x00, 0x00, 0x03, 0x84, 0x00, 0x12, 0x75, 0x00, 0x00, 0x01, 0x51, 0x80
  };

  // First, create a pending entry in the cache that will be filled in when we
  // parse the response
  SAS::TrailId no_trail = 0;
  _resolver.create_cache_entry(domain, ns_t_a, no_trail);

  // Verify that the expiry time is 0
  std::shared_ptr<DnsCachedResolver::DnsCacheEntry> ce = _resolver.get_cache_entry(domain, ns_t_a);
  ASSERT_EQ(0, ce->expires);

  // Now, pretend we got the above response and parse it
  _resolver.dns_response(domain, ns_t_a, ARES_ENOTFOUND, dns_response, 122, 0);

  // Check that the cache entry has an expiry time of 60s from now
  ASSERT_EQ(time(NULL) + 60, ce->expires);
}

TEST_F(DnsCachedResolverTest, NXDomainTTLMoreThan300)
{
  // Tests that if we get NXDOMAIN and the SOA has a TTL larger than 300s, we
  // cap it to 300

  // The domain we'll query
  std::string domain = "abc-abc.abc.cw-ngv.com";

  // Hex representation of a DNS NXDOMAIN response for abc-abc.abc.cw-ngv.com
  // with 60s TTL
  unsigned char dns_response[] = {
    0xf2, 0x6a, // Transction ID
    0x81, 0x83, // Flags (Standard Query Response, No such name)
    0x00, 0x01, // One Question
    0x00, 0x00, // Zero Answer RRs
    0x00, 0x01, // One Authority RR
    0x00, 0x00, // Zero Additional RRs

    // Query: for abc-abc.abc.cw-ngv.com
    0x07, 0x61, 0x62, 0x63, 0x2d, 0x61, 0x62, 0x63, 0x03, 0x61, 0x62, 0x63,
    0x06, 0x63, 0x77, 0x2d, 0x6e, 0x67, 0x76, 0x03, 0x63, 0x6f, 0x6d, 0x00,
    0x00, 0x01, // Type A
    0x00, 0x01, // Class IN

    // Authority RR
    0xc0, 0x18,
    0x00, 0x06, // Type SOA
    0x00, 0x01, // Class IN
    0x00, 0x00, 0x01, 0xf4, // TTL: 500
    0x00, 0x46, 0x07, 0x6e, 0x73, 0x2d, 0x31, 0x32, 0x37, 0x35, 0x09, 0x61,
    0x77, 0x73, 0x64, 0x6e, 0x73, 0x2d, 0x33, 0x31, 0x03, 0x6f, 0x72, 0x67,
    0x00, 0x11, 0x61, 0x77, 0x73, 0x64, 0x6e, 0x73, 0x2d, 0x68, 0x6f, 0x73,
    0x74, 0x6d, 0x61, 0x73, 0x74, 0x65, 0x72, 0x06, 0x61, 0x6d, 0x61, 0x7a,
    0x6f, 0x6e, 0xc0, 0x1f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x1c, 0x20,
    0x00, 0x00, 0x03, 0x84, 0x00, 0x12, 0x75, 0x00, 0x00, 0x01, 0x51, 0x80
  };

  // First, create a pending entry in the cache that will be filled in when we
  // parse the response
  SAS::TrailId no_trail = 0;
  _resolver.create_cache_entry(domain, ns_t_a, no_trail);

  // Verify that the expiry time is 0
  std::shared_ptr<DnsCachedResolver::DnsCacheEntry> ce = _resolver.get_cache_entry(domain, ns_t_a);
  ASSERT_EQ(0, ce->expires);

  // Now, pretend we got the above response and parse it
  _resolver.dns_response(domain, ns_t_a, ARES_ENOTFOUND, dns_response, 122, 0);

  // Check that the cache entry has an expiry time of 300s from now
  ASSERT_EQ(time(NULL) + 300, ce->expires);
}
