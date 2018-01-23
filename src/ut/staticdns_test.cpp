/**
 * @file staticdns_test.cpp UT for StaticDnsCache class.
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
#include "staticdnscache.h"

using namespace std;

class StaticDnsCacheTest  : public ::testing::Test
{
};

TEST_F(StaticDnsCacheTest, Construction)
{
  StaticDnsCache cache("dns_json/a_records.json");
}


// If we try and do a CNAME lookup for a name not in the dns.json file, it
// should be untranslated.
TEST_F(StaticDnsCacheTest, DefaultCNAMELookup)
{
  StaticDnsCache cache("dns_json/a_records.json");

  std::string translated = cache.get_canonical_name("not.in.the.file");

  EXPECT_EQ(translated, "not.in.the.file");
}


// If we try and do a CNAME lookup for a name with only an A record, not a
// CNAME record, in the dns.json file, it should be untranslated.
TEST_F(StaticDnsCacheTest, CNAMELookupOnARecord)
{
  StaticDnsCache cache("dns_json/a_records.json");

  std::string translated = cache.get_canonical_name("a.records.domain");

  EXPECT_EQ(translated, "a.records.domain");
}


// If we try and do a CNAME lookup for a name with a CNAME record in the
// dns.json file, it should be translated.
TEST_F(StaticDnsCacheTest, CNAMELookup)
{
  StaticDnsCache cache("dns_json/a_records.json");

  std::string translated = cache.get_canonical_name("one.extra.domain");

  EXPECT_EQ(translated, "one.made.up.domain");
}


// If we try and do an A lookup for a name not in the dns.json file, it should
// return an empty result.
TEST_F(StaticDnsCacheTest, ARecordLookupNoEntries)
{
  StaticDnsCache cache("dns_json/a_records.json");

  std::vector<DnsResult> entries = cache.get_entries("not.in.the.file", ns_t_a);

  EXPECT_EQ(entries.size(), 0);
}


// If we try and do an A lookup for a name in the dns.json file, it should
// return an accurate list of results.
TEST_F(StaticDnsCacheTest, ARecordLookup)
{
  StaticDnsCache cache("dns_json/a_records.json");

  std::vector<DnsResult> entries = cache.get_entries("a.records.domain", ns_t_a);

  ASSERT_EQ(entries.size(), 1);

  DnsResult res = entries[0];

  // Expect two targets.
  EXPECT_EQ(res.domain(), "a.records.domain");
  EXPECT_EQ(res.dnstype(), ns_t_a);
  EXPECT_EQ(res.ttl(), 0);
  ASSERT_EQ(res.records().size(), 2);

  // First target should be "10.0.0.1"
  DnsRRecord* first_result = res.records()[0];

  ASSERT_EQ(first_result->rrtype(), ns_t_a);

  DnsARecord* first_result_a = dynamic_cast<DnsARecord*>(first_result);
  const char* address = inet_ntoa(first_result_a->address());
  EXPECT_EQ(address, "10.0.0.1");

  // Second target should be "10.0.0.2"
  DnsRRecord* second_result = res.records()[1];

  ASSERT_EQ(second_result->rrtype(), ns_t_a);

  DnsARecord* second_result_a = dynamic_cast<DnsARecord*>(second_result);
  const char* address2 = inet_ntoa(second_result_a->address());
  EXPECT_EQ(address2, "10.0.0.2");
}
