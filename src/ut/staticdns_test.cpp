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

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_interposer.hpp"
#include "test_utils.hpp"
#include "static_dns_cache.h"

using namespace std;

static std::string DNS_JSON_DIR = string(UT_DIR).append("/dns_json/");
static std::string DNS_JSON_TMP_DIR = string(UT_DIR).append("/dns_json/tmp/");

class StaticDnsCacheTest  : public ::testing::Test
{
  void TearDown()
  {
    // If we've created temporary JSON files (for config reload tests), delete
    // them here.
    DIR* temp_dir = opendir(DNS_JSON_TMP_DIR.c_str());
    struct dirent* entry = readdir(temp_dir);
    while (entry)
    {
      std::string filename = DNS_JSON_TMP_DIR + entry->d_name;

      // Check whether it ends with ".json", by checking whether the
      // 5-character substring ".json" appears 5 characters from the end.
      if (filename.rfind(".json") == filename.size() - 5)
      {
        unlink(filename.c_str());
      }
      entry = readdir(temp_dir);
    }
    closedir(temp_dir);
  }

  std::string extract_a_record(DnsRRecord* result)
  {
    DnsARecord* result_a = dynamic_cast<DnsARecord*>(result);
    const char* addr = inet_ntoa(result_a->address());
    return addr;
  }
};

TEST_F(StaticDnsCacheTest, Construction)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");
}


// If we try and do a CNAME lookup for a name not in the dns.json file, it
// should be untranslated.
TEST_F(StaticDnsCacheTest, DefaultCNAMELookup)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  std::string translated = cache.get_canonical_name("not.in.the.file");

  EXPECT_EQ(translated, "not.in.the.file");
}


// If we try and do a CNAME lookup for a name with only an A record, not a
// CNAME record, in the dns.json file, it should be untranslated.
TEST_F(StaticDnsCacheTest, CNAMELookupOnARecord)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  std::string translated = cache.get_canonical_name("a.records.domain");

  EXPECT_EQ(translated, "a.records.domain");
}


// If we try and do a CNAME lookup for a name with a CNAME record in the
// dns.json file, it should be translated.
TEST_F(StaticDnsCacheTest, CNAMELookup)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  std::string translated = cache.get_canonical_name("one.extra.domain");

  EXPECT_EQ(translated, "one.made.up.domain");
}

// If we try and do an A lookup for a name not in the dns.json file, it should
// return an empty result.
TEST_F(StaticDnsCacheTest, ARecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_a);

  EXPECT_EQ(res.records().size(), 0);
}


// If we try and do an A lookup for a name in the dns.json file, it should
// return an accurate list of results.
TEST_F(StaticDnsCacheTest, ARecordLookup)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);

  // Expect two targets.
  EXPECT_EQ(res.domain(), "a.records.domain");
  EXPECT_EQ(res.dnstype(), ns_t_a);
  EXPECT_EQ(res.ttl(), 0);
  ASSERT_EQ(res.records().size(), 2);

  // First target should be "10.0.0.1"
  DnsRRecord* first_result = res.records()[0];

  ASSERT_EQ(first_result->rrtype(), ns_t_a);

  EXPECT_EQ(extract_a_record(first_result), "10.0.0.1");

  // Second target should be "10.0.0.2"
  DnsRRecord* second_result = res.records()[1];

  ASSERT_EQ(second_result->rrtype(), ns_t_a);

  EXPECT_EQ(extract_a_record(second_result), "10.0.0.2");
}

TEST_F(StaticDnsCacheTest, DuplicateIPAddressesAllowed)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records_duplicate.json");

  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);

  // Expect two targets.
  EXPECT_EQ(res.domain(), "a.records.domain");
  EXPECT_EQ(res.dnstype(), ns_t_a);
  EXPECT_EQ(res.ttl(), 0);
  ASSERT_EQ(res.records().size(), 2);

  // First target should be "10.0.0.3"
  DnsRRecord* first_result = res.records()[0];

  ASSERT_EQ(first_result->rrtype(), ns_t_a);

  EXPECT_EQ(extract_a_record(first_result), "10.0.0.3");

  // Second target should be "10.0.0.2"
  DnsRRecord* second_result = res.records()[1];

  ASSERT_EQ(second_result->rrtype(), ns_t_a);

  EXPECT_EQ(extract_a_record(second_result), "10.0.0.3");
}



// The lack of a dns.json file shouldn't break things.
TEST_F(StaticDnsCacheTest, CopesWithNoJSONFile)
{
  StaticDnsCache cache("this_file_does_not_exist.json");

  std::string translated = cache.get_canonical_name("not.in.the.file");
  EXPECT_EQ(translated, "not.in.the.file");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_a);
  EXPECT_EQ(res.records().size(), 0);
}


