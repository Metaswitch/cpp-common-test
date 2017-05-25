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

using namespace std;

// We don't test the DnsCachedResolver directly as we want to be able to
// manually add entries to the DnsCache
class TestDnsCachedResolver : public  DnsCachedResolver
{
  TestDnsCachedResolver(string filename) :
    DnsCachedResolver({"0.0.0.0"}, DnsCachedResolver::DEFAULT_TIMEOUT, filename)
  {
    reload_static_records();
    add_fake_entries_to_cache();
  }

  // Adds some A records to the cache
  void add_fake_entries_to_cache()
  {
    vector<string> domains = {"one.made.up.domain", "two.made.up.domain"};

    time_t expiry = time(NULL) + 1;

    for (vector<string>::const_iterator domain = domains.begin();
         domain != domains.end();
         ++domain)
    {
      DnsCacheEntryPtr ce = create_cache_entry(*domain, ns_t_a);
      ce->expires = expiry;

      struct in_addr address;
      DnsARecord* record = new DnsARecord(*domain, 1000, address);
      add_record_to_cache(ce, record);
    }
  }

  map<string, vector<DnsRRecord*>> static_records()
  {
    return _static_records;
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

TEST_F(DnsCachedResolverTest, MissingJson)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/nonexistent_file.json"));

  EXPECT_EQ(resolver.static_records().size(), 0);
}

TEST_F(DnsCachedResolverTest, ValidJson)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/valid_dns_config.json"));

  EXPECT_EQ(resolver.static_records().size(), 3);
}

TEST_F(DnsCachedResolverTest, InvalidJson)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/invalid_dns_config.json"));

  EXPECT_EQ(resolver.static_records().size(), 0);
}

TEST_F(DnsCachedResolverTest, ValidJsonRedirectedLookup)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/valid_dns_config.json"));

  DnsResult result = resolver.dns_query("one.extra.domain", ns_t_a, 0);

  // The lookup should have been redirected by the static CNAME record in the json
  // and retrieved one result record.
  EXPECT_EQ(result.domain(), "one.made.up.domain");
  EXPECT_EQ(result.records().size(), 1);
}

TEST_F(DnsCachedResolverTest, ValidJsonRedirectedLookupNoResult)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/valid_dns_config.json"));

  DnsResult result = resolver.dns_query("three.extra.domain", ns_t_a, 0);

  // The lookup should have been redirected by the static CNAME record in the json
  // but there should be no results
  EXPECT_EQ(result.domain(), "three.made.up.domain");
  EXPECT_EQ(result.records().size(), 0);
}

TEST_F(DnsCachedResolverTest, DuplicateJson)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/duplicate_dns_config.json"));

  DnsResult result = resolver.dns_query("one.duplicated.domain", ns_t_a, 0);

  // Only the first of the two duplicates should have been read in, and that
  // should be used for the redirection
  EXPECT_EQ(resolver.static_records().size(), 1);
  EXPECT_EQ(resolver.static_records().at("one.duplicated.domain").size(), 1);
  EXPECT_EQ(result.domain(), "one.made.up.domain");
  EXPECT_EQ(result.records().size(), 1);
}

TEST_F(DnsCachedResolverTest, JsonBadRrtype)
{
  TestDnsCachedResolver resolver(string(UT_DIR).append("/bad_rrtype_dns_config.json"));

  DnsResult result = resolver.dns_query("one.redirected.domain", ns_t_a, 0);

  // The first entry with a missing "rrtype" member and the A record should have
  // been skipped, but the valid CNAME record should have been read in
  EXPECT_EQ(resolver.static_records().size(), 1);
  EXPECT_EQ(resolver.static_records().at("one.redirected.domain").size(), 1);
  EXPECT_EQ(result.domain(), "one.made.up.domain");
  EXPECT_EQ(result.records().size(), 1);
}
