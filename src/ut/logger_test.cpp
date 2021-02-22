/**
 * @file logger_test.cpp UT for Sprout logger.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
#include "sas.h"
#include "logger.h"
#include "test_utils.hpp"
#include "test_interposer.hpp"

using namespace std;
using ::testing::HasSubstr;

/// Fixture for LoggerTest.
class LoggerTest : public ::testing::Test
{
  LoggerTest()
  {
    system("rm -f /tmp/logtest*");
  }

  virtual ~LoggerTest()
  {
    system("rm -f /tmp/logtest*");
  }
};

/// Subclass the class under test so we can override the current time.
class Logger2 : public Logger
{
  Logger2(const std::string& directory, const std::string& filename) :
    Logger(directory, filename),
    _time_sec(0),
    _time_nsec(0),
    _monotonic_time_sec(0),
    _monotonic_time_nsec(0)
  {
  }

  virtual ~Logger2()
  {
  }

  void gettime(struct timespec* ts)
  {
    if (_time_sec == 0 && _time_nsec == 0)
    {
      Logger::gettime(ts);
    }
    else
    {
      ts->tv_sec = _time_sec;
      ts->tv_nsec = _time_nsec;
    }
  }

  void gettime_monotonic(struct timespec* ts)
  {
    if (_monotonic_time_sec == 0 && _monotonic_time_nsec == 0)
    {
      Logger::gettime_monotonic(ts);
    }
    else
    {
      ts->tv_sec = _monotonic_time_sec;
      ts->tv_nsec = _monotonic_time_nsec;
    }
  }

  void settime(time_t time_sec, long time_nsec)
  {
    _time_sec = time_sec;
    _time_nsec = time_nsec;
  }

  void settime_monotonic(time_t time_sec, long time_nsec)
  {
    _monotonic_time_sec = time_sec;
    _monotonic_time_nsec = time_nsec;
  }

private:
  time_t _time_sec;
  long _time_nsec;

  time_t _monotonic_time_sec;
  long _monotonic_time_nsec;
};


TEST_F(LoggerTest, Mainline)
{
  Logger2 log("/tmp", "logtest");
  time_t midnight = 1356048000u; // 2012-12-21T00:00:00 UTC
  log.settime(midnight - 30, 123456789l);
  log.write("Some data goes here\n");
  EXPECT_EQ((int)Logger::ADD_TIMESTAMPS, log.get_flags());
  log.set_flags(0);
  log.settime(midnight - 20, 234567890l);
  log.write("Some more data goes there\n");
  log.set_flags(Logger::ADD_TIMESTAMPS);
  log.settime(midnight + 10, 345678901l);
  log.write("And on the next day\n");
  log.settime(midnight + 360, 456789012l);
  log.write("And yet more of course\n");
  log.flush();

  FILE* f;
  char linebuf[1024];
  char* line;

  f = fopen("/tmp/logtest_20121220T230000Z.txt", "r");
  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("20-12-2012 23:59:30.123 UTC Some data goes here\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("Some more data goes there\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_TRUE(line == NULL);
  fclose(f);

  f = fopen("/tmp/logtest_20121221T000000Z.txt", "r");
  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("21-12-2012 00:00:10.345 UTC And on the next day\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("21-12-2012 00:06:00.456 UTC And yet more of course\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_TRUE(line == NULL);
  fclose(f);

  log.write("Foo\n");
  // Not flushed yet
  f = fopen("/tmp/logtest_20121221T000000Z.txt", "r");
  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  ASSERT_TRUE(line != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  ASSERT_TRUE(line != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_TRUE(line == NULL);
  fclose(f);

  log.settime(midnight + 730, 0);
  log.set_flags(Logger::ADD_TIMESTAMPS | Logger::FLUSH_ON_WRITE);
  log.write("Bar\n");
  EXPECT_EQ(Logger::ADD_TIMESTAMPS | Logger::FLUSH_ON_WRITE, log.get_flags());

  f = fopen("/tmp/logtest_20121221T000000Z.txt", "r");
  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  ASSERT_TRUE(line != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  ASSERT_TRUE(line != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("21-12-2012 00:06:00.456 UTC Foo\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("21-12-2012 00:12:10.000 UTC Bar\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_TRUE(line == NULL);
  fclose(f);
}

TEST_F(LoggerTest, RealTime)
{
  Logger2 log("/tmp", "logtest");
  log.write("Wossat it sez for da test\n");
  log.flush();

  int rc = system("grep '^[0-3][0-9]-[0-1][0-9]-[0-9][0-9][0-9][0-9] ..:..:..\\.... UTC Wossat it sez for da test' /tmp/logtest_*.txt >/dev/null");
  EXPECT_EQ(0, WEXITSTATUS(rc));
}

TEST_F(LoggerTest, CycleLogsOnError)
{
  // Simulate inability to open files.
  cwtest_control_fopen(NULL);

  Logger2 log("/tmp", "logtest");
  time_t midnight = 1356048000u; // 2012-12-21T00:00:00 UTC
  log.settime(midnight, 0);
  log.settime_monotonic(midnight, 0);

  // Attempt to open a log file and fail.
  log.write("Log 1\n");

  // Now allow the logger to open a file.
  cwtest_release_fopen();

  // Log should not rotate after 3s.
  log.settime_monotonic(midnight + 3, 0);
  log.write("Log 2\n");

  // Log should rotate after 6s.
  log.settime_monotonic(midnight + 6, 0);
  log.write("Log 3\n");

  log.flush();

  FILE* f = fopen("/tmp/logtest_20121221T000000Z.txt", "r");
  char linebuf[1024];
  char* line;

  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("21-12-2012 00:00:00.000 UTC Failed to open logfile (2 - No such file or directory), 2 logs discarded\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("21-12-2012 00:00:00.000 UTC Log 3\n", line);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_TRUE(line == NULL);
  fclose(f);
}

// This test case is interesting because the logger will normally only attempt
// to open a log file if at least 5s have passed since time zero on the
// monotonic clock.
TEST_F(LoggerTest, StartNearTimeZero)
{
  Logger2 log("/tmp", "logtest");
  log.settime(2u, 0);
  log.settime_monotonic(2u, 0);

  log.write("Log 1\n");
  log.flush();

  FILE* f = fopen("/tmp/logtest_19700101T000000Z.txt", "r");
  char linebuf[1024];
  char* line;

  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_STREQ("01-01-1970 00:00:02.000 UTC Log 1\n", line);
  fclose(f);
}

TEST_F(LoggerTest, LongLine)
{
  // Logging long lines should cause them to be truncated, and a
  // message saying so to be logged

  Logger2 log("/tmp", "logtest");
  std::string long_line(9000, 'a');
  long_line += "should not see this";
  Logger* prev = Log::setLogger(&log);
  Log::write(1, "", 0, "%s", long_line.c_str());
  log.flush();
  Log::setLogger(prev);

  int rc = system("grep 'truncated' /tmp/logtest_*.txt >/dev/null");
  EXPECT_EQ(0, WEXITSTATUS(rc));

  int rc2 = system("grep 'should not see this' /tmp/logtest_*.txt >/dev/null");
  EXPECT_EQ(1, WEXITSTATUS(rc2));
}

// Test that the logging interface correctly deals with various C++ strings that
// are passed as an lvalue, an rvalue by explicit move, and an implicit rvalue.
static std::string get_name() { return "Kermit"; }

TEST_F(LoggerTest, CppStrings)
{
  Logger2 log("/tmp", "logtest");
  time_t midnight = 1356048000u; // 2012-12-21T00:00:00 UTC
  log.settime(midnight, 0l);

  // Install the new logger.
  Logger* old_logger = Log::setLogger(&log);

  std::string name = "Kermit";
  TRC_STATUS("Hello %s", name);
  TRC_STATUS("Hello again %s", std::move(name));
  TRC_STATUS("Goodbye %s", get_name());

  log.flush();

  FILE* f;
  char linebuf[1024];
  char* line;

  f = fopen("/tmp/logtest_20121221T000000Z.txt", "r");
  ASSERT_TRUE(f != NULL);
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_THAT(line, HasSubstr("Hello Kermit"));
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_THAT(line, HasSubstr("Hello again Kermit"));
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_THAT(line, HasSubstr("Goodbye Kermit"));
  line = fgets(linebuf, sizeof(linebuf), f);
  EXPECT_TRUE(line == NULL);
  fclose(f);

  // Restore the old logger.
  Log::setLogger(old_logger);
}
