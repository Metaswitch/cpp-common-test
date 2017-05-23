/**
 * @file test_memcachedstoreview.cpp Tests for the Memcached Store View class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "memcachedstoreview.h"

using ::testing::AnyOf;
using ::testing::Contains;

// Function that expects a valid replica list.
//
// @param - The list to check.
// @param - The expected length of the list.
// @param - A matcher that should match every element of the list.  This will
//          typically be a std::string (if the caller expects a list
//          containing a single server) or an AnyOf matcher (listing the
//          possible replicas).
template<class M>
void expect_replica_list(MemcachedStoreView::ReplicaList replicas,
                         size_t count,
                         M matcher)
{
  // Check the count.
  EXPECT_EQ(replicas.size(), count);

  // Check each entry is unique. Do this by sorting the list, unique-ing it and
  // checking the length has not changed.
  std::sort(replicas.begin(), replicas.end());
  std::unique(replicas.begin(), replicas.end());
  EXPECT_EQ(replicas.size(), count);

  // Check each entry of the list is allowed.
  for (size_t i = 0; i < count; ++i)
  {
    EXPECT_THAT(replicas[i], matcher);
  }
}

#define EXPECT_REPLICA_LIST(LIST, COUNT, MATCHER)                              \
{                                                                              \
  SCOPED_TRACE("Checking replica list");                                       \
  expect_replica_list((LIST), (COUNT), (MATCHER));                             \
}

//
// Base fixture for all our tests.
//
const int NUM_VBUCKETS = 128;
const int NUM_REPLICAS = 2;

class MemcachedStoreViewTest : public ::testing::Test
{
  MemcachedStoreViewTest() : _view(NUM_VBUCKETS, NUM_REPLICAS) {}
  MemcachedStoreView _view;
};

//
// Fixture that sets up a single server.
//
class SingleServerTest : public MemcachedStoreViewTest
{
  void SetUp()
  {
    MemcachedConfig cfg;
    cfg.servers.push_back("localhost:40001");
    _view.update(cfg);
  }
};

TEST_F(SingleServerTest, OneCurrentReplica)
{
  std::map<int, MemcachedStoreView::ReplicaList> replicas = _view.current_replicas();
  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    EXPECT_REPLICA_LIST(replicas[i], 1u, "localhost:40001");
  }
}

TEST_F(SingleServerTest, NoNewReplicas)
{
  EXPECT_TRUE(_view.new_replicas().empty());
}

TEST_F(SingleServerTest, MovesEmpty)
{
  EXPECT_TRUE(_view.calculate_vbucket_moves().empty());
}

//
// Fixture that sets up a single server. This mimics the state where all the
// nodes are new
//
class SingleNewServerTest : public MemcachedStoreViewTest
{
  void SetUp()
  {
    MemcachedConfig cfg;
    cfg.new_servers.push_back("localhost:40001");
    _view.update(cfg);
  }
};

TEST_F(SingleNewServerTest, OneCurrentReplica)
{
  std::map<int, MemcachedStoreView::ReplicaList> replicas = _view.current_replicas();
  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    EXPECT_REPLICA_LIST(replicas[i], 1u, "localhost:40001");
  }
}

//
// Fixture that sets up several current servers, but no new ones.
//
class MultiServerTest : public MemcachedStoreViewTest
{
  void SetUp()
  {
    MemcachedConfig cfg;
    cfg.servers.push_back("localhost:40001");
    cfg.servers.push_back("localhost:40002");
    cfg.servers.push_back("localhost:40003");
    _view.update(cfg);
  }
};

TEST_F(MultiServerTest, TwoCurrentReplicas)
{
  std::map<int, MemcachedStoreView::ReplicaList> replicas = _view.current_replicas();
  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    EXPECT_REPLICA_LIST(replicas[i], 2u, AnyOf("localhost:40001",
                                               "localhost:40002",
                                               "localhost:40003"));
  }
}

TEST_F(MultiServerTest, NoNewReplicas)
{
  EXPECT_TRUE(_view.new_replicas().empty());
}

TEST_F(MultiServerTest, MovesEmpty)
{
  EXPECT_TRUE(_view.calculate_vbucket_moves().empty());
}

//
// Fixture that sets up several current servers, and one new one.
//
class MultiNewServerTest : public MemcachedStoreViewTest
{
  void SetUp()
  {
    MemcachedConfig cfg;
    cfg.servers.push_back("localhost:40001");
    cfg.servers.push_back("localhost:40002");
    cfg.new_servers.push_back("localhost:40003");
    _view.update(cfg);
  }
};

TEST_F(MultiNewServerTest, TwoCurrentReplicas)
{
  std::map<int, MemcachedStoreView::ReplicaList> replicas = _view.current_replicas();
  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    EXPECT_REPLICA_LIST(replicas[i], 2u, AnyOf("localhost:40001",
                                               "localhost:40002",
                                               "localhost:40003"));
  }
}

//
// Fixture that sets up several current servers and several new ones.
//
class ScaleUpTest : public MemcachedStoreViewTest
{
  void SetUp()
  {
    MemcachedConfig cfg;
    cfg.servers.push_back("localhost:40001");
    cfg.servers.push_back("localhost:40002");
    cfg.new_servers.push_back("localhost:40001");
    cfg.new_servers.push_back("localhost:40002");
    cfg.new_servers.push_back("localhost:40003");
    _view.update(cfg);
  }
};

TEST_F(ScaleUpTest, CurrentReplicasDontHaveNewServer)
{
  std::map<int, MemcachedStoreView::ReplicaList> replicas = _view.current_replicas();
  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    EXPECT_REPLICA_LIST(replicas[i], 2u, AnyOf("localhost:40001", "localhost:40002"));
  }
}

TEST_F(ScaleUpTest, NewReplicasFilledIn)
{
  std::map<int, MemcachedStoreView::ReplicaList> replicas = _view.new_replicas();
  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    EXPECT_REPLICA_LIST(replicas[i], 2u, AnyOf("localhost:40001",
                                               "localhost:40002",
                                               "localhost:40003"));
  }
}

TEST_F(ScaleUpTest, MovesNotEmpty)
{
  EXPECT_FALSE(_view.calculate_vbucket_moves().empty());
}

TEST_F(ScaleUpTest, MovesAgreeWithReplicaLists)
{
  std::map<int, MemcachedStoreView::ReplicaList> curr_replicas = _view.current_replicas();
  std::map<int, MemcachedStoreView::ReplicaList> new_replicas = _view.new_replicas();
  std::map<int, MemcachedStoreView::ReplicaChange> moves = _view.calculate_vbucket_moves();

  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    if (moves.find(i) != moves.end())
    {
      EXPECT_EQ(moves[i].first, curr_replicas[i]);
      EXPECT_EQ(moves[i].second, new_replicas[i]);
    }
    else
    {
      MemcachedStoreView::ReplicaList old_sorted = curr_replicas[i];
      MemcachedStoreView::ReplicaList new_sorted = new_replicas[i];
      std::sort(old_sorted.begin(), old_sorted.end());
      std::sort(new_sorted.begin(), new_sorted.end());
      EXPECT_EQ(old_sorted, new_sorted);
    }
  }
}

TEST_F(ScaleUpTest, WriteListContainsAllCurrents)
{
  std::map<int, MemcachedStoreView::ReplicaList> curr_replicas = _view.current_replicas();

  for (int i = 0; i < NUM_VBUCKETS; ++i)
  {
    std::vector<std::string> write_list = _view.write_replicas(i);
    EXPECT_THAT(write_list, Contains(curr_replicas[i][0]));
    EXPECT_THAT(write_list, Contains(curr_replicas[i][1]));
  }
}
