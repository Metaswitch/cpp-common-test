/**
 * @file dnscachedresolver_test.cpp UT for DnsCachedResolver class.
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
    DnsCachedResolver({"0.0.0.0"}, filename)
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
