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