// If we try and do an AAAA lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, AAAARecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_aaaa);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_aaaa);
  EXPECT_EQ(res2.records().size(), 0);
}

// If we try and do an SRV lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, SRVRecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_srv);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_srv);
  EXPECT_EQ(res2.records().size(), 0);
}


// If we try and do a NS lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, NSRecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_ns);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_ns);
  EXPECT_EQ(res2.records().size(), 0);
}

// If we try and do an SOA lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, SOARecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_soa);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_soa);
  EXPECT_EQ(res2.records().size(), 0);
}

// If we try and do a PTR lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, PTRRecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_ptr);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_ptr);
  EXPECT_EQ(res2.records().size(), 0);
}

// If we try and do a NAPTR lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, NAPTRRecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_naptr);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_naptr);
  EXPECT_EQ(res2.records().size(), 0);
}

// If we try and do a CNAME lookup, it should return an empty result.
TEST_F(StaticDnsCacheTest, CNAMERecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", ns_t_cname);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_cname);
  EXPECT_EQ(res2.records().size(), 0);
}

// If we try and do a lookup for an unrecognised DNS type, it should return an
// empty result.
TEST_F(StaticDnsCacheTest, UnknownTypeRecordLookupNoEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "a_records.json");

  DnsResult res = cache.get_static_dns_records("not.in.the.file", INT_MAX);
  EXPECT_EQ(res.records().size(), 0);

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", INT_MAX);
  EXPECT_EQ(res2.records().size(), 0);
}


TEST_F(StaticDnsCacheTest, ConfigReloadAddsEntries)
{
  std::string target_file = DNS_JSON_TMP_DIR + "reload.json";
  StaticDnsCache cache(target_file.c_str());

  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res.records().size(), 0);

  std::string new_file = DNS_JSON_DIR + "a_records.json";
  std::string cp_command = "cp " + new_file + " " + target_file;
  int rc = system(cp_command.c_str());
  ASSERT_EQ(0, rc);
  cache.reload_static_records();

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res2.records().size(), 2);
}

TEST_F(StaticDnsCacheTest, ConfigReloadRemovesEntries)
{
  std::string target_file = DNS_JSON_TMP_DIR + "reload.json";
  StaticDnsCache cache(target_file.c_str());

  // Put a file into place with "a.records.domain" defined.
  {
    std::string new_file = DNS_JSON_DIR + "a_records.json";
    std::string cp_command = "cp " + new_file + " " + target_file;
    int rc = system(cp_command.c_str());
    ASSERT_EQ(0, rc);
    cache.reload_static_records();
  }

  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res.records().size(), 2);

  // Now put a file into place with "a.records.domain" not defined.
  {
    std::string new_file = DNS_JSON_DIR + "bad_rrtype_dns_config.json";
    std::string cp_command = "cp " + new_file + " " + target_file;
    int rc = system(cp_command.c_str());
    ASSERT_EQ(0, rc);
    cache.reload_static_records();
  }

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res2.records().size(), 0);
}

TEST_F(StaticDnsCacheTest, ConfigReloadChangesEntries)
{
  std::string target_file = DNS_JSON_TMP_DIR + "reload.json";
  StaticDnsCache cache(target_file.c_str());

  // Put a file into place with "a.records.domain" defined.
  {
    std::string new_file = DNS_JSON_DIR + "a_records.json";
    std::string cp_command = "cp " + new_file + " " + target_file;
    int rc = system(cp_command.c_str());
    ASSERT_EQ(0, rc);
    cache.reload_static_records();
  }

  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res.records().size(), 2);

  // First target should be "10.0.0.1"
  DnsRRecord* first_result = res.records()[0];
  ASSERT_EQ(first_result->rrtype(), ns_t_a);
  EXPECT_EQ(extract_a_record(first_result), "10.0.0.1");

  // Now put a file into place with "a.records.domain" defined differently.
  {
    std::string new_file = DNS_JSON_DIR + "a_records2.json";
    std::string cp_command = "cp " + new_file + " " + target_file;
    int rc = system(cp_command.c_str());
    ASSERT_EQ(0, rc);
    cache.reload_static_records();
  }

  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res2.records().size(), 3);

  // First target should be "10.16.16.16"
  first_result = res2.records()[0];
  ASSERT_EQ(first_result->rrtype(), ns_t_a);
  EXPECT_EQ(extract_a_record(first_result), "10.16.16.16");
}

