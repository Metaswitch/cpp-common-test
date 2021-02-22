/**
 * @file utils_test.cpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include <atomic>
#include <thread>
#include "gtest/gtest.h"
#include "basetest.hpp"
#include "utils.h"

#include "test_utils.hpp"
#include "test_interposer.hpp"

using namespace Utils;
using namespace std;

using ::testing::Matcher;
using ::testing::AllOf;
using ::testing::Gt;
using ::testing::Lt;

/// Fixture for UtilsTest.
class UtilsTest : public BaseTest
{
  UtilsTest()
  {
  }

  virtual ~UtilsTest()
  {
  }

  double nCm(int n, int m)
  {
    double r = 1.0;
    for (int i = 1; i <= m; i++)
    {
      r *= (double)(n - i + 1)/(double)i;
    }
    return r;
  }
};

TEST_F(UtilsTest, StripURIScheme)
{
  std::string before = "sip:alice@example.com";
  std::string after = "alice@example.com";
  EXPECT_EQ(after, strip_uri_scheme(before));
}

TEST_F(UtilsTest, StripURISchemeNoScheme)
{
  std::string before = "bob@example.com";
  std::string after = "bob@example.com";
  EXPECT_EQ(after, strip_uri_scheme(before));
}

TEST_F(UtilsTest, RemoveVisualSeparators)
{
  std::string before = "(123)456-789.1234";
  std::string after = "1234567891234";
  EXPECT_EQ(after, remove_visual_separators(before));
}

TEST_F(UtilsTest, IsUserNumericTrue)
{
  EXPECT_TRUE(is_user_numeric("3"));
  EXPECT_TRUE(is_user_numeric("+442083623893"));
  EXPECT_TRUE(is_user_numeric("02083623893"));
  EXPECT_TRUE(is_user_numeric("+44(208)3.6.2.[3893]"));
}

TEST_F(UtilsTest, IsUserNumericFalse)
{
  EXPECT_FALSE(is_user_numeric(""));
  EXPECT_FALSE(is_user_numeric("..."));
  EXPECT_FALSE(is_user_numeric(".+[]()"));
  EXPECT_FALSE(is_user_numeric("alice"));
  EXPECT_FALSE(is_user_numeric("alice319"));
  EXPECT_FALSE(is_user_numeric("+1233456789o"));
  EXPECT_FALSE(is_user_numeric("+1233456789o0"));
}

TEST_F(UtilsTest, ValidIPv4Address)
{
  EXPECT_EQ(IPAddressType::IPV4_ADDRESS, parse_ip_address("127.0.0.1"));
}

TEST_F(UtilsTest, ValidIPv4AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV4_ADDRESS_WITH_PORT, parse_ip_address("127.0.0.1:80"));
}

TEST_F(UtilsTest, ValidIPv6Address)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS, parse_ip_address("1234:1234:1234:1234:1234:1234:1234:1234"));
}

TEST_F(UtilsTest, ValidIPv6AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_WITH_PORT, parse_ip_address("[1234:1234:1234:1234:1234:1234:1234:1234]:80"));
}

TEST_F(UtilsTest, ValidIPv6AddressWithBracketsNoPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_BRACKETED, parse_ip_address("[1234:1234:1234:1234:1234:1234:1234:1234]"));
}

TEST_F(UtilsTest, CompressedIPv6Address)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS, parse_ip_address("1::1"));
}

TEST_F(UtilsTest, CompressedIPv6AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_WITH_PORT, parse_ip_address("[1::1]:80"));
}

TEST_F(UtilsTest, CompressedIPv6AddressWithBracketsNoPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_BRACKETED, parse_ip_address("[1::1]"));
}

TEST_F(UtilsTest, LocalIPv6Address)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS, parse_ip_address("::1"));
}

TEST_F(UtilsTest, LocalIPv6AddressWithPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_WITH_PORT, parse_ip_address("[::1]:80"));
}

TEST_F(UtilsTest, LocalIPv6AddressWithBracketsNoPort)
{
  EXPECT_EQ(IPAddressType::IPV6_ADDRESS_BRACKETED, parse_ip_address("[::1]"));
}

TEST_F(UtilsTest, InvalidAddress)
{
  EXPECT_EQ(IPAddressType::INVALID, parse_ip_address("327.0.0.1"));
}

TEST_F(UtilsTest, IsBracketedAddressNo)
{
  EXPECT_FALSE(is_bracketed_address("::1"));
}

TEST_F(UtilsTest, IsBracketedAddressYes)
{
  EXPECT_TRUE(is_bracketed_address("[::1]"));
}

TEST_F(UtilsTest, UriServerForIpV4WithDefaultPort)
{
  EXPECT_EQ("127.0.0.1:80", uri_address("127.0.0.1:80", 80));
}

TEST_F(UtilsTest, UriServerForIpV4WithSpecifiedPort)
{
  EXPECT_EQ("127.0.0.1:81", uri_address("127.0.0.1:81", 80));
}

TEST_F(UtilsTest, UriServerForIpV6WithDefaultPort)
{
  EXPECT_EQ("[::1]:80", uri_address("::1", 80));
}

TEST_F(UtilsTest, UriServerForIpV6BracketedWithDefaultPort)
{
  EXPECT_EQ("[::1]:80", uri_address("[::1]", 80));
}

TEST_F(UtilsTest, UriServerForIpV6WithSpecifiedPort)
{
  EXPECT_EQ("[::1]:81", uri_address("[::1]:81", 80));
}

TEST_F(UtilsTest, UriServerForHost)
{
  EXPECT_EQ("example.com:80", uri_address("example.com", 80));
}

TEST_F(UtilsTest, UriServerForHostWithPort)
{
  EXPECT_EQ("example.com:81", uri_address("example.com:81", 80));
}

TEST_F(UtilsTest, RemoveBracketsFromIPv4Address)
{
  EXPECT_EQ("127.0.0.1", remove_brackets_from_ip("127.0.0.1"));
}

TEST_F(UtilsTest, RemoveBracketsFromBracketedIPv6Address)
{
  EXPECT_EQ("::1", remove_brackets_from_ip("[::1]"));
}

TEST_F(UtilsTest, RemoveBracketsFromBareIPv6Address)
{
  EXPECT_EQ("::1", remove_brackets_from_ip("::1"));
}

// Test that basic parsing of IP addresses works
TEST_F(UtilsTest, ParseIPAddresses)
{
  AddrInfo ai;
  ai.port = 80;
  ai.transport = IPPROTO_TCP;

  EXPECT_TRUE(parse_ip_target("1.2.3.4", ai.address));
  EXPECT_EQ("1.2.3.4:80;transport=TCP", ai.to_string());

  EXPECT_TRUE(parse_ip_target("1:2::2", ai.address));
  EXPECT_EQ("[1:2::2]:80;transport=TCP", ai.to_string());

  EXPECT_TRUE(parse_ip_target("[1:2::2]", ai.address));
  EXPECT_EQ("[1:2::2]:80;transport=TCP", ai.to_string());

  EXPECT_FALSE(parse_ip_target("1.2.3.4:8888", ai.address));
}

TEST_F(UtilsTest, Split)
{
  list<string> tokens;
  Utils::split_string(" , really,long,,string,alright , ",
                      ',',
                      tokens);
  list<string> expected;
  expected.push_back(" ");
  expected.push_back(" really");
  expected.push_back("long");
  expected.push_back("string");
  expected.push_back("alright ");
  expected.push_back(" ");
  EXPECT_EQ(expected, tokens);

  tokens.clear();
  expected.clear();
  Utils::split_string("  long,g; string ",
                      ';',
                      tokens,
                      999,
                      true);
  expected.push_back("long,g");
  expected.push_back(" string");
  EXPECT_EQ(expected, tokens);

  tokens.clear();
  expected.clear();
  Utils::split_string(",,,",
                      ',',
                      tokens,
                      0,
                      true);
  EXPECT_EQ(expected, tokens);

  tokens.clear();
  expected.clear();
  Utils::split_string("",
                      ',',
                      tokens,
                      999,
                      false);
  EXPECT_EQ(expected, tokens);

  tokens.clear();
  expected.clear();
  Utils::split_string("a,b,,d,e",
                      ',',
                      tokens,
                      3,
                      false);
  expected.push_back("a");
  expected.push_back("b");
  expected.push_back(",d,e");
  EXPECT_EQ(expected, tokens);
}

TEST_F(UtilsTest, Quote)
{
  string actual = Utils::quote_string("");
  EXPECT_EQ("\"\"", actual);

  actual = Utils::quote_string("The quick brown fox \";'$?&=%\n\\\377");
  EXPECT_EQ("\"The quick brown fox \\\";'$?&=%\n\\\\\377\"", actual);

  actual = Utils::quote_string("\"\\");
  EXPECT_EQ("\"\\\"\\\\\"", actual);
}

TEST_F(UtilsTest, Escape)
{
  string actual = Utils::url_escape("");
  EXPECT_EQ("", actual);

  actual = Utils::url_escape("The quick brown fox \";'$?&=%\n\377");
  EXPECT_EQ("The%20quick%20brown%20fox%20%22%3B%27%24%3F%26%3D%25\n\377", actual);

  string input;
  string expected;
  for (unsigned int i = 32; i <= 127; i++)
  {
    char c = (char)i;
    input.push_back(c);
    if ((string("!#$&'()*+,/:;=?@[]").find(c) == string::npos) &&
        (string(" \"%<>\\^`{|}~").find(c) == string::npos))
    {
      expected.push_back(c);
    }
    else
    {
      char buf[4];
      sprintf(buf, "%%%02X", i);
      expected.append(buf);
    }
  }

  actual = Utils::url_escape(input);
  EXPECT_EQ(expected, actual);
}

TEST_F(UtilsTest, Unescape)
{
  // The only rule for url_unescape is that it should do the opposite
  // of url_escape.

  for (char c = 1; c < 127; c++)
  {
    std::string original(10, c);
    EXPECT_EQ(original, Utils::url_unescape(Utils::url_escape(original)));
  }
}

TEST_F(UtilsTest, XmlEscape)
{
  string actual = Utils::xml_escape("");
  EXPECT_EQ("", actual);

  actual = Utils::xml_escape("The quick brown fox &\"'<>\n\377");
  EXPECT_EQ("The quick brown fox &amp;&quot;&apos;&lt;&gt;\n\377", actual);
}

TEST_F(UtilsTest, Trim)
{
  string s = "    floop  ";
  string& t = Utils::ltrim(s);
  EXPECT_EQ(&s, &t);  // should return the same string
  EXPECT_EQ("floop  ", s);

  s = "  barp   ";
  t = Utils::rtrim(s);
  EXPECT_EQ(&s, &t);
  EXPECT_EQ("  barp", s);

  s = "";
  Utils::ltrim(s);
  EXPECT_EQ("", s);
  Utils::rtrim(s);
  EXPECT_EQ("", s);

  s = "xx   ";
  Utils::ltrim(s);
  EXPECT_EQ("xx   ", s);
  s = "   xx";
  Utils::rtrim(s);
  EXPECT_EQ("   xx", s);

  s = "    ";
  Utils::ltrim(s);
  EXPECT_EQ("", s);
  s = "    ";
  Utils::rtrim(s);
  EXPECT_EQ("", s);

  s = "   floop   ";
  t = Utils::trim(s);
  EXPECT_EQ(&s, &t);
  EXPECT_EQ("floop", s);

  s = "xy  zzy";
  Utils::trim(s);
  EXPECT_EQ("xy  zzy", s);

  s = "";
  Utils::trim(s);
  EXPECT_EQ("", s);
}


TEST_F(UtilsTest, ExponentialDistribution)
{
  double lambda = 1.0 / (double)300;
  Utils::ExponentialDistribution e(lambda);

  // Use a fixed seed to make the test deterministic.
  srand(2013);

  // Sample the distribution 10000 times.
  std::vector<double> x(10000);
  for (int i = 0; i < 10000; ++i)
  {
    x[i] = e();
    if (x[i] < 0)
    {
      printf("Bad value %g\n", x[i]);
      exit(1);
    }
  }

  // Calculate the observed mean and variance.
  double observed_mean = 0.0;
  for (int i = 0; i < 10000; ++i)
  {
    observed_mean += x[i];
  }
  observed_mean /= (double)10000;
  double observed_variance = 0.0;
  for (int i = 0; i < 10000; ++i)
  {
    observed_variance += (x[i] - observed_mean)*(x[i] - observed_mean);
  }
  observed_variance /= (double)10000;

  double expected_mean = 1.0 / lambda;
  double expected_variance = expected_mean*expected_mean;
  EXPECT_THAT(observed_mean, testing::AllOf(testing::Ge(expected_mean * 0.95), testing::Le(expected_mean * 1.05)));
  EXPECT_THAT(observed_variance, testing::AllOf(testing::Ge(expected_variance * 0.95), testing::Le(expected_variance * 1.05)));
}


TEST_F(UtilsTest, BinomialDistribution)
{
  int t = 10;
  double p = 0.1;
  Utils::BinomialDistribution b(t, p);
  std::vector<int> c(t+1);

  // Use a fixed seed to make the test deterministic.
  srand(2013);

  for (int i = 0; i < 10000; ++i)
  {
    int v = b();
    EXPECT_THAT(v, testing::AllOf(testing::Ge(0), testing::Le(t)));
    ++c[v];
  }

  // Test that the resulting distribution is close to the expected one.
  for (int i = 0; i <= t; ++i)
  {
    double expected = nCm(t,i) * pow(p, i) * pow(1-p, t-i);
    double observed = (double)c[i] / (double)10000;
    EXPECT_THAT(observed, testing::AllOf(testing::Ge(expected - 0.05), testing::Le(expected + 0.05)));
  }
}

/// Test the parse_stores_arg function with various input parameters.
TEST_F(UtilsTest, ParseStoresArg)
{
  std::vector<std::string> stores_arg = {"local_site=store0",
                                         "remote_site1=store1",
                                         "remote_site2=store2"};
  std::string local_site_name = "local_site";
  std::string local_store_location;
  std::vector<std::string> remote_stores_locations;

  bool ret = Utils::parse_stores_arg(stores_arg,
                                     local_site_name,
                                     local_store_location,
                                     remote_stores_locations);

  EXPECT_TRUE(ret);
  EXPECT_EQ(local_store_location, "store0");
  EXPECT_EQ(remote_stores_locations.size(), 2);
  EXPECT_EQ(remote_stores_locations[0], "store1");
  EXPECT_EQ(remote_stores_locations[1], "store2");

  // Vector is invalid since one of the stores is not identfied by a site.
  local_store_location = "";
  remote_stores_locations.clear();
  stores_arg = {"local_site=store0",
                "store1",
                "remote_site2=store2"};

  ret = Utils::parse_stores_arg(stores_arg,
                                local_site_name,
                                local_store_location,
                                remote_stores_locations);

  EXPECT_FALSE(ret);

  // Single site deployment.
  local_store_location = "";
  remote_stores_locations.clear();
  stores_arg = {"local_site=store0"};

  ret = Utils::parse_stores_arg(stores_arg,
                                local_site_name,
                                local_store_location,
                                remote_stores_locations);

  EXPECT_TRUE(ret);
  EXPECT_EQ(local_store_location, "store0");
  EXPECT_EQ(remote_stores_locations.size(), 0);

  // Single site deployment where no site is specified - parse_stores_arg
  // assumes it is the local site.
  local_store_location = "";
  remote_stores_locations.clear();
  stores_arg = {"store0"};

  ret = Utils::parse_stores_arg(stores_arg,
                                local_site_name,
                                local_store_location,
                                remote_stores_locations);

  EXPECT_TRUE(ret);
  EXPECT_EQ(local_store_location, "store0");
  EXPECT_EQ(remote_stores_locations.size(), 0);
}

//
// IOHook tests
//

TEST_F(UtilsTest, IOHookMainline)
{
  std::string reason1;
  std::string reason2;

  // Create an IO hook that just stores the reason off.
  IOHook hook([&reason1](const std::string& reason) { reason1 = reason; },
              [&reason2](const std::string& reason) { reason2 = reason; });

  CW_IO_STARTS("Kermit")
  {
    // Would normally do some blocking IO here.
  }
  CW_IO_COMPLETES();

  EXPECT_EQ(reason1, "Kermit");
  EXPECT_EQ(reason2, "Kermit");
}

TEST_F(UtilsTest, IOHookJustStartCallback)
{
  int count = 0;

  // Create an IO hook that only acts when IO starts.
  IOHook hook([&count](const std::string& _) { count++; },
              IOHook::NOOP_ON_COMPLETE);

  CW_IO_STARTS("Kermit")
  {
    // Would normally do some blocking IO here.
  }
  CW_IO_COMPLETES();

  EXPECT_EQ(count, 1);
}

TEST_F(UtilsTest, IOHookJustCompletesCallback)
{
  int count = 0;

  // Create an IO hook that only acts when IO starts.
  IOHook hook(IOHook::NOOP_ON_START,
              [&count](const std::string& reason) { count++; });

  CW_IO_STARTS("Kermit")
  {
    // Would normally do some blocking IO here.
  }
  CW_IO_COMPLETES();

  EXPECT_EQ(count, 1);
}

TEST_F(UtilsTest, MultipleIOHooks)
{
  int x = 0;

  // Create two hooks that do different operations on count.
  IOHook hook1(IOHook::NOOP_ON_START,
               [&x](const std::string& reason) { x += 1; });
  IOHook hook2(IOHook::NOOP_ON_START,
               [&x](const std::string& reason) { x *= 3; });

  CW_IO_STARTS("Kermit")
  {
    // Would normally do some blocking IO here.
  }
  CW_IO_COMPLETES();

  // Hooks form a stack so the last hook is invoked first. This means that the
  // result of the calculation should be ((0*3)+1) == 1
  EXPECT_EQ(x, 1);
}

TEST_F(UtilsTest, IOHooksGetCleanedUp)
{
  int x = 0;

  IOHook hook1(IOHook::NOOP_ON_START,
               [&x](const std::string& reason) { x += 2; });

  // Create a new scope with a hook. No IO is done in this scope so only the
  // first hook gets invoked.
  {
    IOHook hoo2k(IOHook::NOOP_ON_START,
                 [&x](const std::string& reason) { x += 3; });
  }

  CW_IO_STARTS("Kermit")
  {
    // Would normally do some blocking IO here.
  }
  CW_IO_COMPLETES();

  // Only the first hook gets triggered so x should be 2.
  EXPECT_EQ(x, 2);
}

TEST_F(UtilsTest, IOHooksMultipleIOOperations)
{
  int count = 0;

  IOHook hook(IOHook::NOOP_ON_START,
              [&count](const std::string& reason) { count++; });

  CW_IO_STARTS("One") { } CW_IO_COMPLETES();
  CW_IO_STARTS("Two") { } CW_IO_COMPLETES();
  CW_IO_STARTS("Three") { } CW_IO_COMPLETES();

  // Only the first hook gets triggered so x should be 2.
  EXPECT_EQ(count, 3);
}

TEST_F(UtilsTest, IOHooksArePerThread)
{
  int count = 0;
  std::atomic_bool terminate(false);

  std::thread t1([&count, &terminate] () {
    IOHook hook(IOHook::NOOP_ON_START,
                [&count](const std::string& reason) { count++; });

    while (!terminate.load()) { sleep(1); }
  });

  CW_IO_STARTS("One") { } CW_IO_COMPLETES();
  CW_IO_STARTS("Two") { } CW_IO_COMPLETES();
  CW_IO_STARTS("Three") { } CW_IO_COMPLETES();

  // The hook does not get triggered so the count should be 0
  EXPECT_EQ(count, 0);

  terminate.store(true);
  t1.join();
}

TEST_F(UtilsTest, IOMonitorCovertIOAllowed)
{
  EXPECT_TRUE(IOMonitor::thread_allows_covert_io());
  CW_IO_CALLS_REQUIRED();
  EXPECT_FALSE(IOMonitor::thread_allows_covert_io());

  // Reset whether covert IO is allowed.
  IOMonitor::set_thread_allows_covert_io(true);
}

TEST_F(UtilsTest, IOMonitorDoingOvertIO)
{
  EXPECT_FALSE(IOMonitor::thread_doing_overt_io());
  CW_IO_STARTS("IO 1")
  {
    EXPECT_TRUE(IOMonitor::thread_doing_overt_io());
    CW_IO_STARTS("IO 2")
    {
      EXPECT_TRUE(IOMonitor::thread_doing_overt_io());
    }
    CW_IO_COMPLETES();
    EXPECT_TRUE(IOMonitor::thread_doing_overt_io());
  }
  CW_IO_COMPLETES();
  EXPECT_FALSE(IOMonitor::thread_doing_overt_io());
}

class StopWatchTest : public ::testing::Test
{
public:
  StopWatchTest()
  {
    cwtest_completely_control_time();
  }

  virtual ~StopWatchTest()
  {
    cwtest_reset_time();
  }

  unsigned long ms_to_us(int ms) { return (unsigned long)(ms * 1000); }

  Utils::StopWatch _sw;
};

TEST_F(StopWatchTest, Mainline)
{
  EXPECT_TRUE(_sw.start());
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.stop());

  unsigned long elapsed_us;
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(11), elapsed_us);
}

TEST_F(StopWatchTest, StopIsIdempotent)
{
  EXPECT_TRUE(_sw.start());
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.stop());
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.stop());

  unsigned long elapsed_us;
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(11), elapsed_us);
}

TEST_F(StopWatchTest, ReadGetsLatestValueWhenNotStopped)
{
  EXPECT_TRUE(_sw.start());

  unsigned long elapsed_us;
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(11), elapsed_us);

  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.read(elapsed_us));
  // The returned value is greater on the second read.
  EXPECT_EQ(ms_to_us(22), elapsed_us);
}

// Test that reading a restarted stopwatch gives the right result.
TEST_F(StopWatchTest, StopStartNotIncludedInReading)
{
  EXPECT_TRUE(_sw.start());
  cwtest_advance_time_ms(11);

  // Stop and restart the stopwatch a few times. The time increases in between
  // should not be included in the final value read.
  EXPECT_TRUE(_sw.stop());
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.start());

  EXPECT_TRUE(_sw.stop());
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.start());

  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.stop());

  unsigned long elapsed_us;
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(22), elapsed_us);
}

// Test that reading a restarted stopwatch gives the right result, even when the
// read is done while the stopwatch is running.
TEST_F(StopWatchTest, StopStartThenReadWhenRunning)
{
  EXPECT_TRUE(_sw.start());
  cwtest_advance_time_ms(11);

  // Stop and restart the stopwatch. The time increase in between should not be
  // included in the final value read.
  EXPECT_TRUE(_sw.stop());
  cwtest_advance_time_ms(11);
  EXPECT_TRUE(_sw.start());

  cwtest_advance_time_ms(11);
  unsigned long elapsed_us;
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(22), elapsed_us);
}

// Test that substracting a time from the stopwatch gives the right result
TEST_F(StopWatchTest, SubtractTime)
{
  EXPECT_TRUE(_sw.start());
  cwtest_advance_time_ms(22);

  _sw.subtract_time(ms_to_us(11));

  unsigned long elapsed_us;
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(11), elapsed_us);
}

// Test that adding a time to the stopwatch gives the right result
TEST_F(StopWatchTest, AddTime)
{
  EXPECT_TRUE(_sw.start());
  cwtest_advance_time_ms(22);

  _sw.add_time(ms_to_us(11));

  unsigned long elapsed_us;
  EXPECT_TRUE(_sw.read(elapsed_us));
  EXPECT_EQ(ms_to_us(33), elapsed_us);
}
