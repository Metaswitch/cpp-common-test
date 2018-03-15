/**
 * @file logger_test.cpp UT for the RAM Recorder
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
#include "logger.h"
#include "test_utils.hpp"
#include "test_interposer.hpp"

using namespace std;

class RamRecorderTest : public ::testing::Test
{
private:
  std::string _dir;
  FILE* _f = nullptr;

public:
  virtual void SetUp() override
  {
    _dir = "/tmp/ramrecordertest-" + std::to_string(time(NULL));

    cwtest_completely_control_time(true);

    std::string rm = "rm -rf " + _dir;
    system(rm.c_str());
    std::string mkdir = "mkdir -p " + _dir;
    system(mkdir.c_str());

    RamRecorder::reset();
  }

  virtual void TearDown() override
  {
    if (_f)
    {
      fclose(_f);
    }

    std::string rm = "rm -rf " + _dir;
    system(rm.c_str());
    cwtest_reset_time();
  }

  void expect_line(const char* expected_line)
  {
    char linebuf[20480];
    char* line = fgets(linebuf, sizeof(linebuf), _f);
    EXPECT_TRUE(line != nullptr);
    EXPECT_EQ(strlen(expected_line), strlen(linebuf));
    EXPECT_STREQ(expected_line, linebuf);
  }

  std::string get_thread_id()
  {
    std::stringstream stream;
    stream << std::hex << pthread_self();
    return stream.str();
  }

  void expect_header()
  {
    expect_line("RAM BUFFER\n");
    expect_line("==========\n");
  }

  void expect_footer()
  {
    expect_line("==========\n");
  }

  void load_file()
  {
    std::string filename = _dir + "/ramtrace.0.txt";

    _f = fopen(filename.c_str(), "r");

    ASSERT_TRUE(_f != nullptr);
  }

  void expect_file(const char* line)
  {
    load_file();
    expect_header();
    expect_line(line);
    expect_footer();
  }
};

TEST_F(RamRecorderTest, ContextNoParams)
{
  RamRecorder::record_with_context(Log::INFO_LEVEL, "test.c", 1, "ctx", "test");
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC [" << get_thread_id() << "] Info test.c:1:ctx: test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, ContextNoLine)
{
  RamRecorder::record_with_context(Log::INFO_LEVEL, "test.c", 0, "ctx", "test");
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC [" << get_thread_id() << "] Info test.c:ctx: test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, NoModule)
{
  RamRecorder::record(Log::INFO_LEVEL, nullptr, 0, "test");
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC [" << get_thread_id() << "] Info test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, NoContextNoParams)
{
  RamRecorder::record(Log::INFO_LEVEL, "test.c", 1, "test");
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC [" << get_thread_id() << "] Info test.c:1: test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, NoLineNumber)
{
  RamRecorder::record(Log::INFO_LEVEL, "test.c", 0, "test");
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC [" << get_thread_id() << "] Info test.c: test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, Params)
{
  RamRecorder::record(Log::INFO_LEVEL,
                      "test.c", 0,
                      "test: %s %u %d %x %p", "hello", 1, -1, 0xA, NULL);
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC ["
         << get_thread_id()
         << "] Info test.c: test: hello 1 -1 a (nil)\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, ErrorLevel)
{
  RamRecorder::record(Log::ERROR_LEVEL,
                      "test.c", 0,
                      "test");
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC ["
         << get_thread_id()
         << "] Error test.c: test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, Truncation)
{
  int total = 20000;

  std::string truncation(20000, 'a');

  RamRecorder::record(Log::INFO_LEVEL,
                      "test.c", 0,
                      truncation.c_str());
  RamRecorder::dump(_dir);

  load_file();
  expect_header();
  std::string thrd_id = get_thread_id();

  int displayed = 8174 - thrd_id.length();

  {
    std::stringstream stream;
    stream << "01-01-1970 00:00:00.000 UTC ["
           << thrd_id
           << "] Info test.c: ";

    std::string truncated(displayed, 'a');

    stream << truncated << '\n';

    std::string line = stream.str();
    expect_line(line.c_str());
  }

  {
    std::stringstream stream;
    stream << "Earlier log was truncated by " << (total - displayed) << " characters\n";
    std::string line = stream.str();
    expect_line(line.c_str());
  }

  expect_footer();
}

TEST_F(RamRecorderTest, Write)
{
  RamRecorder::write("Test\n", 5);
  RamRecorder::dump(_dir);

  expect_file("Test\n");
}

TEST_F(RamRecorderTest, FailedDump)
{
  // Simulate inability to open files.
  cwtest_control_fopen(NULL);

  RamRecorder::write("Test\n", 5);
  RamRecorder::dump(_dir);

  // We don't expect anything in particular here, we just don't
  // want it to crash

  cwtest_release_fopen();
}

TEST_F(RamRecorderTest, FillBuffer)
{
  std::string fill(1023, '*');
  fill += "\n";
  for (int i = 0; i < 20*1024; ++i)
  {
    RamRecorder::write(fill.c_str(), 1024);
  }
  RamRecorder::dump(_dir);

  load_file();
  expect_header();
  for (int i = 0; i < 20*1024; ++i)
  {
    if (i == 0)
    {
      // Expect the first line to be truncated by one byte
      expect_line(fill.c_str() + 1);
    }
    else
    {
      expect_line(fill.c_str());
    }
  }
  expect_footer();
}

TEST_F(RamRecorderTest, OverFillBuffer)
{
  std::string fill(1023, '*');
  fill += "\n";
  for (int i = 0; i < 21*1024; ++i)
  {
    RamRecorder::write(fill.c_str(), 1024);
  }
  RamRecorder::dump(_dir);

  load_file();
  expect_header();
  for (int i = 0; i < 20*1024; ++i)
  {
    if (i == 0)
    {
      // Expect the first line to be truncated by one byte
      expect_line(fill.c_str() + 1);
    }
    else
    {
      expect_line(fill.c_str());
    }
  }
  expect_footer();
}

TEST_F(RamRecorderTest, DoubleFillBuffer)
{
  std::string fill(1023, '*');
  fill += "\n";
  for (int i = 0; i < 40*1024; ++i)
  {
    RamRecorder::write(fill.c_str(), 1024);
  }
  RamRecorder::dump(_dir);

  load_file();
  expect_header();
  for (int i = 0; i < 20*1024; ++i)
  {
    if (i == 0)
    {
      // Expect the first line to be truncated by one byte
      expect_line(fill.c_str() + 1);
    }
    else
    {
      expect_line(fill.c_str());
    }
  }
  expect_footer();
}

TEST_F(RamRecorderTest, ExactlyDoubleFillBuffer)
{
  std::string fill(1023, '*');
  fill += "\n";
  for (int i = 0; i < 40*1024; ++i)
  {
    if (i == 0)
    {
      RamRecorder::write(fill.c_str() + 1, 1023);
    }
    else
    {
      RamRecorder::write(fill.c_str(), 1024);
    }
  }
  RamRecorder::dump(_dir);

  load_file();
  expect_header();
  for (int i = 0; i < 20*1024; ++i)
  {
    if (i == 0)
    {
      // Expect the first line to be truncated by one byte
      expect_line(fill.c_str() + 1);
    }
    else
    {
      expect_line(fill.c_str());
    }
  }
  expect_footer();
}

TEST_F(RamRecorderTest, AlwaysMacro)
{
  TRC_RAMTRACE(Log::INFO_LEVEL, "test"); long line_no = __LINE__;
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC ["
         << get_thread_id()
         << "] Info ramrecorder_test.cpp:" << line_no << ": test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, MaybeOnMacro)
{
  RamRecorder::recordEverything();
  TRC_MAYBE_RAMTRACE(Log::INFO_LEVEL, "test"); long line_no = __LINE__;
  RamRecorder::dump(_dir);

  std::stringstream stream;
  stream << "01-01-1970 00:00:00.000 UTC ["
         << get_thread_id()
         << "] Info ramrecorder_test.cpp:" << line_no << ": test\n";
  std::string line = stream.str();
  expect_file(line.c_str());
}

TEST_F(RamRecorderTest, MaybeMacro)
{
  TRC_MAYBE_RAMTRACE(Log::INFO_LEVEL, "test");
  RamRecorder::dump(_dir);

  // Expect an empty file
  expect_file("No recorded logs\n");
}