// If we can't parse the file (e.g. it doesn't exist or is corrupt), the eold
// entries should continue to exist.
TEST_F(StaticDnsCacheTest, ConfigReloadSurvivesDeletion)
{
  std::string target_file = DNS_JSON_TMP_DIR + "reload.json";
  StaticDnsCache cache(target_file.c_str());

  // Put a file into place with "a.records.domain" defined.
  {
    std::string new_file = DNS_JSON_DIR + "a_records.json";
    std::string cp_command = "cp " + new_file + " " + target_file;
    int rc = system(cp_command.c_str());
    ASSERT_EQ(0, rc);
    cache.reload_static_records();
  }

  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res.records().size(), 2);

  // First target should be "10.0.0.1"
  DnsRRecord* first_result = res.records()[0];
  ASSERT_EQ(first_result->rrtype(), ns_t_a);
  EXPECT_EQ(extract_a_record(first_result), "10.0.0.1");

  // Delete that file and try and reload it.
  unlink(target_file.c_str());
  cache.reload_static_records();

  // The cache should stay in the same state.
  DnsResult res2 = cache.get_static_dns_records("a.records.domain", ns_t_a);
  ASSERT_EQ(res2.records().size(), 2);

  first_result = res2.records()[0];
  ASSERT_EQ(first_result->rrtype(), ns_t_a);
  EXPECT_EQ(extract_a_record(first_result), "10.0.0.1");
}

TEST_F(StaticDnsCacheTest, InvalidJson)
{
  StaticDnsCache cache(DNS_JSON_DIR + "invalid_dns_config.json");

  EXPECT_EQ(cache.size(), 0);
}

TEST_F(StaticDnsCacheTest, DuplicateCNAME)
{
  StaticDnsCache cache(DNS_JSON_DIR + "duplicate_dns_config.json");

  // Only the first of the two duplicates should have been read in, and that
  // should be used for the redirection
  EXPECT_EQ(cache.size(), 1);
  EXPECT_EQ(cache.get_canonical_name("one.duplicated.domain"), "one.made.up.domain");
}

TEST_F(StaticDnsCacheTest, DuplicateARecord)
{
  StaticDnsCache cache(DNS_JSON_DIR + "duplicate_dns_config_a.json");

  // Only the first of the two duplicates should have been read in, and that
  // should be used for the lookup.
  EXPECT_EQ(cache.size(), 1);
  DnsResult res = cache.get_static_dns_records("a.records.domain", ns_t_a);

  ASSERT_EQ(res.records().size(), 1);
  // First target should be "10.0.0.1"
  DnsRRecord* first_result = res.records()[0];
  ASSERT_EQ(first_result->rrtype(), ns_t_a);
  EXPECT_EQ(extract_a_record(first_result), "10.0.0.1");
}

TEST_F(StaticDnsCacheTest, JsonBadRrtype)
{
  StaticDnsCache cache(DNS_JSON_DIR + "bad_rrtype_dns_config.json");

  // The first entry with a missing "rrtype" member, and the second entry with
  // type UNKNOWN should have been skipped, but the valid CNAME record should
  // have been read in.
  EXPECT_EQ(cache.size(), 1);
  EXPECT_EQ(cache.get_canonical_name("one.redirected.domain"), "one.made.up.domain");
}

TEST_F(StaticDnsCacheTest, JsonMultipleEntries)
{
  StaticDnsCache cache(DNS_JSON_DIR + "multiple_rrtypes_for_name.json");

  EXPECT_EQ(cache.get_canonical_name("one.redirected.domain"), "one.made.up.domain");
  DnsResult res = cache.get_static_dns_records("one.redirected.domain", ns_t_a);
  ASSERT_EQ(res.records().size(), 1);

  // First target should be "10.10.10.10"
  DnsRRecord* first_result = res.records()[0];

  ASSERT_EQ(first_result->rrtype(), ns_t_a);

  EXPECT_EQ(extract_a_record(first_result), "10.10.10.10");
}

TEST_F(StaticDnsCacheTest, MissingHostnamesJson)
{
  StaticDnsCache cache(DNS_JSON_DIR + "missing_hostnames.json");

  EXPECT_EQ(cache.size(), 0);
}

TEST_F(StaticDnsCacheTest, BadNameJson)
{
  StaticDnsCache cache(DNS_JSON_DIR + "bad_name.json");

  // The first entry with a missing "name" member, but the valid CNAME record should
  // have been read in.
  EXPECT_EQ(cache.size(), 1);

  EXPECT_EQ(cache.get_canonical_name("two.redirected.domain"), "two.made.up.domain");
}
