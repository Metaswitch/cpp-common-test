/**
 * @file test_memcached_config.cpp Test reading a memcached config file.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <fstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "memcached_config.h"

class MemcachedConfigTest : public ::testing::Test
{
public:
  char _filename[64];
  int _fd;

  MemcachedConfigReader* _reader;

  virtual void SetUp()
  {
    // Create a temporary file to write the config to.
    strcpy(_filename, "./cluster_settings.XXXXXX");
    _fd = mkstemp(_filename);
    ASSERT_GT(_fd, 0)
      << "Got error " << _fd << " (" << strerror(errno) << ")";

    _reader = new MemcachedConfigFileReader(std::string(_filename));
  }

  virtual void TearDown()
  {
    delete _reader; _reader = NULL;
    close(_fd);
    unlink(_filename);
  }

  void write_config(const std::string cfg)
  {
    size_t num_bytes = ::write(_fd, cfg.c_str(), cfg.length());
    ASSERT_EQ(num_bytes, cfg.length())
      << "Got error " << num_bytes << " (" << strerror(errno) << ")";
  }
};

TEST_F(MemcachedConfigTest, AllSettings)
{
  write_config("servers=192.168.0.1:11211,192.168.0.2:11211\n"
               "new_servers=10.0.0.1:11211\n");

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
  EXPECT_EQ(config.servers.size(), 2u);
  EXPECT_EQ(config.servers[0], "192.168.0.1:11211");
  EXPECT_EQ(config.servers[1], "192.168.0.2:11211");
  EXPECT_EQ(config.new_servers.size(), 1u);
  EXPECT_EQ(config.new_servers[0], "10.0.0.1:11211");
  EXPECT_EQ(config.tombstone_lifetime, 1800);
}


TEST_F(MemcachedConfigTest, OptionalFieldsEmpty)
{
  write_config("servers=192.168.0.1:11211,192.168.0.2:11211");

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
  EXPECT_EQ(config.servers.size(), 2u);
  EXPECT_EQ(config.servers[0], "192.168.0.1:11211");
  EXPECT_EQ(config.servers[1], "192.168.0.2:11211");
  // new_servers defaults to an empty vector.
  EXPECT_EQ(config.new_servers.size(), 0u);
  EXPECT_EQ(config.tombstone_lifetime, 1800);
}


TEST_F(MemcachedConfigTest, TombstoneLifetime)
{
  write_config("servers=192.168.0.1:11211,192.168.0.2:11211\n"
               "new_servers=10.0.0.1:11211\n"
               "tombstone_lifetime=200");

  MemcachedConfig config;
  // We no longer support tombstone_lifetime as a valid setting in
  // cluster_settings, so this file should be rejected.
  EXPECT_FALSE(_reader->read_config(config));
}


TEST_F(MemcachedConfigTest, EmptyConfig)
{
  write_config("");

  MemcachedConfig config;
  EXPECT_FALSE(_reader->read_config(config));
}


TEST_F(MemcachedConfigTest, CorruptConfig)
{
  write_config("qwerty");

  MemcachedConfig config;
  EXPECT_FALSE(_reader->read_config(config));
}


TEST_F(MemcachedConfigTest, CorruptConfigIncorrectTokens)
{
  write_config("qw=er=ty");

  MemcachedConfig config;
  EXPECT_FALSE(_reader->read_config(config));
}


TEST_F(MemcachedConfigTest, MissingConfig)
{
  // For this test, destroy the reader and recreate it with a missing config
  // file
  delete _reader;
  _reader = new MemcachedConfigFileReader(std::string("NotARealFile"));
  MemcachedConfig config;
  EXPECT_FALSE(_reader->read_config(config));
}


TEST_F(MemcachedConfigTest, ServerListEmpty)
{
  // A blank server list should be valid and parseable - this is so that an empty
  // remote_cluster_settings file can be put in place, then updated without a
  // restart when GR config is learnt.
  write_config("servers=");

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
}


TEST_F(MemcachedConfigTest, NoServerLine)
{
  write_config("new_servers=a:11211");

  MemcachedConfig config;
  EXPECT_FALSE(_reader->read_config(config));
}

TEST_F(MemcachedConfigTest, OnlyNewServers)
{
  write_config("servers=\nnew_servers=a:11211");

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
  EXPECT_EQ(config.servers.size(), 0u);
  EXPECT_EQ(config.new_servers.size(), 1u);
}

TEST_F(MemcachedConfigTest, BothServerListsEmpty)
{
  // A blank server list should be valid and parseable - this is so that an empty
  // remote_cluster_settings file can be put in place, then updated without a
  // restart when GR config is learnt.
  write_config("servers=\nnew_servers=");

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
}

TEST_F(MemcachedConfigTest, Comments)
{
  write_config("servers=192.168.0.1:11211\n"
               "#comment"); 

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
}

TEST_F(MemcachedConfigTest, Whitespace)
{
  write_config(" # Comment\n"
               "servers=10.0.0.1:11211, 10.0.0.2:11211");

  MemcachedConfig config;
  EXPECT_TRUE(_reader->read_config(config));
  EXPECT_EQ(config.servers.size(), 2u);
  EXPECT_EQ(config.servers[0], "10.0.0.1:11211");
  EXPECT_EQ(config.servers[1], "10.0.0.2:11211");
}
